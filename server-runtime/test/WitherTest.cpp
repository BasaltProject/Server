#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/world/WitherPatterns.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace boss = basalt::server::runtime::entity::witherBoss;
namespace combat = basalt::server::runtime::entity::entityCombat;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties = {}) {
    const std::optional<std::int32_t> stateId =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

struct Loaded {
    WorldState state{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};

    Loaded() { state.getOrGenerateChunk(0, 0); }

    void put(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& path) {
        REQUIRE(state.setBlockState(x, y, z, stateOf(path)));
    }

    /// The T along x at height y, skulls on top.
    void buildT(std::int32_t y) {
        put(5, y, 5, "soul_sand");
        for (std::int32_t x = 4; x <= 6; ++x) {
            put(x, y + 1, 5, "soul_sand");
            put(x, y + 2, 5, "wither_skeleton_skull");
        }
    }
};

const witherPatterns::IsBase kSoul = [](std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && (block->path() == "soul_sand" || block->path() == "soul_soil");
};

[[nodiscard]] ServerEntity wither() {
    ServerEntity made{1, Uuid{1, 1}, Identifier::vanilla("wither"), 0, 0.0, 64.0, 0.0};
    made.wither = std::make_shared<boss::WitherState>();
    return made;
}

} // namespace

// parity: `getForwards` is across the T, so one spanning x is turned to 90 degrees.
TEST_CASE("the T of soul sand with three skulls is a wither, found from any skull") {
    Loaded loaded;
    loaded.buildT(64);
    for (std::int32_t x = 4; x <= 6; ++x) {
        const std::optional<witherPatterns::Match> match = witherPatterns::find(loaded.state, x, 66, 5, kSoul);
        REQUIRE(match.has_value());
        CHECK_FALSE(match->alongX);
        CHECK(match->spawn == blockPatterns::Cell{5, 64, 5});
        CHECK(match->cells.size() == 9U);
    }
}

TEST_CASE("the T needs air in its bottom corners and all three skulls") {
    Loaded loaded;
    loaded.buildT(64);
    loaded.put(4, 64, 5, "stone");
    CHECK_FALSE(witherPatterns::find(loaded.state, 5, 66, 5, kSoul).has_value());

    Loaded missing;
    missing.buildT(64);
    missing.put(6, 66, 5, "air");
    CHECK_FALSE(witherPatterns::find(missing.state, 5, 66, 5, kSoul).has_value());
}

TEST_CASE("a T built along z is turned the other way") {
    Loaded loaded;
    loaded.put(5, 64, 5, "soul_soil");
    for (std::int32_t z = 4; z <= 6; ++z) {
        loaded.put(5, 65, z, "soul_sand");
        loaded.put(5, 66, z, "wither_skeleton_wall_skull");
    }
    const std::optional<witherPatterns::Match> match = witherPatterns::find(loaded.state, 5, 66, 4, kSoul);
    REQUIRE(match.has_value());
    CHECK(match->alongX);
}

TEST_CASE("a newborn wither refuses everything but the void") {
    ServerEntity made = wither();
    boss::makeInvulnerable(made, *made.wither);
    CHECK(made.health == doctest::Approx(100.0));
    CHECK_FALSE(combat::hurt(made, 5.0F, std::nullopt, std::nullopt, {}, "attack").landed);
    CHECK(combat::hurt(made, 5.0F, std::nullopt, std::nullopt, {}, "out_of_world").landed);
}

TEST_CASE("an armoured wither shrugs off arrows but not swords, and undead never hurt it") {
    ServerEntity made = wither();
    made.health = 100.0;
    CHECK_FALSE(combat::hurt(made, 5.0F, std::nullopt, std::nullopt, {}, "projectile").landed);
    CHECK_FALSE(
        combat::hurt(made, 5.0F, std::nullopt, std::nullopt, {}, "mob_attack", std::string{"minecraft:zombie"}).landed);
    made.wither->nextHitFromFriend = true;
    CHECK_FALSE(combat::hurt(made, 5.0F, std::nullopt, std::nullopt, {}, "explosion").landed);
    CHECK_FALSE(made.wither->nextHitFromFriend);
    CHECK(combat::hurt(made, 5.0F, std::nullopt, std::nullopt, {}, "attack").landed);
    CHECK(made.wither->destroyBlocksTick == 20);
    CHECK(made.wither->idleHeadUpdates[0] == 3);
}

TEST_CASE("the side heads sit either side of the body and the middle one on top") {
    const ServerEntity made = wither();
    const std::array<double, 3> middle = boss::headPosition(made, 0);
    CHECK(middle[1] == doctest::Approx(67.0));
    const std::array<double, 3> side = boss::headPosition(made, 1);
    CHECK(side[0] == doctest::Approx(1.3));
    CHECK(side[1] == doctest::Approx(66.2));
    CHECK(boss::headPosition(made, 2)[0] == doctest::Approx(-1.3));
}
