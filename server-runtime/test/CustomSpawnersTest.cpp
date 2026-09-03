#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/CustomSpawners.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::CatSpawner;
using basalt::server::runtime::world::CatSpawnRequest;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::PatrolSpawner;
using basalt::server::runtime::world::PatrolSpawnRequest;
using basalt::server::runtime::world::PhantomSpawner;
using basalt::server::runtime::world::SpawnRequest;
using basalt::server::runtime::world::TraderLlamaSpawnRequest;
using basalt::server::runtime::world::WanderingTraderSpawner;
using basalt::server::runtime::world::WanderingTraderSpawnRequest;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace config = basalt::server::runtime::config;
namespace gameRules = basalt::server::runtime::world::gameRules;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

struct Fixture {
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<PlayerSession> player;

    explicit Fixture(const std::string& root, config::WorldDimension dimension = config::WorldDimension::Overworld)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{
                  .name = "world", .dimension = dimension, .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})) {
        player = std::make_unique<PlayerSession>(Uuid{1, 1}, "Steve", 1, PlayerConnection{});
        player->worldName = "world";
        player->x = 8.0;
        // Standing on the flat world's own grass, which is three blocks above its floor.
        player->y = -60.0;
        player->z = 8.0;
        world->state().getOrGenerateChunk(0, 0);
    }

    [[nodiscard]] std::vector<PlayerSession*> players() { return {player.get()}; }
};

} // namespace

TEST_CASE("phantoms need the overworld, the night and the game rule") {
    Fixture fixture{scratch("phantom-gates")};
    RandomSource random = RandomSource::legacy(3);
    std::vector<SpawnRequest> spawned;
    const PhantomSpawner::Spawn record = [&spawned](const SpawnRequest& request) { spawned.push_back(request); };

    PhantomSpawner spawner;
    // The very first tick sets the clock rather than spawning, whatever else is true.
    spawner.tick(*fixture.world, true, fixture.players(), random, record);
    CHECK(spawned.empty());

    // With monsters switched off nothing is tried at all, and the clock is not even touched.
    PhantomSpawner peaceful;
    peaceful.tick(*fixture.world, false, fixture.players(), random, record);
    CHECK(spawned.empty());

    // And with the game rule off, the same.
    fixture.world->gameRules().set(gameRules::kSpawnPhantoms, "false");
    PhantomSpawner ruled;
    ruled.tick(*fixture.world, true, fixture.players(), random, record);
    CHECK(spawned.empty());
}

TEST_CASE("no phantom spawns in the nether, which has no sky to fly under") {
    Fixture fixture{scratch("phantom-nether"), config::WorldDimension::Nether};
    RandomSource random = RandomSource::legacy(3);
    std::vector<SpawnRequest> spawned;
    PhantomSpawner spawner;
    for (std::int32_t tick = 0; tick < 5000; tick++) {
        spawner.tick(*fixture.world, true, fixture.players(), random,
                     [&spawned](const SpawnRequest& request) { spawned.push_back(request); });
    }
    CHECK(spawned.empty());
}

TEST_CASE("a cat needs somebody alive to spawn near, and a village or a hut to spawn in") {
    Fixture fixture{scratch("cat-gates")};
    RandomSource random = RandomSource::legacy(7);
    std::vector<CatSpawnRequest> spawned;
    const CatSpawner::Spawn record = [&spawned](const CatSpawnRequest& request) { spawned.push_back(request); };

    CatSpawner spawner;
    // The first tick only starts the clock.
    spawner.tick(*fixture.world, fixture.players(), random, record);
    CHECK(spawned.empty());

    // With nobody alive there is nowhere to measure from, so nothing happens however long it runs.
    fixture.player->dead = true;
    for (std::int32_t tick = 0; tick < 3000; tick++) {
        spawner.tick(*fixture.world, fixture.players(), random, record);
    }
    CHECK(spawned.empty());

    // And with no villagers and no hut, an alive player still gets no cat.
    fixture.player->dead = false;
    CatSpawner again;
    for (std::int32_t tick = 0; tick < 3000; tick++) {
        again.tick(*fixture.world, fixture.players(), random, record);
    }
    CHECK(spawned.empty());
}

TEST_CASE("a swamp hut spawns exactly one black cat and then stops") {
    Fixture fixture{scratch("cat-hut")};
    // Enough chunks around the player for the loaded-square check.
    for (std::int32_t chunkX = -2; chunkX <= 2; chunkX++) {
        for (std::int32_t chunkZ = -2; chunkZ <= 2; chunkZ++) {
            fixture.world->state().getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    RandomSource random = RandomSource::legacy(4);
    std::vector<CatSpawnRequest> spawned;
    CatSpawner spawner{[](LoadedWorld&, std::int32_t, std::int32_t, std::int32_t) { return true; }};
    for (std::int32_t tick = 0; tick < 12000 && spawned.empty(); tick++) {
        spawner.tick(*fixture.world, fixture.players(), random,
                     [&spawned](const CatSpawnRequest& request) { spawned.push_back(request); });
    }
    REQUIRE_FALSE(spawned.empty());
    // A hut cat is the black one and stays where it was put, unlike a village stray.
    CHECK(spawned.front().persistent);
    CHECK(spawned.front().context.inBlackCatStructure);
}

TEST_CASE("no patrol before the fifth day, and none at all without the game rule") {
    Fixture fixture{scratch("patrol-gates")};
    RandomSource random = RandomSource::legacy(2);
    std::vector<PatrolSpawnRequest> spawned;
    const PatrolSpawner::Spawn record = [&spawned](const PatrolSpawnRequest& request) {
        spawned.push_back(request);
        return true;
    };

    // The clock starts at zero, which is well before patrols are enabled at all.
    PatrolSpawner spawner;
    for (std::int32_t tick = 0; tick < 60000; tick++) {
        spawner.tick(*fixture.world, true, fixture.players(), random, record);
    }
    CHECK(spawned.empty());

    fixture.world->gameRules().set(gameRules::kSpawnPatrols, "false");
    PatrolSpawner ruled;
    for (std::int32_t tick = 0; tick < 60000; tick++) {
        ruled.tick(*fixture.world, true, fixture.players(), random, record);
    }
    CHECK(spawned.empty());
}

TEST_CASE("the trader's chance climbs until one arrives, and resets when it does") {
    Fixture fixture{scratch("trader-chance")};
    RandomSource levelRandom = RandomSource::legacy(1);
    // A stream that never rolls the arrival, so the chance is free to climb to its ceiling.
    WanderingTraderSpawner spawner{{}, RandomSource::legacy(2)};
    for (std::int32_t tick = 0; tick < 200000; tick++) {
        spawner.tick(
            *fixture.world, fixture.players(), levelRandom,
            [](const WanderingTraderSpawnRequest&) { return std::optional<std::int32_t>{}; },
            [](const TraderLlamaSpawnRequest&, std::int32_t) {});
    }
    // parity: 25 up by 25 a time to a ceiling of 75, and never past it.
    CHECK(spawner.savedState().spawnChance <= 75);
    CHECK(spawner.savedState().spawnChance >= 25);
}

TEST_CASE("no trader without the game rule, whatever the clock says") {
    Fixture fixture{scratch("trader-rule")};
    fixture.world->gameRules().set(basalt::server::runtime::world::gameRules::kSpawnWanderingTraders, "false");
    RandomSource levelRandom = RandomSource::legacy(1);
    std::int32_t offered = 0;
    WanderingTraderSpawner spawner;
    for (std::int32_t tick = 0; tick < 60000; tick++) {
        spawner.tick(
            *fixture.world, fixture.players(), levelRandom,
            [&offered](const WanderingTraderSpawnRequest&) {
                offered++;
                return std::optional<std::int32_t>{7};
            },
            [](const TraderLlamaSpawnRequest&, std::int32_t) {});
    }
    CHECK(offered == 0);
    // And the saved state is untouched, so switching the rule back on does not owe a trader.
    CHECK(spawner.savedState().spawnDelay == 24000);
}
