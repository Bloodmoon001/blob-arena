#pragma once

#include "client/NetworkClient.h"
#include "client/Interpolator.h"
#include "client/Renderer.h"
#include "common/Config.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace blob {

    class ClientGame {
    public:
        ClientGame(const std::string& serverHost,
            std::uint16_t serverPort,
            const std::string& playerName);
        ~ClientGame() = default;

        ClientGame(const ClientGame&) = delete;
        ClientGame& operator=(const ClientGame&) = delete;

        bool Run();

    private:
        void Update(float dt);
        void Render();

        void SendInputIfNeeded(float dt);
        Vector2 ComputeMoveDirection() const;

        NetworkClient network_;
        Interpolator interpolator_;
        Renderer renderer_;

        std::string playerName_;
        bool joined_ = false;
        bool quit_ = false;

        float inputTimer_ = 0.0f;
        float elapsedTime_ = 0.0f;

        Vector2 cameraPos_;
        bool cameraInitialized_ = false;

        // FPS
        float fpsTimer_ = 0.0f;
        int   fpsFrames_ = 0;
        int   currentFps_ = 0;
    };

} // namespace blob