#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/world/NetherPortalGrowth.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

constexpr std::int32_t kOdds = 2000;

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::function<std::int32_t()> ids() {
    return [next = std::make_shared<std::int32_t>(7)]() mutable { return (*next)++; };
}

/// A legacy source whose first `nextInt(2000)` is exactly `target`, so every case below names the
/// roll it is testing instead of hoping for one.
[[nodiscard]] RandomSource rolling(std::int32_t target) {
    for (std::int64_t seed = 0; seed < 8192; ++seed) {
        RandomSource candidate = RandomSource::legacy(seed);
        if (candidate.nextInt(kOdds) == target) {
            return RandomSource::legacy(seed);
        }
    }
    FAIL("no seed rolls " << target);
    return RandomSource::legacy(0);
}

/// A portal three blocks tall over stone, with air above it.
struct Portal {
    WorldState state = emptyWorld();

    Portal() {
        state.getOrGenerateChunk(0, 0);
        REQUIRE(state.setBlockState(3, 64, 5, stateOf("stone")));
        for (std::int32_t y = 65; y <= 67; ++y) {
            REQUIRE(state.setBlockState(3, y, 5, stateOf("nether_portal")));
        }
    }
};

} // namespace

TEST_CASE("a portal spawns a piglin standing on the block under it") {
    Portal portal;
    GameRuleValues gameRules;
    RandomSource random = rolling(0);

    ServerEntity* piglin =
        netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 67, 5, random, ids());
    REQUIRE(piglin != nullptr);
    CHECK(piglin->type() == Identifier::vanilla("zombified_piglin"));
    const registry::EntityTypeEntry* type = registry::entityTypes::of(Identifier::vanilla("zombified_piglin"));
    REQUIRE(type != nullptr);
    CHECK(piglin->protocolType() == type->protocolId);
    CHECK(piglin->entityId() == 7);

    // The walk went from 67 down to the stone at 64, so the piglin stands at 65.
    CHECK(piglin->x == doctest::Approx(3.5));
    CHECK(piglin->y == doctest::Approx(65.0));
    CHECK(piglin->z == doctest::Approx(5.5));

    CHECK(portal.state.entities().size() == 1);
    CHECK(portal.state.entity(7) == piglin);
    // Exactly one roll went into it: the source now stands where a single roll leaves it.
    RandomSource expected = rolling(0);
    CHECK(expected.nextInt(kOdds) == 0);
    CHECK(random.nextInt(kOdds) == expected.nextInt(kOdds));
}

TEST_CASE("the roll is compared against the difficulty of the world") {
    struct Case {
        std::string difficulty;
        std::int32_t roll;
        bool spawns;
    };

    const Case cases[] = {
        {"easy", 0, true},  {"easy", 1, false},  {"normal", 1, true},  {"normal", 2, false},   {"hard", 2, true},
        {"hard", 3, false}, {"insane", 2, true}, {"insane", 3, false}, {"peaceful", 0, false},
    };

    for (const Case& one : cases) {
        Portal portal;
        GameRuleValues gameRules;
        RandomSource random = rolling(one.roll);
        ServerEntity* piglin =
            netherPortalGrowth::netherPortal(portal.state, gameRules, one.difficulty, true, 3, 67, 5, random, ids());
        CHECK((piglin != nullptr) == one.spawns);
        CHECK(portal.state.entities().size() == (one.spawns ? 1U : 0U));
    }
}

TEST_CASE("peaceful, the game rule and the dimension all refuse before the roll") {
    const std::int32_t firstRoll = 0;

    SUBCASE("a dimension that does not spawn them") {
        Portal portal;
        GameRuleValues gameRules;
        RandomSource random = rolling(firstRoll);
        CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "hard", false, 3, 67, 5, random, ids()) ==
              nullptr);
        // Nothing was rolled: the next number is still the first one.
        CHECK(random.nextInt(kOdds) == firstRoll);
    }

    SUBCASE("monsters switched off") {
        Portal portal;
        GameRuleValues gameRules;
        gameRules.set(gameRules::kSpawnMonsters, "false");
        RandomSource random = rolling(firstRoll);
        CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "hard", true, 3, 67, 5, random, ids()) ==
              nullptr);
        CHECK(random.nextInt(kOdds) == firstRoll);
    }

    SUBCASE("peaceful") {
        Portal portal;
        GameRuleValues gameRules;
        RandomSource random = rolling(firstRoll);
        CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "peaceful", true, 3, 67, 5, random, ids()) ==
              nullptr);
        CHECK(random.nextInt(kOdds) == firstRoll);
    }

    SUBCASE("the rule on by default lets it through") {
        Portal portal;
        GameRuleValues gameRules;
        CHECK(gameRules.get(gameRules::kSpawnMonsters));
        RandomSource random = rolling(firstRoll);
        CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "hard", true, 3, 67, 5, random, ids()) !=
              nullptr);
    }
}

TEST_CASE("the floor has to hold a mob up") {
    SUBCASE("stone does") {
        Portal portal;
        GameRuleValues gameRules;
        RandomSource random = rolling(0);
        CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 67, 5, random, ids()) !=
              nullptr);
    }

    SUBCASE("air under the portal does not") {
        Portal portal;
        REQUIRE(portal.state.setBlockState(3, 64, 5, registry::blocks::air().stateId));
        GameRuleValues gameRules;
        RandomSource random = rolling(0);
        CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 67, 5, random, ids()) ==
              nullptr);
        CHECK(portal.state.entities().empty());
    }

    SUBCASE("grass growing under the portal does not") {
        Portal portal;
        REQUIRE(portal.state.setBlockState(3, 64, 5, stateOf("short_grass")));
        GameRuleValues gameRules;
        RandomSource random = rolling(0);
        CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 67, 5, random, ids()) ==
              nullptr);
    }
}

TEST_CASE("the walk down stops at the first block that is not portal") {
    Portal portal;
    // A second floor halfway up: the piglin has to stand on this one, not on the stone below.
    REQUIRE(portal.state.setBlockState(3, 66, 5, stateOf("stone")));
    GameRuleValues gameRules;
    RandomSource random = rolling(0);

    ServerEntity* piglin =
        netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 67, 5, random, ids());
    REQUIRE(piglin != nullptr);
    CHECK(piglin->y == doctest::Approx(67.0));
}

TEST_CASE("a position that is not a portal at all is judged where it stands") {
    Portal portal;
    GameRuleValues gameRules;
    RandomSource random = rolling(0);

    // The stone itself: nothing to walk down, and stone holds a mob, so it spawns on top of it.
    ServerEntity* piglin =
        netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 64, 5, random, ids());
    REQUIRE(piglin != nullptr);
    CHECK(piglin->y == doctest::Approx(65.0));
}

TEST_CASE("a piglin needs a player near, and the roll is spent looking anyway") {
    Portal portal;
    GameRuleValues gameRules;
    RandomSource random = rolling(0);
    std::int32_t asked = 0;

    const auto nobody = [&asked](std::int32_t x, std::int32_t y, std::int32_t z) {
        ++asked;
        CHECK(x == 3);
        CHECK(y == 67);
        CHECK(z == 5);
        return false;
    };
    CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 67, 5, random, ids(), nobody) ==
          nullptr);
    CHECK(asked == 1);
    // parity: vanilla rolls before it looks for a player, so the roll is gone either way.
    CHECK(random.nextInt(kOdds) != 0);

    RandomSource second = rolling(0);
    const auto somebody = [](std::int32_t, std::int32_t, std::int32_t) { return true; };
    CHECK(netherPortalGrowth::netherPortal(portal.state, gameRules, "easy", true, 3, 67, 5, second, ids(), somebody) !=
          nullptr);
}

TEST_CASE("nothing spawns where there is no world to stand on") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    GameRuleValues gameRules;
    RandomSource random = rolling(0);

    // A chunk that was never generated: no block to read, so no floor.
    CHECK(netherPortalGrowth::netherPortal(world, gameRules, "easy", true, 200, 67, 200, random, ids()) == nullptr);
    CHECK(world.entities().empty());
}
