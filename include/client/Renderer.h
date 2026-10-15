#pragma once

#include "common/Vector2.h"
#include "common/Protocol.h"
#include "common/Config.h"

#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>

namespace blob {

    class Renderer {
    public:
        Renderer(unsigned int width, unsigned int height);
        ~Renderer() = default;

        sf::RenderWindow& window() noexcept { return window_; }
        bool isOpen() const noexcept { return window_.isOpen(); }

        void pollEvents();
        void clear(const sf::Color& color);
        void display();

        // Мировые координаты -> экранные (с учётом камеры).
        Vector2 worldToScreen(const Vector2& world) const;
        Vector2 screenToWorld(const Vector2& screen) const;

        // Центр камеры на игроке.
        void setCamera(const Vector2& worldPos) { camera_ = worldPos; }

        // Примитивы рисования.
        void drawCircle(const Vector2& screenPos, float radius, const sf::Color& fill);
        void drawCircleOutline(const Vector2& screenPos, float radius,
            const sf::Color& color, float thickness);
        void drawRect(float x, float y, float w, float h, const sf::Color& color);
        void drawText(const std::string& text, float x, float y,
            unsigned int size, const sf::Color& color);
        void drawLine(const Vector2& a, const Vector2& b,
            const sf::Color& color, float thickness = 1.0f);

        Vector2 mousePosition() const;

        float width()  const noexcept { return static_cast<float>(window_.getSize().x); }
        float height() const noexcept { return static_cast<float>(window_.getSize().y); }

    private:
        sf::RenderWindow window_;
        sf::Font font_;
        bool fontLoaded_ = false;
        Vector2 camera_;
    };

} // namespace blob