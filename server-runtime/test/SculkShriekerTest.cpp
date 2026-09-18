#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/SculkShriekers.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;
namespace tracker = wardenSpawnTracker;

TEST_CASE("a warning raises everybody near to the highest level and then cools down") {
    tracker::Tracker quiet;
    tracker::Tracker warned{.warningLevel = 2};
    CHECK(tracker::warn({&quiet, &warned}) == 3);
    CHECK(quiet == warned);
    CHECK(quiet.cooldownTicks == tracker::kWarningCooldown);
    // parity: anybody still cooling down stops the whole warning.
    CHECK_FALSE(tracker::warn({&quiet}).has_value());
    for (std::int32_t tick = 0; tick < tracker::kWarningCooldown; ++tick) {
        quiet.tick();
    }
    CHECK(tracker::warn({&quiet}) == 4);
    for (std::int32_t tick = 0; tick < tracker::kWarningCooldown; ++tick) {
        quiet.tick();
    }
    CHECK(tracker::warn({&quiet}) == tracker::kMaxWarning);
}

TEST_CASE("a warning fades a level every ten minutes") {
    tracker::Tracker warned{.warningLevel = 2};
    for (std::int32_t tick = 0; tick <= tracker::kWarningDecayTicks; ++tick) {
        warned.tick();
    }
    CHECK(warned.warningLevel == 1);
    CHECK(warned.ticksSinceLastWarning == 0);
}

TEST_CASE("the shrieker's state and its warning level") {
    WorldState state{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
    state.getOrGenerateChunk(0, 0);
    const std::optional<std::int32_t> summoner = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("sculk_shrieker"),
        {{"can_summon", "true"}, {"shrieking", "false"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(summoner.has_value());
    CHECK(sculkShriekers::canSummon(*summoner));
    CHECK_FALSE(sculkShriekers::isShrieking(*summoner));
    const std::optional<std::int32_t> loud = sculkShriekers::withShrieking(*summoner, true);
    REQUIRE(loud.has_value());
    CHECK(sculkShriekers::isShrieking(*loud));
    CHECK(sculkShriekers::canSummon(*loud));
    static_cast<void>(state.setBlockState(3, 64, 3, *summoner));
    CHECK(sculkShriekers::warningLevel(state, 3, 64, 3) == 0);
    sculkShriekers::setWarningLevel(state, 3, 64, 3, 4);
    CHECK(sculkShriekers::warningLevel(state, 3, 64, 3) == 4);
}

TEST_CASE("a warden rises on the first full top face in the column") {
    WorldState state{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
    for (std::int32_t chunkX = -1; chunkX <= 0; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 0; ++chunkZ) {
            state.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    const std::int32_t stone = registry::blocks::named("stone")->stateId;
    for (std::int32_t x = -6; x <= 12; ++x) {
        for (std::int32_t z = -6; z <= 12; ++z) {
            static_cast<void>(state.setBlockState(x, 60, z, stone));
        }
    }
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(5);
    std::int32_t attempt = 0;
    const auto at = sculkShriekers::spawnPosition(state, 3, 61, 3, random, attempt);
    REQUIRE(at.has_value());
    CHECK((*at)[1] == 61);
    CHECK(std::abs((*at)[0] - 3) <= sculkShriekers::kSpawnRangeXZ);
    CHECK(attempt == 1);
}
