#include "common/Protocol.h"
#include <cstring>
#include <stdexcept>
#include <string>
namespace blob {

    void ByteWriter::writeU8(std::uint8_t v) {
        buf_.push_back(v);
    }

    void ByteWriter::writeU16(std::uint16_t v) {
        buf_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        buf_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    }

    void ByteWriter::writeU32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            buf_.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    void ByteWriter::writeF32(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        writeU32(bits);
    }

    void ByteWriter::writeBytes(const void* data, std::size_t size) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        buf_.insert(buf_.end(), p, p + size);
    }

    std::uint8_t ByteReader::readU8() {
        if (!canRead(1)) { ok_ = false; return 0; }
        return data_[pos_++];
    }

    std::uint16_t ByteReader::readU16() {
        if (!canRead(2)) { ok_ = false; return 0; }
        std::uint16_t v = static_cast<std::uint16_t>(data_[pos_])
            | (static_cast<std::uint16_t>(data_[pos_ + 1]) << 8);
        pos_ += 2;
        return v;
    }

    std::uint32_t ByteReader::readU32() {
        if (!canRead(4)) { ok_ = false; return 0; }
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= static_cast<std::uint32_t>(data_[pos_ + i]) << (i * 8);
        }
        pos_ += 4;
        return v;
    }

    float ByteReader::readF32() {
        std::uint32_t bits = readU32();
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

    void ByteReader::readBytes(void* out, std::size_t n) {
        if (!canRead(n)) { ok_ = false; return; }
        std::memcpy(out, data_ + pos_, n);
        pos_ += n;
    }

    namespace {

        void WriteHeader(ByteWriter& w, PacketType type) {
            w.writeU32(config::kProtocolMagic);
            w.writeU16(config::kProtocolVersion);
            w.writeU8(static_cast<std::uint8_t>(type));
        }

        void WriteVector2(ByteWriter& w, const Vector2& v) {
            w.writeF32(v.x);
            w.writeF32(v.y);
        }

        Vector2 ReadVector2(ByteReader& r) {
            float x = r.readF32();
            float y = r.readF32();
            return { x, y };
        }

        void WriteString(ByteWriter& w, const std::string& s) {
            w.writeU16(static_cast<std::uint16_t>(s.size()));
            w.writeBytes(s.data(), s.size());
        }

        std::string ReadString(ByteReader& r) {
            std::uint16_t len = r.readU16();
            if (!r.ok() || len > 1024) return {};
            std::string s(len, '\0');
            r.readBytes(s.data(), len);
            return s;
        }

    } // namespace

    std::vector<std::uint8_t> PackJoinRequest(const std::string& playerName) {
        ByteWriter w;
        WriteHeader(w, PacketType::JoinRequest);
        WriteString(w, playerName);
        return w.buffer();
    }

    std::vector<std::uint8_t> PackJoinAccepted(config::EntityId assignedId) {
        ByteWriter w;
        WriteHeader(w, PacketType::JoinAccepted);
        w.writeU32(assignedId);
        return w.buffer();
    }

    std::vector<std::uint8_t> PackJoinRejected(const std::string& reason) {
        ByteWriter w;
        WriteHeader(w, PacketType::JoinRejected);
        WriteString(w, reason);
        return w.buffer();
    }

    std::vector<std::uint8_t> PackInput(const InputCommand& cmd) {
        ByteWriter w;
        WriteHeader(w, PacketType::Input);
        w.writeU32(cmd.sequence);
        w.writeF32(cmd.moveX);
        w.writeF32(cmd.moveY);
        w.writeF32(cmd.dt);
        return w.buffer();
    }

    std::vector<std::uint8_t> PackSnapshot(const WorldSnapshot& s) {
        ByteWriter w;
        WriteHeader(w, PacketType::StateSnapshot);
        w.writeU32(s.tick);
        w.writeU32(s.lastProcessedInput);

        w.writeU16(static_cast<std::uint16_t>(s.players.size()));
        for (const auto& p : s.players) {
            w.writeU32(p.id);
            WriteVector2(w, p.position);
            w.writeF32(p.radius);
        }

        w.writeU16(static_cast<std::uint16_t>(s.food.size()));
        for (const auto& f : s.food) {
            w.writeU32(f.id);
            WriteVector2(w, f.position);
        }

        return w.buffer();
    }

    std::vector<std::uint8_t> PackPing(std::uint32_t clientTimeMs) {
        ByteWriter w;
        WriteHeader(w, PacketType::Ping);
        w.writeU32(clientTimeMs);
        return w.buffer();
    }

    std::vector<std::uint8_t> PackPong(std::uint32_t clientTimeMs) {
        ByteWriter w;
        WriteHeader(w, PacketType::Pong);
        w.writeU32(clientTimeMs);
        return w.buffer();
    }


    PacketHeader ReadHeader(const std::uint8_t* data, std::size_t size) {
        PacketHeader h;
        if (size < 7) return h;

        ByteReader r(data, size);
        h.magic = r.readU32();
        h.version = r.readU16();
        h.type = static_cast<PacketType>(r.readU8());

        h.valid = r.ok() &&
            h.magic == config::kProtocolMagic &&
            h.version == config::kProtocolVersion;

        return h;
    }

    namespace {
        // После ReadHeader указатель сдвигается на 7 байт.
        constexpr std::size_t kHeaderSize = 7;
    }

    bool UnpackJoinRequest(const std::uint8_t* data, std::size_t size, JoinRequestData& out) {
        if (size < kHeaderSize) return false;
        ByteReader r(data + kHeaderSize, size - kHeaderSize);
        out.playerName = ReadString(r);
        return r.ok();
    }

    bool UnpackJoinAccepted(const std::uint8_t* data, std::size_t size, JoinAcceptedData& out) {
        if (size < kHeaderSize + 4) return false;
        ByteReader r(data + kHeaderSize, size - kHeaderSize);
        out.assignedId = r.readU32();
        return r.ok();
    }

    bool UnpackInput(const std::uint8_t* data, std::size_t size, InputCommand& out) {
        if (size < kHeaderSize + 16) return false;
        ByteReader r(data + kHeaderSize, size - kHeaderSize);
        out.sequence = r.readU32();
        out.moveX = r.readF32();
        out.moveY = r.readF32();
        out.dt = r.readF32();
        return r.ok();
    }

    bool UnpackSnapshot(const std::uint8_t* data, std::size_t size, WorldSnapshot& out) {
        if (size < kHeaderSize + 10) return false;

        ByteReader r(data + kHeaderSize, size - kHeaderSize);

        out.tick = r.readU32();
        out.lastProcessedInput = r.readU32();

        std::uint16_t playerCount = r.readU16();
        if (!r.ok() || playerCount > config::kMaxPlayers) return false;

        out.players.clear();
        out.players.reserve(playerCount);
        for (std::uint16_t i = 0; i < playerCount; ++i) {
            PlayerSnapshot p;
            p.id = r.readU32();
            p.position = ReadVector2(r);
            p.radius = r.readF32();
            if (!r.ok()) return false;
            out.players.push_back(p);
        }

        std::uint16_t foodCount = r.readU16();
        if (!r.ok() || foodCount > 10000) return false;

        out.food.clear();
        out.food.reserve(foodCount);
        for (std::uint16_t i = 0; i < foodCount; ++i) {
            FoodSnapshot f;
            f.id = r.readU32();
            f.position = ReadVector2(r);
            if (!r.ok()) return false;
            out.food.push_back(f);
        }

        return true;
    }

} // namespace blob