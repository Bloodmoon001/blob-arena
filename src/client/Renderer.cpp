#include "client/Renderer.h"
#include <iostream>

namespace blob {

    Renderer::Renderer(unsigned int width, unsigned int height)
        : window_(sf::VideoMode({ width, height }),
            "Blob Arena",
            sf::Style::Titlebar | sf::Style::Close)
    {
        window_.setFramerateLimit(60);

        const char* paths[] = {
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/calibri.ttf"
        };
        for (const char* p : paths) {
            if (font_.openFromFile(p)) {
                fontLoaded_ = true;
                break;
            }
        }
        if (!fontLoaded_) {
            std::cerr << "[Renderer] Could not load system font\n";
        }
    }

    void Renderer::pollEvents() {
        while (const std::optional event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window_.close();
            }
        }
    }

    void Renderer::clear(const sf::Color& color) {
        window_.clear(color);
    }

    void Renderer::display() {
        window_.display();
    }

    Vector2 Renderer::worldToScreen(const Vector2& world) const {
        float halfW = width() * 0.5f;
        float halfH = height() * 0.5f;
        return { world.x - camera_.x + halfW,
                world.y - camera_.y + halfH };
    }

    Vector2 Renderer::screenToWorld(const Vector2& screen) const {
        float halfW = width() * 0.5f;
        float halfH = height() * 0.5f;
        return { screen.x - halfW + camera_.x,
                screen.y - halfH + camera_.y };
    }

    void Renderer::drawCircle(const Vector2& screenPos, float radius,
        const sf::Color& fill)
    {
        sf::CircleShape circle(radius);
        circle.setPosition({ screenPos.x - radius, screenPos.y - radius });
        circle.setFillColor(fill);
        circle.setOutlineThickness(0.0f);
        window_.draw(circle);
    }

    void Renderer::drawCircleOutline(const Vector2& screenPos, float radius,
        const sf::Color& color, float thickness)
    {
        sf::CircleShape circle(radius);
        circle.setPosition({ screenPos.x - radius, screenPos.y - radius });
        circle.setFillColor(sf::Color::Transparent);
        circle.setOutlineColor(color);
        circle.setOutlineThickness(thickness);
        window_.draw(circle);
    }

    void Renderer::drawRect(float x, float y, float w, float h,
        const sf::Color& color)
    {
        sf::RectangleShape rect({ w, h });
        rect.setPosition({ x, y });
        rect.setFillColor(color);
        window_.draw(rect);
    }

    void Renderer::drawText(const std::string& text, float x, float y,
        unsigned int size, const sf::Color& color)
    {
        if (!fontLoaded_) return;

        sf::Text t(font_, text, size);
        t.setPosition({ x, y });
        t.setFillColor(color);
        window_.draw(t);
    }

    void Renderer::drawLine(const Vector2& a, const Vector2& b,
        const sf::Color& color, float thickness)
    {
        (void)thickness;
        sf::Vertex line[2];
        line[0].position = sf::Vector2f{ a.x, a.y };
        line[0].color = color;
        line[1].position = sf::Vector2f{ b.x, b.y };
        line[1].color = color;
        window_.draw(line, 2, sf::PrimitiveType::Lines);
    }

    Vector2 Renderer::mousePosition() const {
        sf::Vector2i p = sf::Mouse::getPosition(window_);
        return Vector2(static_cast<float>(p.x), static_cast<float>(p.y));
    }

} // namespace blob