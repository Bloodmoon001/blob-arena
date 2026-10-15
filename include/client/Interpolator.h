#pragma once

#include "common/Protocol.h"
#include "common/Vector2.h"
#include "common/Config.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace blob {

    class Interpolator {
    public:
        Interpolator() = default;

        // Сохранить снапшот для интерполяции.
        void PushSnapshot(const std::vector<PlayerSnapshot>& players,
            std::uint32_t serverTick,
            float currentTimeSec);

        bool GetInterpolated(config::EntityId id, Vector2& outPos, float& outRadius) const;

        // Очистить данные игрока, который отключился.
        void RemovePlayer(config::EntityId id);

    private:
        struct Sample {
            float time = 0.0f;
            Vector2 position;
            float radius = 0.0f;
        };

        // Для каждого игрока — небольшая очередь последних сэмплов.
        struct PlayerHistory {
            std::array<Sample, config::kSnapshotBufferSize> samples{};
            std::size_t count = 0;
            std::size_t head = 0;
        };

        void PushSample(config::EntityId id, float time,
            const Vector2& pos, float radius);

        std::unordered_map<config::EntityId, PlayerHistory> history_;

        float bufferTimeSec_ = config::kInterpolationDelay;
    };

} // namespace blob