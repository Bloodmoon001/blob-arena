#pragma once

#include <cstdint>

namespace blob {

    namespace config {

        // Мир
        constexpr float kWorldWidth = 2000.0f;
        constexpr float kWorldHeight = 2000.0f;

        // Игрок
        constexpr float kPlayerStartRadius = 30.0f;
        constexpr float kPlayerMinRadius = 20.0f;
        constexpr float kPlayerMaxRadius = 200.0f;
        constexpr float kPlayerBaseSpeed = 200.0f;
        constexpr float kPlayerMassDecay = 0.5f;

        // Еда
        constexpr int   kFoodCount = 200;
        constexpr float kFoodRadius = 8.0f;
        constexpr float kFoodMassGain = 1.0f;

        // Поедание
        constexpr float kEatRadiusRatio = 1.2f;
        constexpr float kEatMassTransfer = 0.8f;

        // Сеть
        constexpr int   kMaxPlayers = 8;
        constexpr int   kTickRate = 20;
        constexpr float kTickInterval = 1.0f / static_cast<float>(kTickRate);

        constexpr int   kServerPort = 27015;
        constexpr int   kMaxPacketSize = 8192;
        constexpr int   kClientTimeoutMs = 5000;

        // Input rate клиента (отправка команд)
        constexpr int   kInputRate = 30;
        constexpr float kInputInterval = 1.0f / static_cast<float>(kInputRate);

        // Интерполяция
        constexpr float kInterpolationDelay = 0.1f;
        constexpr int   kSnapshotBufferSize = 16;

        // Протокол
        constexpr std::uint32_t kProtocolMagic = 0x424C4F42; // "BLOB"
        constexpr std::uint16_t kProtocolVersion = 1;

        // IDs
        using EntityId = std::uint32_t;
        constexpr EntityId kInvalidEntityId = 0;

    } // namespace config

} // namespace blob