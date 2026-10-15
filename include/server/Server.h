#pragma once

#include "server/GameState.h"
#include "common/Protocol.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace blob {

    class Server {
    public:
        explicit Server(std::uint16_t port);
        ~Server();

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        void Run();
        void Stop();

        std::size_t clientCount() const noexcept { return clients_.size(); }
        std::uint32_t currentTick() const noexcept { return gameState_.currentTick(); }

        static void InstallSignalHandler(Server* server);

    private:
        struct ClientInfo {
            sockaddr_in address{};
            std::chrono::steady_clock::time_point lastSeen;
            std::string name;
            config::EntityId playerId = config::kInvalidEntityId;
        };

        void ProcessIncomingPackets();
        void ProcessPacket(const std::uint8_t* data, std::size_t size,
            const sockaddr_in& from);
        void HandleJoinRequest(const std::uint8_t* data, std::size_t size,
            const sockaddr_in& from);
        void HandleInput(const std::uint8_t* data, std::size_t size,
            const sockaddr_in& from);

        void BroadcastSnapshot();
        void SendTo(const sockaddr_in& addr,
            const std::vector<std::uint8_t>& packet);

        void KickTimedOutClients();

        static std::uint64_t AddressKey(const sockaddr_in& addr);
        static std::string AddressToString(const sockaddr_in& addr);

        std::uint16_t port_;
        SOCKET socket_ = INVALID_SOCKET;
        std::atomic<bool> running_{ false };

        GameState gameState_;
        std::unordered_map<std::uint64_t, ClientInfo> clients_;

        std::chrono::steady_clock::time_point lastTickTime_;
    };

} // namespace blob