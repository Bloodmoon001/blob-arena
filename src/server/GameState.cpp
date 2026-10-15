#include "server/GameState.h"

#include <algorithm>
#include <cmath>

namespace blob {

    GameState::GameState()
        : rng_(std::random_device{}())
    {
        for (int i = 0; i < config::kFoodCount; ++i) {
            SpawnFood();
        }
    }

    config::EntityId GameState::AddPlayer(const std::string& name) {
        if (players_.size() >= config::kMaxPlayers) {
            return config::kInvalidEntityId;
        }

        config::EntityId id = nextPlayerId_++;

        Player p;
        p.id = id;
        p.name = name.empty() ? ("Player" + std::to_string(id)) : name;
        p.radius = config::kPlayerStartRadius;
        p.lastProcessedInput = 0;

        // Случайная стартовая позиция с отступом от краёв.
        std::uniform_real_distribution<float> dx(p.radius, config::kWorldWidth - p.radius);
        std::uniform_real_distribution<float> dy(p.radius, config::kWorldHeight - p.radius);
        p.position = { dx(rng_), dy(rng_) };

        players_[id] = std::move(p);
        return id;
    }

    bool GameState::RemovePlayer(config::EntityId id) {
        return players_.erase(id) > 0;
    }

    bool GameState::HasPlayer(config::EntityId id) const {
        return players_.find(id) != players_.end();
    }

    void GameState::ApplyInput(config::EntityId playerId, const InputCommand& cmd) {
        auto it = players_.find(playerId);
        if (it == players_.end()) return;

        Player& p = it->second;

        // Устаревшие команды игнорируем.
        if (cmd.sequence < p.lastProcessedInput) return;
        p.lastProcessedInput = cmd.sequence;

        // Скорость обратно пропорциональна размеру.
        float sizeFactor = config::kPlayerStartRadius / p.radius;
        float speed = config::kPlayerBaseSpeed * std::clamp(sizeFactor, 0.3f, 1.0f);

        Vector2 dir{ cmd.moveX, cmd.moveY };
        if (dir.lengthSquared() > 1.0f) {
            dir = dir.normalized();
        }

        p.position += dir * speed * cmd.dt;
        ClampToWorld(p);
    }

    void GameState::Update(float dt) {
        (void)dt;
        CheckFoodCollisions();
        CheckPlayerCollisions();
        ++tick_;
    }

    void GameState::CheckFoodCollisions() {
        // Для каждого игрока ищем съеденную еду.
        for (auto& [id, p] : players_) {
            (void)id;
            for (auto it = food_.begin(); it != food_.end(); ) {
                float d2 = Vector2::distanceSquared(p.position, it->position);
                float eatDist = p.radius + config::kFoodRadius * 0.5f;
                if (d2 <= eatDist * eatDist) {
                    p.radius += config::kFoodMassGain;
                    if (p.radius > config::kPlayerMaxRadius) {
                        p.radius = config::kPlayerMaxRadius;
                    }
                    // Спавним новую еду взамен съеденной.
                    std::uniform_real_distribution<float> dx(config::kFoodRadius, config::kWorldWidth - config::kFoodRadius);
                    std::uniform_real_distribution<float> dy(config::kFoodRadius, config::kWorldHeight - config::kFoodRadius);
                    it->position = { dx(rng_), dy(rng_) };
                    it->id = nextFoodId_++;
                    ++it;
                }
                else {
                    ++it;
                }
            }
        }
    }

    void GameState::CheckPlayerCollisions() {
        // Собираем список id, кого съели, чтобы удалить после.
        std::vector<config::EntityId> eaten;

        for (auto& [idA, a] : players_) {
            if (std::find(eaten.begin(), eaten.end(), idA) != eaten.end()) continue;

            for (auto& [idB, b] : players_) {
                if (idA == idB) continue;
                if (std::find(eaten.begin(), eaten.end(), idB) != eaten.end()) continue;

                float d2 = Vector2::distanceSquared(a.position, b.position);
                float eatDist = a.radius;

                if (d2 <= eatDist * eatDist) {
                    if (a.radius >= b.radius * config::kEatRadiusRatio) {
                        a.radius = std::min(
                            a.radius + b.radius * config::kEatMassTransfer,
                            config::kPlayerMaxRadius);
                        eaten.push_back(idB);
                    }
                }
            }
        }

        for (auto id : eaten) {
            players_.erase(id);
        }
    }

    void GameState::ClampToWorld(Player& p) const {
        p.position.x = std::clamp(p.position.x, p.radius, config::kWorldWidth - p.radius);
        p.position.y = std::clamp(p.position.y, p.radius, config::kWorldHeight - p.radius);
    }

    void GameState::SpawnFood() {
        std::uniform_real_distribution<float> dx(config::kFoodRadius, config::kWorldWidth - config::kFoodRadius);
        std::uniform_real_distribution<float> dy(config::kFoodRadius, config::kWorldHeight - config::kFoodRadius);

        Food f;
        f.id = nextFoodId_++;
        f.position = { dx(rng_), dy(rng_) };
        food_.push_back(f);
    }

    WorldSnapshot GameState::ToSnapshot() const {
        WorldSnapshot s;
        s.tick = tick_;

        s.players.reserve(players_.size());
        for (const auto& [id, p] : players_) {
            PlayerSnapshot ps;
            ps.id = p.id;
            ps.position = p.position;
            ps.radius = p.radius;
            s.players.push_back(ps);
        }

        s.food.reserve(food_.size());
        for (const auto& f : food_) {
            FoodSnapshot fs;
            fs.id = f.id;
            fs.position = f.position;
            s.food.push_back(fs);
        }

        return s;
    }

    const Player* GameState::GetPlayer(config::EntityId id) const {
        auto it = players_.find(id);
        if (it == players_.end()) return nullptr;
        return &it->second;
    }

} // namespace blob