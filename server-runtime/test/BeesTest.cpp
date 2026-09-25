#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/BeeNavigation.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::nbt::NbtCompound;
namespace bees = basalt::server::runtime::entity::bees;
namespace beehives = basalt::server::runtime::world::beehives;
namespace nav = basalt::server::runtime::entity::ai::beeNavigation;

TEST_CASE("a hive keeps a nectar bee for 2400 ticks and lets it out on the tick after") {
    NbtCompound hive;
    NbtCompound bee;
    bee.putString("id", "minecraft:bee");
    bee.putBoolean("HasNectar", true);
    beehives::addOccupant(hive, bee);
    NbtCompound empty;
    empty.putString("id", "minecraft:bee");
    beehives::addOccupant(hive, empty);
    CHECK(beehives::occupantCount(hive) == 2);
    CHECK(beehives::occupant(hive, 0)->getInt("min_ticks_in_hive") == beehives::kMinTicksNectar);
    CHECK(beehives::occupant(hive, 1)->getInt("min_ticks_in_hive") == beehives::kMinTicksNectarless);

    // parity: `ticksInHive++ > minTicksInHive`, so the stay is one more than the minimum.
    std::vector<std::size_t> due;
    for (std::int32_t tick = 0; tick <= beehives::kMinTicksNectarless; ++tick) {
        beehives::tickOccupants(hive, due);
        CHECK(due.empty());
    }
    beehives::tickOccupants(hive, due);
    REQUIRE(due == std::vector<std::size_t>{1});
    beehives::removeOccupants(hive, due);
    CHECK(beehives::occupantCount(hive) == 1);
    CHECK(beehives::occupant(hive, 0)->getInt("ticks_in_hive") == beehives::kMinTicksNectarless + 2);
}

TEST_CASE("a bee's own fields survive a save and a load") {
    const basalt::server::registry::EntityTypeEntry* entry =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("bee"));
    REQUIRE(entry != nullptr);
    ServerEntity bee{1, Uuid{1, 1}, entry->id, entry->protocolId, 0.0, 64.0, 0.0};
    RandomSource random = RandomSource::legacy(0);
    bees::BeeState& state = bees::stateOf(bee, random);
    state.hive = bees::Pos{1, 2, 3};
    state.flower = bees::Pos{-4, 5, 6};
    state.hasNectar = true;
    state.cropsGrown = 7;
    state.angerEndTime = 1234;
    state.angryAt = Uuid{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    NbtCompound tag;
    bees::write(bee, tag);

    ServerEntity loaded{2, Uuid{2, 2}, entry->id, entry->protocolId, 0.0, 64.0, 0.0};
    loaded.sourceNbt = tag;
    const bees::BeeState& read = bees::stateOf(loaded, random);
    CHECK(read.hive == state.hive);
    CHECK(read.flower == state.flower);
    CHECK(read.hasNectar);
    CHECK(read.cropsGrown == 7);
    CHECK(read.angerEndTime == 1234);
    CHECK(read.angryAt == state.angryAt);
    // parity: `IGNORED_BEE_TAGS` takes the hive and the counters, never the nectar.
    bees::stripHiveTags(tag);
    CHECK_FALSE(tag.contains("hive_pos"));
    CHECK(tag.contains("HasNectar"));
}

TEST_CASE("the manhattan box walks the nearest shell first, x then y, and mirrors z") {
    std::vector<nav::Block> seen;
    static_cast<void>(nav::firstByManhattan(nav::Block{0, 0, 0}, 1, [&seen](const nav::Block& at) {
        seen.push_back(at);
        return false;
    }));
    const std::vector<nav::Block> first{{0, 0, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {1, 0, 0}};
    REQUIRE(seen.size() >= first.size());
    CHECK(std::vector<nav::Block>(seen.begin(), seen.begin() + static_cast<std::ptrdiff_t>(first.size())) == first);
    // Every square of the three by three by three box, once.
    CHECK(seen.size() == 27);
}
