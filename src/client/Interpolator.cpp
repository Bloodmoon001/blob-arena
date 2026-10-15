#include "client/Interpolator.h"

#include <algorithm>

namespace blob {

    void Interpolator::PushSnapshot(const std::vector<PlayerSnapshot>& players,
        std::uint32_t serverTick,
        float currentTimeSec)
    {
        (void)serverTick;

        float sampleTime = currentTimeSec - bufferTimeSec_;

        for (const auto& p : players) {
            PushSample(p.id, sampleTime, p.position, p.radius);
        }
    }

    void Interpolator::PushSample(config::EntityId id, float time,
        const Vector2& pos, float radius)
    {
        auto& h = history_[id];

        Sample s;
        s.time = time;
        s.position = pos;
        s.radius = radius;

        if (h.count < h.samples.size()) {
            h.samples[h.count++] = s;
        }
        else {
            // Кольцевой буфер: перезаписываем самый старый.
            h.samples[h.head] = s;
            h.head = (h.head + 1) % h.samples.size();
        }
    }

    bool Interpolator::GetInterpolated(config::EntityId id,
        Vector2& outPos, float& outRadius) const
    {
        auto it = history_.find(id);
        if (it == history_.end()) return false;

        const auto& h = it->second;
        if (h.count == 0) return false;
        if (h.count == 1) {
            // Только один сэмпл — отдаём как есть.
            outPos = h.samples[0].position;
            outRadius = h.samples[0].radius;
            return true;
        }

        // Достаём два последних сэмпла в хронологическом порядке.
        Sample a, b;
        if (h.count < h.samples.size()) {
            a = h.samples[h.count - 2];
            b = h.samples[h.count - 1];
        }
        else {
            // В кольцевом буфере head указывает на самый старый.
            std::size_t secondLast = (h.head + h.count - 2) % h.count;
            std::size_t last = (h.head + h.count - 1) % h.count;
            a = h.samples[secondLast];
            b = h.samples[last];
        }

        // Линейная интерполяция.
        float span = b.time - a.time;
        if (span <= 1e-6f) {
            outPos = b.position;
            outRadius = b.radius;
            return true;
        }

        float alpha = 0.5f;

        outPos.x = a.position.x + (b.position.x - a.position.x) * alpha;
        outPos.y = a.position.y + (b.position.y - a.position.y) * alpha;
        outRadius = a.radius + (b.radius - a.radius) * alpha;

        return true;
    }

    void Interpolator::RemovePlayer(config::EntityId id) {
        history_.erase(id);
    }

} // namespace blob