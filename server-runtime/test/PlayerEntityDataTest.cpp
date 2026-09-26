#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/player/PlayerEntityData.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <doctest.h>

#include <algorithm>
#include <map>
#include <optional>

using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerEntityData;
using basalt::server::runtime::player::PlayerSession;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::Pose;
using basalt::wire::protocol::play::SetEntityDataPacket;
namespace blocks = basalt::server::registry::blocks;

namespace {

[[nodiscard]] PlayerSession freshPlayer() { return PlayerSession{Uuid{1, 2}, "Alex", 7, PlayerConnection{}}; }

[[nodiscard]] bool has(const std::vector<SetEntityDataPacket::Entry>& entries, std::uint8_t index) {
    return std::ranges::any_of(entries,
                               [index](const SetEntityDataPacket::Entry& entry) { return entry.index == index; });
}

/// A world of air with stone at the given squares.
struct Blocks {
    std::map<std::array<std::int32_t, 3>, std::int32_t> solid;

    [[nodiscard]] basalt::server::runtime::entity::entityCollision::BlockAt at() const {
        return [this](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
            const auto found = solid.find({x, y, z});
            return found == solid.end() ? blocks::air().stateId : found->second;
        };
    }
};

} // namespace

TEST_CASE("a player comes into view with only what differs from the defined defaults") {
    PlayerSession player = freshPlayer();
    const std::vector<SetEntityDataPacket::Entry> pairing = PlayerEntityData::of(player).changesFrom(nullptr);
    // parity: health is defined at one and set to twenty, and every skin layer is on.
    CHECK(has(pairing, PlayerEntityData::kHealthIndex));
    CHECK(has(pairing, SetEntityDataPacket::kPlayerSkinPartsIndex));
    // The right hand, full air, standing and no score are all the defaults, so none of them goes.
    CHECK_FALSE(has(pairing, PlayerEntityData::kMainHandIndex));
    CHECK_FALSE(has(pairing, SetEntityDataPacket::kAirSupplyIndex));
    CHECK_FALSE(has(pairing, SetEntityDataPacket::kPoseIndex));
    CHECK_FALSE(has(pairing, PlayerEntityData::kScoreIndex));

    const PlayerEntityData before = PlayerEntityData::of(player);
    player.mainArm = 0;
    player.sneaking = true;
    const std::vector<SetEntityDataPacket::Entry> changes = PlayerEntityData::of(player).changesFrom(&before);
    CHECK(changes.size() == 2);
    CHECK(has(changes, PlayerEntityData::kMainHandIndex));
    CHECK(has(changes, SetEntityDataPacket::kSharedFlagsIndex));
}

TEST_CASE("a player crouches, crawls under a block and swims as updatePlayerPose decides") {
    PlayerSession player = freshPlayer();
    player.x = 0.5;
    player.y = 64.0;
    player.z = 0.5;
    Blocks world;
    basalt::server::runtime::player::tickBody(player, world.at());
    CHECK(player.pose == Pose::Standing);

    player.sneaking = true;
    basalt::server::runtime::player::tickBody(player, world.at());
    CHECK(player.pose == Pose::Crouching);

    // A block a square above the feet leaves room only to lie flat.
    player.sneaking = false;
    world.solid[{0, 65, 0}] = blocks::named("stone")->stateId;
    basalt::server::runtime::player::tickBody(player, world.at());
    CHECK(player.pose == Pose::Swimming);

    // Sprinting with the eyes under water starts a swim.
    world.solid.clear();
    const std::int32_t water = blocks::named("water")->stateId;
    world.solid[{0, 64, 0}] = water;
    world.solid[{0, 65, 0}] = water;
    player.sprinting = true;
    basalt::server::runtime::player::tickBody(player, world.at());
    CHECK(player.swimming);
    CHECK(player.pose == Pose::Swimming);
}

TEST_CASE("a stuck arrow falls out after twenty times thirty less the count") {
    PlayerSession player = freshPlayer();
    player.arrowCount = 2;
    Blocks world;
    for (std::int32_t tick = 0; tick < 20 * 28 - 1; ++tick) {
        basalt::server::runtime::player::tickBody(player, world.at());
    }
    CHECK(player.arrowCount == 2);
    basalt::server::runtime::player::tickBody(player, world.at());
    CHECK(player.arrowCount == 1);
}
