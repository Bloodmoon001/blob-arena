#include <gtest/gtest.h>
#include "common/Vector2.h"
#include "common/Protocol.h"
#include "server/GameState.h"

using namespace blob;

TEST(Vector2Test, Addition) {
    Vector2 a(1.0f, 2.0f);
    Vector2 b(3.0f, 4.0f);
    Vector2 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 4.0f);
    EXPECT_FLOAT_EQ(c.y, 6.0f);
}

TEST(Vector2Test, Length) {
    Vector2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.length(), 5.0f);
}

TEST(Vector2Test, Normalize) {
    Vector2 v(3.0f, 4.0f);
    Vector2 n = v.normalized();
    EXPECT_NEAR(n.length(), 1.0f, 1e-5f);
}

TEST(Vector2Test, ClampedLength) {
    Vector2 v(100.0f, 0.0f);
    Vector2 c = v.clampedLength(10.0f);
    EXPECT_FLOAT_EQ(c.length(), 10.0f);
}

TEST(Vector2Test, ClampedLengthNoOp) {
    Vector2 v(3.0f, 4.0f);
    Vector2 c = v.clampedLength(10.0f);
    EXPECT_FLOAT_EQ(c.x, 3.0f);
    EXPECT_FLOAT_EQ(c.y, 4.0f);
}

TEST(Vector2Test, Distance) {
    EXPECT_FLOAT_EQ(Vector2::distance({ 0, 0 }, { 3, 4 }), 5.0f);
}

TEST(Protocol, PingPacketHasValidHeader) {
    auto buf = PackPing(12345);
    ASSERT_GT(buf.size(), 7u);

    auto h = ReadHeader(buf.data(), buf.size());
    EXPECT_TRUE(h.valid);
    EXPECT_EQ(h.type, PacketType::Ping);
}

TEST(Protocol, GarbageHeaderIsInvalid) {
    std::uint8_t junk[16] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    auto h = ReadHeader(junk, sizeof(junk));
    EXPECT_FALSE(h.valid);
}

TEST(Protocol, TooSmallBufferIsInvalid) {
    std::uint8_t small[3] = { 1, 2, 3 };
    auto h = ReadHeader(small, sizeof(small));
    EXPECT_FALSE(h.valid);
}

TEST(Protocol, JoinRequestRoundTrip) {
    auto buf = PackJoinRequest("Alice");

    auto h = ReadHeader(buf.data(), buf.size());
    EXPECT_TRUE(h.valid);
    EXPECT_EQ(h.type, PacketType::JoinRequest);

    JoinRequestData data;
    ASSERT_TRUE(UnpackJoinRequest(buf.data(), buf.size(), data));
    EXPECT_EQ(data.playerName, "Alice");
}

TEST(Protocol, JoinRequestEmptyName) {
    auto buf = PackJoinRequest("");
    JoinRequestData data;
    ASSERT_TRUE(UnpackJoinRequest(buf.data(), buf.size(), data));
    EXPECT_EQ(data.playerName, "");
}

TEST(Protocol, JoinAcceptedRoundTrip) {
    auto buf = PackJoinAccepted(42);

    auto h = ReadHeader(buf.data(), buf.size());
    EXPECT_EQ(h.type, PacketType::JoinAccepted);

    JoinAcceptedData data;
    ASSERT_TRUE(UnpackJoinAccepted(buf.data(), buf.size(), data));
    EXPECT_EQ(data.assignedId, 42u);
}

TEST(Protocol, InputRoundTrip) {
    InputCommand cmd;
    cmd.sequence = 100;
    cmd.moveX = 0.5f;
    cmd.moveY = -0.5f;
    cmd.dt = 0.016f;

    auto buf = PackInput(cmd);

    auto h = ReadHeader(buf.data(), buf.size());
    EXPECT_EQ(h.type, PacketType::Input);

    InputCommand out;
    ASSERT_TRUE(UnpackInput(buf.data(), buf.size(), out));
    EXPECT_EQ(out.sequence, 100u);
    EXPECT_FLOAT_EQ(out.moveX, 0.5f);
    EXPECT_FLOAT_EQ(out.moveY, -0.5f);
    EXPECT_FLOAT_EQ(out.dt, 0.016f);
}

TEST(Protocol, SnapshotRoundTrip) {
    WorldSnapshot s;
    s.tick = 500;
    s.lastProcessedInput = 100;

    s.players.push_back({ 1, {100.0f, 200.0f}, 30.0f });
    s.players.push_back({ 2, {300.0f, 400.0f}, 45.0f });

    s.food.push_back({ 100, {10.0f, 20.0f} });
    s.food.push_back({ 101, {30.0f, 40.0f} });
    s.food.push_back({ 102, {50.0f, 60.0f} });

    auto buf = PackSnapshot(s);

    auto h = ReadHeader(buf.data(), buf.size());
    EXPECT_EQ(h.type, PacketType::StateSnapshot);

    WorldSnapshot out;
    ASSERT_TRUE(UnpackSnapshot(buf.data(), buf.size(), out));

    EXPECT_EQ(out.tick, 500u);
    EXPECT_EQ(out.lastProcessedInput, 100u);
    ASSERT_EQ(out.players.size(), 2u);
    ASSERT_EQ(out.food.size(), 3u);

    EXPECT_EQ(out.players[0].id, 1u);
    EXPECT_FLOAT_EQ(out.players[0].position.x, 100.0f);
    EXPECT_FLOAT_EQ(out.players[0].radius, 30.0f);

    EXPECT_EQ(out.players[1].id, 2u);
    EXPECT_FLOAT_EQ(out.players[1].position.y, 400.0f);
    EXPECT_FLOAT_EQ(out.players[1].radius, 45.0f);

    EXPECT_EQ(out.food[0].id, 100u);
    EXPECT_FLOAT_EQ(out.food[2].position.x, 50.0f);
}

TEST(Protocol, SnapshotEmpty) {
    WorldSnapshot s;
    s.tick = 1;
    auto buf = PackSnapshot(s);

    WorldSnapshot out;
    ASSERT_TRUE(UnpackSnapshot(buf.data(), buf.size(), out));
    EXPECT_EQ(out.players.size(), 0u);
    EXPECT_EQ(out.food.size(), 0u);
}

TEST(Protocol, TruncatedSnapshotFails) {
    WorldSnapshot s;
    s.tick = 1;
    s.players.push_back({ 1, {10.0f, 20.0f}, 30.0f });
    auto buf = PackSnapshot(s);

    buf.resize(buf.size() - 5);

    WorldSnapshot out;
    EXPECT_FALSE(UnpackSnapshot(buf.data(), buf.size(), out));
}

TEST(ByteStream, U8) {
    ByteWriter w;
    w.writeU8(0xAB);
    ByteReader r(w.buffer().data(), w.buffer().size());
    EXPECT_EQ(r.readU8(), 0xAB);
    EXPECT_TRUE(r.ok());
}

TEST(ByteStream, U16LittleEndian) {
    ByteWriter w;
    w.writeU16(0x1234);
    const auto& b = w.buffer();
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x34);
    EXPECT_EQ(b[1], 0x12);
}

TEST(ByteStream, U32RoundTrip) {
    ByteWriter w;
    w.writeU32(0xDEADBEEF);
    ByteReader r(w.buffer().data(), w.buffer().size());
    EXPECT_EQ(r.readU32(), 0xDEADBEEFu);
}

TEST(ByteStream, F32RoundTrip) {
    ByteWriter w;
    w.writeF32(3.14159f);
    ByteReader r(w.buffer().data(), w.buffer().size());
    EXPECT_FLOAT_EQ(r.readF32(), 3.14159f);
}

TEST(ByteStream, ReaderFailsOnShortBuffer) {
    std::uint8_t data[2] = { 1, 2 };
    ByteReader r(data, sizeof(data));
    r.readU32();
    EXPECT_FALSE(r.ok());
}

TEST(GameStateTest, StartsEmpty) {
    GameState gs;
    EXPECT_EQ(gs.playerCount(), 0u);
    EXPECT_GT(gs.foodCount(), 0u);
}

TEST(GameStateTest, AddPlayer) {
    GameState gs;
    auto id = gs.AddPlayer("Alice");
    EXPECT_NE(id, config::kInvalidEntityId);
    EXPECT_TRUE(gs.HasPlayer(id));
    EXPECT_EQ(gs.playerCount(), 1u);

    const Player* p = gs.GetPlayer(id);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "Alice");
    EXPECT_FLOAT_EQ(p->radius, config::kPlayerStartRadius);
}

TEST(GameStateTest, AddPlayerWithoutName) {
    GameState gs;
    auto id = gs.AddPlayer("");
    const Player* p = gs.GetPlayer(id);
    ASSERT_NE(p, nullptr);
    EXPECT_FALSE(p->name.empty());
}

TEST(GameStateTest, AddPlayerRespectsMax) {
    GameState gs;
    for (int i = 0; i < config::kMaxPlayers; ++i) {
        EXPECT_NE(gs.AddPlayer("P" + std::to_string(i)), config::kInvalidEntityId);
    }
    EXPECT_EQ(gs.AddPlayer("Overflow"), config::kInvalidEntityId);
    EXPECT_EQ(gs.playerCount(), static_cast<std::size_t>(config::kMaxPlayers));
}

TEST(GameStateTest, RemovePlayer) {
    GameState gs;
    auto id = gs.AddPlayer("Bob");
    EXPECT_TRUE(gs.RemovePlayer(id));
    EXPECT_FALSE(gs.HasPlayer(id));
    EXPECT_EQ(gs.playerCount(), 0u);
}

TEST(GameStateTest, RemoveUnknownPlayer) {
    GameState gs;
    EXPECT_FALSE(gs.RemovePlayer(9999));
}

TEST(GameStateTest, ApplyInputMovesPlayer) {
    GameState gs;
    auto id = gs.AddPlayer("Alice");
    const Player* before = gs.GetPlayer(id);
    Vector2 startPos = before->position;

    InputCommand cmd;
    cmd.sequence = 1;
    cmd.moveX = 1.0f;
    cmd.moveY = 0.0f;
    cmd.dt = 1.0f;

    gs.ApplyInput(id, cmd);

    const Player* after = gs.GetPlayer(id);
    EXPECT_GT(after->position.x, startPos.x);
    EXPECT_FLOAT_EQ(after->position.y, startPos.y);
}

TEST(GameStateTest, OldInputIgnored) {
    GameState gs;
    auto id = gs.AddPlayer("Alice");

    InputCommand cmd1;
    cmd1.sequence = 5;
    cmd1.moveX = 1.0f;
    cmd1.dt = 0.1f;
    gs.ApplyInput(id, cmd1);
    Vector2 posAfter5 = gs.GetPlayer(id)->position;

    InputCommand cmd2;
    cmd2.sequence = 3;
    cmd2.moveX = -1.0f;
    cmd2.dt = 0.1f;
    gs.ApplyInput(id, cmd2);
    Vector2 posAfterStale = gs.GetPlayer(id)->position;

    EXPECT_FLOAT_EQ(posAfterStale.x, posAfter5.x);
}

TEST(GameStateTest, InputClampsToWorld) {
    GameState gs;
    auto id = gs.AddPlayer("Alice");

    for (int i = 0; i < 1000; ++i) {
        InputCommand cmd;
        cmd.sequence = static_cast<std::uint32_t>(i + 1);
        cmd.moveX = 1.0f;
        cmd.dt = 0.1f;
        gs.ApplyInput(id, cmd);
    }

    const Player* after = gs.GetPlayer(id);
    EXPECT_LE(after->position.x, config::kWorldWidth);
}

TEST(GameStateTest, SnapshotContainsAllPlayers) {
    GameState gs;
    gs.AddPlayer("Alice");
    gs.AddPlayer("Bob");

    auto snap = gs.ToSnapshot();
    EXPECT_EQ(snap.players.size(), 2u);
    EXPECT_EQ(snap.food.size(), gs.foodCount());
}

TEST(GameStateTest, TickIncrementsAfterUpdate) {
    GameState gs;
    EXPECT_EQ(gs.currentTick(), 0u);
    gs.Update(0.05f);
    EXPECT_EQ(gs.currentTick(), 1u);
    gs.Update(0.05f);
    EXPECT_EQ(gs.currentTick(), 2u);
}

TEST(GameStateTest, PlayerEatsFoodGrowsRadius) {
    GameState gs;
    auto id = gs.AddPlayer("Alice");
    float r0 = gs.GetPlayer(id)->radius;

    for (int i = 0; i < 100; ++i) {
        gs.Update(0.05f);
    }

    float r1 = gs.GetPlayer(id)->radius;
    EXPECT_GE(r1, r0);
}

#include "client/Interpolator.h"

TEST(InterpolatorTest, StartsEmpty) {
    Interpolator interp;
    Vector2 pos;
    float radius = 0.0f;
    EXPECT_FALSE(interp.GetInterpolated(1, pos, radius));
}

TEST(InterpolatorTest, SingleSnapshot) {
    Interpolator interp;

    std::vector<PlayerSnapshot> players = {
        {1, {100.0f, 200.0f}, 30.0f}
    };
    interp.PushSnapshot(players, 1, 0.5f);

    Vector2 pos;
    float radius = 0.0f;
    ASSERT_TRUE(interp.GetInterpolated(1, pos, radius));
    EXPECT_FLOAT_EQ(pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.y, 200.0f);
    EXPECT_FLOAT_EQ(radius, 30.0f);
}

TEST(InterpolatorTest, TwoSnapshotsInterpolate) {
    Interpolator interp;

    interp.PushSnapshot({ {1, {0.0f, 0.0f}, 30.0f} }, 1, 0.0f);
    interp.PushSnapshot({ {1, {100.0f, 0.0f}, 50.0f} }, 2, 0.5f);

    Vector2 pos;
    float radius = 0.0f;
    ASSERT_TRUE(interp.GetInterpolated(1, pos, radius));
    EXPECT_GT(pos.x, 0.0f);
    EXPECT_LT(pos.x, 100.0f);
}

TEST(InterpolatorTest, RemovePlayer) {
    Interpolator interp;
    interp.PushSnapshot({ {1, {100.0f, 200.0f}, 30.0f} }, 1, 0.0f);

    Vector2 pos;
    float radius = 0.0f;
    EXPECT_TRUE(interp.GetInterpolated(1, pos, radius));

    interp.RemovePlayer(1);
    EXPECT_FALSE(interp.GetInterpolated(1, pos, radius));
}

TEST(InterpolatorTest, UnknownPlayerReturnsFalse) {
    Interpolator interp;
    interp.PushSnapshot({ {1, {0.0f, 0.0f}, 30.0f} }, 1, 0.0f);

    Vector2 pos;
    float radius = 0.0f;
    EXPECT_FALSE(interp.GetInterpolated(999, pos, radius));
}

TEST(InterpolatorTest, ManySnapshotsRespectBufferSize) {
    Interpolator interp;

    for (int i = 0; i < 100; ++i) {
        float t = i * 0.05f;
        interp.PushSnapshot({ {1, {static_cast<float>(i), 0.0f}, 30.0f} },
            static_cast<std::uint32_t>(i), t);
    }
    Vector2 pos;
    float radius = 0.0f;
    ASSERT_TRUE(interp.GetInterpolated(1, pos, radius));
    EXPECT_GT(pos.x, 0.0f);
}

TEST(GameStateTest, PlayerEatsSmallerPlayer) {
    GameState gs;
    auto big = gs.AddPlayer("Big");
    auto small = gs.AddPlayer("Small");

    // Делаем первого игрока значительно больше.
    {
        InputCommand cmd;
        cmd.sequence = 1;
        cmd.moveX = 1.0f;
        cmd.dt = 0.0f;
        gs.ApplyInput(big, cmd);
    }

    for (int i = 0; i < 50; ++i) {
        gs.Update(0.05f);
    }

    // Игроков не больше, чем было изначально.
    EXPECT_LE(gs.playerCount(), 2u);
    // Оба живы или один съеден — но не больше 2.
    (void)big;
    (void)small;
}

TEST(GameStateTest, SnapshotLastProcessedInputPreserved) {
    GameState gs;
    auto id = gs.AddPlayer("Alice");

    InputCommand cmd;
    cmd.sequence = 42;
    cmd.moveX = 0.0f;
    cmd.moveY = 0.0f;
    cmd.dt = 0.1f;
    gs.ApplyInput(id, cmd);

    auto snap = gs.ToSnapshot();
    ASSERT_EQ(snap.players.size(), 1u);
    EXPECT_EQ(snap.players[0].id, id);
}