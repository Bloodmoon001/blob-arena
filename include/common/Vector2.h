#pragma once

#include <cmath>
#include <cstdint>

namespace blob {

    struct Vector2 {
        float x = 0.0f;
        float y = 0.0f;

        Vector2() = default;
        Vector2(float xv, float yv) : x(xv), y(yv) {}

        Vector2 operator+(const Vector2& o) const { return { x + o.x, y + o.y }; }
        Vector2 operator-(const Vector2& o) const { return { x - o.x, y - o.y }; }
        Vector2 operator*(float s) const { return { x * s, y * s }; }
        Vector2 operator/(float s) const { return { x / s, y / s }; }

        Vector2& operator+=(const Vector2& o) { x += o.x; y += o.y; return *this; }
        Vector2& operator-=(const Vector2& o) { x -= o.x; y -= o.y; return *this; }
        Vector2& operator*=(float s) { x *= s; y *= s; return *this; }

        bool operator==(const Vector2& o) const { return x == o.x && y == o.y; }
        bool operator!=(const Vector2& o) const { return !(*this == o); }

        float lengthSquared() const { return x * x + y * y; }
        float length() const { return std::sqrt(lengthSquared()); }

        Vector2 normalized() const {
            float len = length();
            if (len < 1e-6f) return { 0.0f, 0.0f };
            return { x / len, y / len };
        }

        Vector2 clampedLength(float maxLen) const {
            float lenSq = lengthSquared();
            if (lenSq <= maxLen * maxLen) return *this;
            float len = std::sqrt(lenSq);
            return { x / len * maxLen, y / len * maxLen };
        }

        static float distance(const Vector2& a, const Vector2& b) {
            return (b - a).length();
        }

        static float distanceSquared(const Vector2& a, const Vector2& b) {
            return (b - a).lengthSquared();
        }
    };

} // namespace blob