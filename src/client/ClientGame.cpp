#include "client/ClientGame.h"

#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cstdio>
#include <iostream>

namespace blob {

    namespace {
        const sf::Color kBgColor(20, 25, 35);
        const sf::Color kGridColor(35, 40, 55);
        const sf::Color kFoodColor(120, 220, 120);
        const sf::Color kLocalPlayerColor(90, 180, 250);
        const sf::Color kRemotePlayerColor(230, 100, 100);
        const sf::Color kTextColor(230, 230, 240);
        const sf::Color kUiBg(15, 15, 25, 200);
        const sf::Color kBorderColor(60, 60, 90);
        const sf::Color kLeaderColor(255, 215, 0, 220);
    }

    ClientGame::ClientGame(const std::string& serverHost,
        std::uint16_t serverPort,
        const std::string& playerName)
        : renderer_(1280, 720),
        playerName_(playerName)
    {
        if (!network_.Connect(serverHost, serverPort, playerName)) {
            throw std::runtime_error("Failed to connect to server");
        }
    }

    bool ClientGame::Run() {
        using clock = std::chrono::steady_clock;
        auto lastTime = clock::now();

        while (renderer_.isOpen() && !quit_) {
            auto now = clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            if (dt > 0.1f) dt = 0.1f;
            elapsedTime_ += dt;

            renderer_.pollEvents();

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
                quit_ = true;
                break;
            }

            if (!network_.Update(dt)) {
                std::cerr << "[Client] Lost connection\n";
                break;
            }

            Update(dt);
            Render();
            renderer_.display();
        }

        return true;
    }

    void ClientGame::Update(float dt) {
        // FPS
        fpsTimer_ += dt;
        ++fpsFrames_;
        if (fpsTimer_ >= 1.0f) {
            currentFps_ = fpsFrames_;
            fpsFrames_ = 0;
            fpsTimer_ = 0.0f;
        }

        if (!joined_) {
            if (network_.state() == ConnectionState::Connected) {
                joined_ = true;
                std::cout << "[Client] Joined game as player "
                    << network_.localPlayerId() << "\n";
            }
            else if (network_.state() == ConnectionState::Rejected) {
                std::cerr << "[Client] Rejected by server\n";
                quit_ = true;
                return;
            }
        }

        if (!joined_) return;

        Vector2 dir = ComputeMoveDirection();
        network_.PredictMove(dir, dt);

        inputTimer_ += dt;
        if (inputTimer_ >= config::kInputInterval) {
            inputTimer_ = 0.0f;
            network_.SendInput(dir, config::kInputInterval);
        }

        interpolator_.PushSnapshot(network_.players(),
            network_.serverTick(),
            elapsedTime_);
    }

    Vector2 ClientGame::ComputeMoveDirection() const {
        Vector2 mouseScreen = renderer_.mousePosition();
        Vector2 mouseWorld = renderer_.screenToWorld(mouseScreen);

        const PlayerSnapshot* local = network_.localPlayer();
        Vector2 playerPos = local ? local->position : Vector2{ 0, 0 };

        Vector2 toMouse = mouseWorld - playerPos;
        float len = toMouse.length();
        if (len < 5.0f) return { 0.0f, 0.0f };
        return toMouse.normalized();
    }

    void ClientGame::Render() {
        renderer_.clear(kBgColor);

        // Экран подключения (5.4)
        if (!joined_) {
            std::string status;
            sf::Color color = kTextColor;

            switch (network_.state()) {
            case ConnectionState::Disconnected:
                status = "Disconnected";
                color = sf::Color(240, 100, 100);
                break;
            case ConnectionState::Connecting:
                status = "Connecting to server...";
                color = sf::Color(255, 220, 100);
                break;
            case ConnectionState::Rejected:
                status = "Connection rejected by server";
                color = sf::Color(240, 100, 100);
                break;
            default:
                status = "Connecting...";
                break;
            }

            renderer_.drawText(status,
                renderer_.width() * 0.35f,
                renderer_.height() * 0.5f,
                28, color);
            return;
        }

        // Камера следит за локальным игроком.
        const PlayerSnapshot* local = network_.localPlayer();
        if (local) {
            Vector2 target = local->position;
            if (!cameraInitialized_) {
                cameraPos_ = target;
                cameraInitialized_ = true;
            }
            else {
                cameraPos_ += (target - cameraPos_) * 0.15f;
            }
            renderer_.setCamera(cameraPos_);
        }

        // Сетка мира.
        for (float x = 0.0f; x <= config::kWorldWidth; x += 200.0f) {
            Vector2 a = renderer_.worldToScreen({ x, 0.0f });
            Vector2 b = renderer_.worldToScreen({ x, config::kWorldHeight });
            renderer_.drawLine(a, b, kGridColor);
        }
        for (float y = 0.0f; y <= config::kWorldHeight; y += 200.0f) {
            Vector2 a = renderer_.worldToScreen({ 0.0f, y });
            Vector2 b = renderer_.worldToScreen({ config::kWorldWidth, y });
            renderer_.drawLine(a, b, kGridColor);
        }

        // Границы мира.
        Vector2 tl = renderer_.worldToScreen({ 0.0f, 0.0f });
        Vector2 br = renderer_.worldToScreen({ config::kWorldWidth, config::kWorldHeight });
        renderer_.drawLine(tl, { br.x, tl.y }, kBorderColor, 3.0f);
        renderer_.drawLine({ br.x, tl.y }, br, kBorderColor, 3.0f);
        renderer_.drawLine(br, { tl.x, br.y }, kBorderColor, 3.0f);
        renderer_.drawLine({ tl.x, br.y }, tl, kBorderColor, 3.0f);

        // Еда.
        for (const auto& f : network_.food()) {
            Vector2 p = renderer_.worldToScreen(f.position);
            renderer_.drawCircle(p, config::kFoodRadius, kFoodColor);
        }

        // Находим лидера (5.3).
        config::EntityId leaderId = config::kInvalidEntityId;
        float leaderRadius = 0.0f;
        for (const auto& p : network_.players()) {
            if (p.radius > leaderRadius) {
                leaderRadius = p.radius;
                leaderId = p.id;
            }
        }

        // Другие игроки.
        for (const auto& p : network_.players()) {
            if (p.id == network_.localPlayerId()) continue;

            Vector2 interpPos = p.position;
            float interpRadius = p.radius;
            if (!interpolator_.GetInterpolated(p.id, interpPos, interpRadius)) {
                interpPos = p.position;
                interpRadius = p.radius;
            }

            Vector2 screen = renderer_.worldToScreen(interpPos);
            renderer_.drawCircle(screen, interpRadius, kRemotePlayerColor);

            if (p.id == leaderId) {
                renderer_.drawCircleOutline(screen, interpRadius + 3.0f,
                    kLeaderColor, 3.0f);
            }
        }

        // Локальный игрок.
        if (local) {
            Vector2 predicted = local->position;
            float predictedRadius = local->radius;

            Vector2 screen = renderer_.worldToScreen(predicted);
            renderer_.drawCircle(screen, predictedRadius, kLocalPlayerColor);
            renderer_.drawCircleOutline(screen, predictedRadius + 2.0f,
                sf::Color(255, 255, 255, 200), 2.0f);

            if (local->id == leaderId) {
                renderer_.drawCircleOutline(screen, predictedRadius + 5.0f,
                    kLeaderColor, 3.0f);
            }
        }

        // UI.
        renderer_.drawRect(10.0f, 10.0f, 260.0f, 170.0f, kUiBg);

        std::string nameLine = "Name: " + playerName_;
        std::string idLine = "Player ID: " + std::to_string(network_.localPlayerId());
        std::string rttLine = "RTT: " + std::to_string(network_.rttMs()) + " ms";
        std::string tickLine = "Server tick: " + std::to_string(network_.serverTick());
        std::string snapLine = "Snapshots: " + std::to_string(network_.snapshotsReceived());

        renderer_.drawText(nameLine, 20.0f, 20.0f, 14, kTextColor);
        renderer_.drawText(idLine, 20.0f, 40.0f, 14, kTextColor);
        renderer_.drawText(rttLine, 20.0f, 60.0f, 14, kTextColor);
        renderer_.drawText(tickLine, 20.0f, 80.0f, 14, kTextColor);
        renderer_.drawText(snapLine, 20.0f, 100.0f, 14, kTextColor);

        // Size/Mass (5.5)
        if (local) {
            char sizeLine[64];
            std::snprintf(sizeLine, sizeof(sizeLine),
                "Size: %.1f  Mass: %.1f",
                local->radius, local->radius * local->radius * 0.01f);
            renderer_.drawText(sizeLine, 20.0f, 120.0f, 14, kTextColor);
        }

        // Players online (5.5)
        std::string onlineLine = "Players online: " +
            std::to_string(network_.players().size());
        renderer_.drawText(onlineLine, 20.0f, 140.0f, 14, kTextColor);

        // FPS (5.6)
        std::string fpsLine = "FPS: " + std::to_string(currentFps_);
        renderer_.drawText(fpsLine,
            renderer_.width() - 90.0f, 20.0f,
            14, sf::Color(180, 180, 200));

        // Подсказка.
        renderer_.drawText("Move mouse to steer. ESC to quit.",
            20.0f, renderer_.height() - 30.0f, 14,
            sf::Color(180, 180, 200));
    }

} // namespace blob