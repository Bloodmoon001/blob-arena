#pragma once

#include "common/Vector2.h"
#include "common/Config.h"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
namespace blob {

    // Все типы пакетов, которые могут ходить по сети.
    enum class PacketType : std::uint8_t {
        JoinRequest = 1,   // клиент -> сервер: хочу подключиться
        JoinAccepted = 2,   // сервер -> клиент: добро пожаловать
        JoinRejected = 3,   // сервер -> клиент: отказ (сервер полон)

        Input = 10,  // клиент -> сервер: куда двигаться
        StateSnapshot = 20,  // сервер -> клиенты: текущее состояние мира

        PlayerLeft = 30,  // сервер -> клиенты: игрок отключился
        Ping = 40,
        Pong = 41,
    };

    // Команда ввода от игрока.
    struct InputCommand {
        std::uint32_t sequence = 0;   // порядковый номер команды
        float moveX = 0.0f;           // нормализованное направление
        float moveY = 0.0f;
        float dt = 0.0f;              // время, за которое применяется команда
    };

    // Снимок одного игрока.
    struct PlayerSnapshot {
        config::EntityId id = config::kInvalidEntityId;
        Vector2 position;
        float radius = 0.0f;
    };

    // Снимок одной еды.
    struct FoodSnapshot {
        config::EntityId id = config::kInvalidEntityId;
        Vector2 position;
    };

    // Полный снимок мира, отправляется сервером.
    struct WorldSnapshot {
        std::uint32_t tick = 0;
        std::uint32_t lastProcessedInput = 0;   // для reconciliation клиента
        std::vector<PlayerSnapshot> players;
        std::vector<FoodSnapshot> food;
    };

    class ByteWriter {
    public:
        void writeU8(std::uint8_t v);
        void writeU16(std::uint16_t v);
        void writeU32(std::uint32_t v);
        void writeF32(float v);
        void writeBytes(const void* data, std::size_t size);

        const std::vector<std::uint8_t>& buffer() const noexcept { return buf_; }
        std::size_t size() const noexcept { return buf_.size(); }

    private:
        std::vector<std::uint8_t> buf_;
    };

    class ByteReader {
    public:
        ByteReader(const std::uint8_t* data, std::size_t size)
            : data_(data), size_(size) {
        }

        bool canRead(std::size_t n) const noexcept { return pos_ + n <= size_; }

        std::uint8_t  readU8();
        std::uint16_t readU16();
        std::uint32_t readU32();
        float         readF32();
        void          readBytes(void* out, std::size_t n);

        bool ok() const noexcept { return ok_; }
        std::size_t pos() const noexcept { return pos_; }

    private:
        const std::uint8_t* data_;
        std::size_t size_;
        std::size_t pos_ = 0;
        bool ok_ = true;
    };


    std::vector<std::uint8_t> PackJoinRequest(const std::string& playerName);
    std::vector<std::uint8_t> PackJoinAccepted(config::EntityId assignedId);
    std::vector<std::uint8_t> PackJoinRejected(const std::string& reason);
    std::vector<std::uint8_t> PackInput(const InputCommand& cmd);
    std::vector<std::uint8_t> PackSnapshot(const WorldSnapshot& snapshot);
    std::vector<std::uint8_t> PackPing(std::uint32_t clientTimeMs);
    std::vector<std::uint8_t> PackPong(std::uint32_t clientTimeMs);

    struct PacketHeader {
        std::uint32_t magic = 0;
        std::uint16_t version = 0;
        PacketType type = PacketType::Ping;
        bool valid = false;
    };

    PacketHeader ReadHeader(const std::uint8_t* data, std::size_t size);

    struct JoinRequestData {
        std::string playerName;
    };

    struct JoinAcceptedData {
        config::EntityId assignedId = config::kInvalidEntityId;
    };

    bool UnpackJoinRequest(const std::uint8_t* data, std::size_t size, JoinRequestData& out);
    bool UnpackJoinAccepted(const std::uint8_t* data, std::size_t size, JoinAcceptedData& out);
    bool UnpackInput(const std::uint8_t* data, std::size_t size, InputCommand& out);
    bool UnpackSnapshot(const std::uint8_t* data, std::size_t size, WorldSnapshot& out);

} // namespace blob