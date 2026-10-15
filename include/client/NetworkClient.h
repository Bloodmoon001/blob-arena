#pragma once

#include "common/Protocol.h"
#include "common/Vector2.h"
#include "common/Config.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace blob {

    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Rejected
    };

    class NetworkClient {
    public:
        NetworkClient();
        ~NetworkClient();

        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        // Подключиться к серверу. Возвращает true, если JoinRequest отправлен.
        bool Connect(const std::string& host, std::uint16_t port,
            const std::string& playerName);

        // Обновить сеть: отправить input, принять входящие пакеты.
        // Возвращает false, если соединение потеряно.
        bool Update(float dt);

        // Отправить текущий input серверу.
        void SendInput(const Vector2& direction, float dt);

        // Локальная prediction: двигаем localPlayer_ сразу.
        void PredictMove(const Vector2& direction, float dt);

        // --- Геттеры ---
        ConnectionState state() const noexcept { return state_; }
        config::EntityId localPlayerId() const noexcept { return localPlayerId_; }
        const PlayerSnapshot* localPlayer() const noexcept;
        const std::vector<PlayerSnapshot>& players() const noexcept { return players_; }
        const std::vector<FoodSnapshot>& food()    const noexcept { return food_; }

        // RTT в миллисекундах
        std::uint32_t rttMs() const noexcept { return rttMs_; }

        // Общее число обработанных снапшотов
        std::uint32_t snapshotsReceived() const noexcept { return snapshotsReceived_; }

        // Серверные тики (для отладки)
        std::uint32_t serverTick() const noexcept { return serverTick_; }

    private:
        struct PendingInput {
            InputCommand cmd;
            Vector2 predictedPosition;
        };

        void ProcessIncomingPackets();
        void HandlePacket(const std::uint8_t* data, std::size_t size);
        void HandleJoinAccepted(const std::uint8_t* data, std::size_t size);
        void HandleJoinRejected(const std::uint8_t* data, std::size_t size);
        void HandleSnapshot(const std::uint8_t* data, std::size_t size);
        void HandlePong(const std::uint8_t* data, std::size_t size);

        void SendToServer(const std::vector<std::uint8_t>& packet);
        void SendPingIfNeeded();
        void ReconcileWithServer(const PlayerSnapshot& serverPlayer);

        // Сеть
        SOCKET socket_ = INVALID_SOCKET;
        sockaddr_in serverAddr_{};
        bool winsockReady_ = false;

        ConnectionState state_ = ConnectionState::Disconnected;
        config::EntityId localPlayerId_ = config::kInvalidEntityId;
        std::string playerName_;

        // Игровые данные из последнего снапшота
        std::vector<PlayerSnapshot> players_;
        std::vector<FoodSnapshot>   food_;
        std::uint32_t serverTick_ = 0;
        std::uint32_t snapshotsReceived_ = 0;

        // Prediction и reconciliation
        std::uint32_t nextInputSequence_ = 1;
        std::deque<PendingInput> pendingInputs_;
        Vector2 predictedLocalPos_;
        float   predictedRadius_ = config::kPlayerStartRadius;

        // Отправка input — накапливаем время
        float inputTimer_ = 0.0f;

        // Ping для измерения RTT
        float pingTimer_ = 0.0f;
        std::uint32_t lastPingSentMs_ = 0;
        std::uint32_t rttMs_ = 0;

        // Общее время работы, для sequence
        std::uint32_t elapsedMs_ = 0;
    };

} // namespace blob