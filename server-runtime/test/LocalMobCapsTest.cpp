#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/LocalMobCaps.hpp>

#include <doctest.h>

#include <cstdint>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::entity::MobCategory;
using basalt::server::runtime::entity::mobCategoryFacts;

TEST_CASE("a chunk with nobody near it spawns nothing") {
    LocalMobCaps caps{{}};
    CHECK_FALSE(caps.canSpawn(MobCategory::Monster, 0, 0));
    CHECK_FALSE(caps.isSpawnCandidate(0, 0));
    // Counting against nobody is not an error.
    caps.add(MobCategory::Monster, 0, 0);
    CHECK(caps.count(1, MobCategory::Monster) == 0);
}

TEST_CASE("a player caps the chunks within a hundred and twenty-eight blocks of them") {
    LocalMobCaps caps{{MobCapPlayer{.id = 1, .x = 8.0, .z = 8.0}}};
    CHECK(caps.canSpawn(MobCategory::Monster, 0, 0));
    // Seven chunks away is 112 blocks to the centre, which is inside.
    CHECK(caps.canSpawn(MobCategory::Monster, 7, 0));
    // Eight chunks away is 128 exactly, and the comparison is strict.
    CHECK_FALSE(caps.canSpawn(MobCategory::Monster, 8, 0));
    CHECK_FALSE(caps.canSpawn(MobCategory::Monster, 20, 20));
    // Far along one axis and right beside them on the other: the distance is the real one, not one
    // axis of it.
    CHECK_FALSE(caps.canSpawn(MobCategory::Monster, 0, 20));
    CHECK_FALSE(caps.canSpawn(MobCategory::Monster, 20, 0));
    // And the diagonal counts: five chunks each way is 113 blocks, which is still inside.
    CHECK(caps.canSpawn(MobCategory::Monster, 5, 5));
    CHECK_FALSE(caps.canSpawn(MobCategory::Monster, 6, 6));
}

TEST_CASE("every mob near a player counts against that player, and only that player") {
    LocalMobCaps caps{{MobCapPlayer{.id = 1, .x = 8.0, .z = 8.0}, MobCapPlayer{.id = 2, .x = 1000.0, .z = 1000.0}}};
    caps.add(MobCategory::Monster, 0, 0);
    caps.add(MobCategory::Monster, 0, 0);
    CHECK(caps.count(1, MobCategory::Monster) == 2);
    CHECK(caps.count(2, MobCategory::Monster) == 0);
    // Its own category, not the others.
    CHECK(caps.count(1, MobCategory::Creature) == 0);

    // A mob between them counts for both.
    LocalMobCaps shared{{MobCapPlayer{.id = 1, .x = 0.0, .z = 0.0}, MobCapPlayer{.id = 2, .x = 100.0, .z = 0.0}}};
    shared.add(MobCategory::Creature, 3, 0);
    CHECK(shared.count(1, MobCategory::Creature) == 1);
    CHECK(shared.count(2, MobCategory::Creature) == 1);
}

TEST_CASE("a full cap stops the spawn, and a second player lifts it again") {
    const std::int32_t monsters = mobCategoryFacts(MobCategory::Monster).maxPerChunk;
    REQUIRE(monsters > 0);

    LocalMobCaps caps{{MobCapPlayer{.id = 1, .x = 8.0, .z = 8.0}}};
    for (std::int32_t at = 0; at < monsters - 1; ++at) {
        caps.add(MobCategory::Monster, 0, 0);
        CHECK(caps.canSpawn(MobCategory::Monster, 0, 0));
    }
    caps.add(MobCategory::Monster, 0, 0);
    CHECK(caps.count(1, MobCategory::Monster) == monsters);
    CHECK_FALSE(caps.canSpawn(MobCategory::Monster, 0, 0));
    // Other categories are their own count and their own cap, and the caps differ: ten animals
    // against seventy monsters, so filling one says nothing about the other.
    CHECK(caps.canSpawn(MobCategory::Creature, 0, 0));
    const std::int32_t animals = mobCategoryFacts(MobCategory::Creature).maxPerChunk;
    CHECK(animals < monsters);
    for (std::int32_t at = 0; at < animals; ++at) {
        caps.add(MobCategory::Creature, 0, 0);
    }
    CHECK_FALSE(caps.canSpawn(MobCategory::Creature, 0, 0));

    // parity: any nearby player under the cap is enough, so somebody who has just arrived opens the
    // chunk again.
    LocalMobCaps two{{MobCapPlayer{.id = 1, .x = 8.0, .z = 8.0}, MobCapPlayer{.id = 2, .x = 200.0, .z = 8.0}}};
    for (std::int32_t at = 0; at < monsters; ++at) {
        two.add(MobCategory::Monster, 0, 0);
    }
    CHECK_FALSE(two.canSpawn(MobCategory::Monster, 0, 0));
    // The chunk between them: player two is under their own cap, so it can still spawn there.
    CHECK(two.canSpawn(MobCategory::Monster, 8, 0));
}

TEST_CASE("the miscellaneous category has no cap and counts towards none") {
    LocalMobCaps caps{{MobCapPlayer{.id = 1, .x = 8.0, .z = 8.0}}};
    for (std::int32_t at = 0; at < 100; ++at) {
        caps.add(MobCategory::Misc, 0, 0);
    }
    CHECK(caps.count(1, MobCategory::Misc) == 0);
}

TEST_CASE("the square of chunks a spawn pass even looks at") {
    LocalMobCaps caps{{MobCapPlayer{.id = 1, .x = 0.5, .z = 0.5}}};
    CHECK(caps.isSpawnCandidate(0, 0));
    CHECK(caps.isSpawnCandidate(8, 8));
    CHECK(caps.isSpawnCandidate(-8, -8));
    CHECK_FALSE(caps.isSpawnCandidate(9, 0));
    CHECK_FALSE(caps.isSpawnCandidate(0, -9));

    // A player west of the origin is in chunk minus one, not chunk zero: the floor has to be the
    // one that rounds down rather than towards zero.
    LocalMobCaps west{{MobCapPlayer{.id = 1, .x = -0.5, .z = -0.5}}};
    CHECK(west.isSpawnCandidate(-9, -9));
    CHECK_FALSE(west.isSpawnCandidate(8, 8));
}
