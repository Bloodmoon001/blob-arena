#include "client/NetworkClient.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace blob {

    namespace {
        bool InitWinsockClient() {
            static bool initialized = false;
            if (initialized) return true;

            WSADATA data;
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                return false;
            }
            initialized = true;
            return true;
        }
    }

    NetworkClient::NetworkClient() {
        winsockReady_ = InitWinsockClient();
    }

    NetworkClient::~NetworkClient() {
        if (socket_ != INVALID_SOCKET) {
            ::closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
    }

    bool NetworkClient::Connect(const std::string& host, std::uint16_t port,
        const std::string& playerName)
    {
        if (!winsockReady_) return false;

        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            std::cerr << "[Client] socket() failed: " << WSAGetLastError() << "\n";
            return false;
        }

        serverAddr_.sin_family = AF_INET;
        serverAddr_.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &serverAddr_.sin_addr) != 1) {
            std::cerr << "[Client] inet_pton failed for " << host << "\n";
            return false;
        }

        // Неблокирующий режим.
        u_long nonBlocking = 1;
        ::ioctlsocket(socket_, FIONBIO, &nonBlocking);

        playerName_ = playerName;
        state_ = ConnectionState::Connecting;

        SendToServer(PackJoinRequest(playerName));
        std::cout << "[Client] JoinRequest sent to " << host << ":" << port << "\n";
        return true;
    }

    bool NetworkClient::Update(float dt) {
        if (state_ == ConnectionState::Disconnected ||
            state_ == ConnectionState::Rejected) {
            return state_ != ConnectionState::Rejected;
        }

        elapsedMs_ += static_cast<std::uint32_t>(dt * 1000.0f);

        ProcessIncomingPackets();
        SendPingIfNeeded();

        return true;
    }

    void NetworkClient::ProcessIncomingPackets() {
        std::uint8_t buffer[config::kMaxPacketSize];

        for (int i = 0; i < 32; ++i) {
            sockaddr_in from{};
            int fromLen = sizeof(from);

            int received = ::recvfrom(socket_,
                reinterpret_cast<char*>(buffer),
                sizeof(buffer),
                0,
                reinterpret_cast<sockaddr*>(&from),
                &fromLen);

            if (received == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) return;
                std::cerr << "[Client] recvfrom error: " << err << "\n";
                return;
            }

            if (received > 0) {
                HandlePacket(buffer, static_cast<std::size_t>(received));
            }
        }
    }

    void NetworkClient::HandlePacket(const std::uint8_t* data, std::size_t size) {
        auto header = ReadHeader(data, size);
        if (!header.valid) return;

        switch (header.type) {
        case PacketType::JoinAccepted:
            HandleJoinAccepted(data, size);
            break;
        case PacketType::JoinRejected:
            HandleJoinRejected(data, size);
            break;
        case PacketType::StateSnapshot:
            HandleSnapshot(data, size);
            break;
        case PacketType::Pong:
            HandlePong(data, size);
            break;
        default:
            break;
        }
    }

    void NetworkClient::HandleJoinAccepted(const std::uint8_t* data, std::size_t size) {
        JoinAcceptedData info;
        if (!UnpackJoinAccepted(data, size, info)) return;

        localPlayerId_ = info.assignedId;
        state_ = ConnectionState::Connected;

        std::cout << "[Client] Joined as player id=" << localPlayerId_ << "\n";
    }

    void NetworkClient::HandleJoinRejected(const std::uint8_t* data, std::size_t size) {
        (void)data; (void)size;
        state_ = ConnectionState::Rejected;
        std::cerr << "[Client] Connection rejected by server\n";
    }

    void NetworkClient::HandleSnapshot(const std::uint8_t* data, std::size_t size) {
        WorldSnapshot snap;
        if (!UnpackSnapshot(data, size, snap)) return;

        players_ = std::move(snap.players);
        food_ = std::move(snap.food);
        serverTick_ = snap.tick;
        ++snapshotsReceived_;

        // Reconciliation: если мы локальный игрок, сравниваем с сервером.
        for (const auto& p : players_) {
            if (p.id == localPlayerId_) {
                ReconcileWithServer(p);
                break;
            }
        }
    }

    void NetworkClient::HandlePong(const std::uint8_t* data, std::size_t size) {
        if (size < 7 + 4) return;
        ByteReader r(data + 7, size - 7);
        std::uint32_t sentTime = r.readU32();
        if (!r.ok()) return;

        std::uint32_t now = elapsedMs_;
        if (now >= sentTime) {
            rttMs_ = now - sentTime;
        }
    }

    void NetworkClient::SendToServer(const std::vector<std::uint8_t>& packet) {
        if (socket_ == INVALID_SOCKET) return;
        ::sendto(socket_,
            reinterpret_cast<const char*>(packet.data()),
            static_cast<int>(packet.size()),
            0,
            reinterpret_cast<const sockaddr*>(&serverAddr_),
            sizeof(serverAddr_));
    }

    void NetworkClient::SendPingIfNeeded() {
        pingTimer_ += 0.1f;
        if (pingTimer_ < 0.5f) return;
        pingTimer_ = 0.0f;

        lastPingSentMs_ = elapsedMs_;
        SendToServer(PackPing(lastPingSentMs_));
    }

    void NetworkClient::SendInput(const Vector2& direction, float dt) {
        if (state_ != ConnectionState::Connected) return;

        InputCommand cmd;
        cmd.sequence = nextInputSequence_++;
        cmd.moveX = direction.x;
        cmd.moveY = direction.y;
        cmd.dt = dt;

        // Сохраняем для reconciliation.
        PendingInput pi;
        pi.cmd = cmd;
        pi.predictedPosition = predictedLocalPos_;
        pendingInputs_.push_back(pi);

        // Ограничиваем историю — если сервер долго не отвечает,
        // не держим бесконечный буфер.
        while (pendingInputs_.size() > 120) {
            pendingInputs_.pop_front();
        }

        SendToServer(PackInput(cmd));
    }

    void NetworkClient::PredictMove(const Vector2& direction, float dt) {
        if (state_ != ConnectionState::Connected) return;

        float sizeFactor = config::kPlayerStartRadius / predictedRadius_;
        float speed = config::kPlayerBaseSpeed * std::clamp(sizeFactor, 0.3f, 1.0f);

        Vector2 dir = direction;
        if (dir.lengthSquared() > 1.0f) {
            dir = dir.normalized();
        }

        predictedLocalPos_ += dir * speed * dt;

        // Клэмпим в мир.
        predictedLocalPos_.x = std::clamp(predictedLocalPos_.x,
            predictedRadius_,
            config::kWorldWidth - predictedRadius_);
        predictedLocalPos_.y = std::clamp(predictedLocalPos_.y,
            predictedRadius_,
            config::kWorldHeight - predictedRadius_);
    }

    void NetworkClient::ReconcileWithServer(const PlayerSnapshot& serverPlayer) {
        // Разница между серверной и локальной позицией.
        Vector2 delta = predictedLocalPos_ - serverPlayer.position;
        float distSq = delta.lengthSquared();

        // Если расхождение мало (< 5 px), ничего не делаем — prediction верен.
        if (distSq < 5.0f * 5.0f) {
            predictedRadius_ = serverPlayer.radius;
            return;
        }

        // Иначе — сбрасываем на серверное состояние и переигрываем pending inputs.
        predictedLocalPos_ = serverPlayer.position;
        predictedRadius_ = serverPlayer.radius;

        for (auto& pi : pendingInputs_) {
            float sizeFactor = config::kPlayerStartRadius / predictedRadius_;
            float speed = config::kPlayerBaseSpeed * std::clamp(sizeFactor, 0.3f, 1.0f);
            Vector2 dir{ pi.cmd.moveX, pi.cmd.moveY };
            if (dir.lengthSquared() > 1.0f) dir = dir.normalized();
            predictedLocalPos_ += dir * speed * pi.cmd.dt;
        }

        predictedLocalPos_.x = std::clamp(predictedLocalPos_.x,
            predictedRadius_,
            config::kWorldWidth - predictedRadius_);
        predictedLocalPos_.y = std::clamp(predictedLocalPos_.y,
            predictedRadius_,
            config::kWorldHeight - predictedRadius_);
    }

    const PlayerSnapshot* NetworkClient::localPlayer() const noexcept {
        for (const auto& p : players_) {
            if (p.id == localPlayerId_) return &p;
        }
        return nullptr;
    }

} // namespace blob