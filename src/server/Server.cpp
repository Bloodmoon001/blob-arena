#include "server/Server.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace blob {

    namespace {
        Server* g_signalServer = nullptr;

        BOOL WINAPI ConsoleHandler(DWORD type) {
            if (type == CTRL_C_EVENT || type == CTRL_CLOSE_EVENT ||
                type == CTRL_BREAK_EVENT) {
                if (g_signalServer) g_signalServer->Stop();
                return TRUE;
            }
            return FALSE;
        }

        bool InitWinsock() {
            static bool initialized = false;
            if (initialized) return true;

            WSADATA data;
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                std::cerr << "[Server] WSAStartup failed\n";
                return false;
            }
            initialized = true;
            return true;
        }
    }

    Server::Server(std::uint16_t port)
        : port_(port)
    {
        if (!InitWinsock()) {
            throw std::runtime_error("Winsock init failed");
        }

        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            ::closesocket(socket_);
            throw std::runtime_error("bind() failed on port " + std::to_string(port_) +
                ": " + std::to_string(WSAGetLastError()));
        }

        u_long nonBlocking = 1;
        ::ioctlsocket(socket_, FIONBIO, &nonBlocking);

        lastTickTime_ = std::chrono::steady_clock::now();
        std::cout << "[Server] Listening on UDP port " << port_ << "\n";
    }

    Server::~Server() {
        Stop();
        if (socket_ != INVALID_SOCKET) {
            ::closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        if (g_signalServer == this) g_signalServer = nullptr;
    }

    void Server::InstallSignalHandler(Server* server) {
        g_signalServer = server;
        SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    }

    void Server::Run() {
        running_ = true;

        while (running_) {
            ProcessIncomingPackets();

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastTickTime_).count();

            if (dt >= config::kTickInterval) {
                lastTickTime_ = now;
                gameState_.Update(dt);
                BroadcastSnapshot();
                KickTimedOutClients();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << "[Server] Stopped\n";
    }

    void Server::Stop() {
        running_ = false;
    }

    void Server::ProcessIncomingPackets() {
        std::uint8_t buffer[config::kMaxPacketSize];

        for (int i = 0; i < 64; ++i) {
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
                std::cerr << "[Server] recvfrom error: " << err << "\n";
                return;
            }

            if (received > 0) {
                ProcessPacket(buffer, static_cast<std::size_t>(received), from);
            }
        }
    }

    void Server::ProcessPacket(const std::uint8_t* data, std::size_t size,
        const sockaddr_in& from)
    {
        auto header = ReadHeader(data, size);
        if (!header.valid) return;

        switch (header.type) {
        case PacketType::JoinRequest:
            HandleJoinRequest(data, size, from);
            break;
        case PacketType::Input:
            HandleInput(data, size, from);
            break;
        case PacketType::Ping: {
            auto key = AddressKey(from);
            auto it = clients_.find(key);
            if (it != clients_.end()) {
                it->second.lastSeen = std::chrono::steady_clock::now();
            }
            ByteReader r(data + 7, size - 7);
            std::uint32_t clientTime = r.readU32();
            if (r.ok()) {
                SendTo(from, PackPong(clientTime));
            }
            break;
        }
        default:
            break;
        }
    }

    void Server::HandleJoinRequest(const std::uint8_t* data, std::size_t size,
        const sockaddr_in& from)
    {
        auto key = AddressKey(from);

        auto it = clients_.find(key);
        if (it != clients_.end()) {
            it->second.lastSeen = std::chrono::steady_clock::now();
            SendTo(from, PackJoinAccepted(it->second.playerId));
            return;
        }

        if (clients_.size() >= static_cast<std::size_t>(config::kMaxPlayers)) {
            SendTo(from, PackJoinRejected("Server full"));
            return;
        }

        JoinRequestData req;
        if (!UnpackJoinRequest(data, size, req)) {
            SendTo(from, PackJoinRejected("Bad join request"));
            return;
        }

        config::EntityId id = gameState_.AddPlayer(req.playerName);
        if (id == config::kInvalidEntityId) {
            SendTo(from, PackJoinRejected("Server full"));
            return;
        }

        ClientInfo info;
        info.address = from;
        info.lastSeen = std::chrono::steady_clock::now();
        info.name = req.playerName;
        info.playerId = id;
        clients_[key] = info;

        SendTo(from, PackJoinAccepted(id));

        std::cout << "[Server] Player '" << (req.playerName.empty() ? "?" : req.playerName)
            << "' joined from " << AddressToString(from)
            << " (id=" << id << ", total=" << clients_.size() << ")\n";
    }

    void Server::HandleInput(const std::uint8_t* data, std::size_t size,
        const sockaddr_in& from)
    {
        auto key = AddressKey(from);
        auto it = clients_.find(key);
        if (it == clients_.end()) return;

        it->second.lastSeen = std::chrono::steady_clock::now();

        InputCommand cmd;
        if (!UnpackInput(data, size, cmd)) return;

        gameState_.ApplyInput(it->second.playerId, cmd);
    }

    void Server::BroadcastSnapshot() {
        if (clients_.empty()) return;

        auto snapshot = gameState_.ToSnapshot();
        auto packet = PackSnapshot(snapshot);

        for (const auto& [key, info] : clients_) {
            (void)key;
            SendTo(info.address, packet);
        }
    }

    void Server::SendTo(const sockaddr_in& addr,
        const std::vector<std::uint8_t>& packet)
    {
        ::sendto(socket_,
            reinterpret_cast<const char*>(packet.data()),
            static_cast<int>(packet.size()),
            0,
            reinterpret_cast<const sockaddr*>(&addr),
            sizeof(addr));
    }

    void Server::KickTimedOutClients() {
        auto now = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(config::kClientTimeoutMs);

        for (auto it = clients_.begin(); it != clients_.end(); ) {
            if (now - it->second.lastSeen > timeout) {
                std::cout << "[Server] Client timed out: " << it->second.name << "\n";
                gameState_.RemovePlayer(it->second.playerId);
                it = clients_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    std::uint64_t Server::AddressKey(const sockaddr_in& addr) {
        return (static_cast<std::uint64_t>(addr.sin_addr.s_addr) << 16)
            | static_cast<std::uint64_t>(addr.sin_port);
    }

    std::string Server::AddressToString(const sockaddr_in& addr) {
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
        return std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
    }

} // namespace blob