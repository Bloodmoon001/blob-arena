#pragma once

#include "common/Vector2.h"
#include "common/Config.h"
#include "common/Protocol.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <cstdint>

namespace blob {

    struct Player {
        config::EntityId id = config::kInvalidEntityId;
        std::string name;
        Vector2 position;
        float radius = config::kPlayerStartRadius;
        std::uint32_t lastProcessedInput = 0;
    };

    class GameState {
    public:
        GameState();

        config::EntityId AddPlayer(const std::string& name);
        bool RemovePlayer(config::EntityId id);
        bool HasPlayer(config::EntityId id) const;
        std::size_t playerCount() const noexcept { return players_.size(); }
        std::size_t maxPlayers() const noexcept { return config::kMaxPlayers; }

        // Применить команду ввода клиента к игроку.
        // Сервер применяет движение сразу, не ждя tick.
        void ApplyInput(config::EntityId playerId, const InputCommand& cmd);

        // Симуляция: обновляет еду, столкновения, тик.
        void Update(float dt);

        WorldSnapshot ToSnapshot() const;

        std::uint32_t currentTick() const noexcept { return tick_; }

        const Player* GetPlayer(config::EntityId id) const;
        std::size_t foodCount() const noexcept { return food_.size(); }

    private:
        struct Food {
            config::EntityId id = config::kInvalidEntityId;
            Vector2 position;
        };

        void SpawnFood();
        void CheckFoodCollisions();
        void CheckPlayerCollisions();
        void ClampToWorld(Player& p) const;

        std::unordered_map<config::EntityId, Player> players_;
        std::vector<Food> food_;

        std::uint32_t tick_ = 0;
        config::EntityId nextPlayerId_ = 1;
        config::EntityId nextFoodId_ = 10000;

        std::mt19937 rng_;
    };

} // namespace blob