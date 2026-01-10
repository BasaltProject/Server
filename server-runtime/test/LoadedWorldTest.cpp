#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using basalt::server::runtime::config::WorldDeclaration;
using basalt::server::runtime::config::WorldDimension;
using basalt::server::runtime::config::WorldSettings;
using basalt::server::runtime::config::WorldType;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::LoadedWorldOptions;
using basalt::server::runtime::world::WorldMetadata;
using basalt::server::world::Chunk;
namespace gameRules = basalt::server::runtime::world::gameRules;
namespace reg = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] WorldDeclaration declarationOf(std::string name, WorldType type = WorldType::Flat,
                                             bool isDefault = true) {
    return WorldDeclaration{.name = std::move(name),
                            .dimension = WorldDimension::Overworld,
                            .type = type,
                            .seed = 1234,
                            .isDefault = isDefault};
}

[[nodiscard]] WorldMetadata metadataOf() {
    WorldMetadata metadata;
    metadata.seed = 1234;
    metadata.spawnX = WorldMetadata::kDefaultSpawn[0];
    metadata.spawnY = WorldMetadata::kDefaultSpawn[1];
    metadata.spawnZ = WorldMetadata::kDefaultSpawn[2];
    return metadata;
}

} // namespace

TEST_CASE("a world knows where its own files live and what the client calls it") {
    const std::string root = scratch("loaded-world-names");
    LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};

    CHECK(world.name() == "world");
    CHECK(world.worldRoot() == root + "/world");
    // parity: the default world keeps vanilla's own level key, so an unmodified setup looks
    // exactly as before in the client's debug screen.
    CHECK(world.levelKey() == "minecraft:overworld");
}

TEST_CASE("a world that is not the default gets a key of its own") {
    const std::string root = scratch("loaded-world-key");
    LoadedWorld world{declarationOf("Skyblock Nether!", WorldType::Flat, false), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};

    // Anything a resource location cannot carry is replaced rather than refused.
    CHECK(world.levelKey() == "basalt:skyblock_nether_");
}

TEST_CASE("a flat world generates and keeps its chunks") {
    const std::string root = scratch("loaded-world-chunks");
    LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};

    const Chunk& chunk = world.state().getOrGenerateChunk(0, 0);

    CHECK(chunk.chunkX() == 0);
    CHECK(world.state().loadedChunkCount() == 1);
    // A flat world has a floor, which is what tells it apart from a void one.
    CHECK(world.state().blockState(0, -64, 0).value_or(0) == reg::blocks::bedrock().stateId);
    CHECK(world.state().blockState(0, -61, 0).value_or(0) != reg::blocks::air().stateId);
}

TEST_CASE("a void world is empty but for the platform under its spawn") {
    const std::string root = scratch("loaded-world-void");
    LoadedWorld world{declarationOf("world", WorldType::Void), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};

    static_cast<void>(world.state().getOrGenerateChunk(0, 0));

    CHECK(world.state().blockState(8, 63, 8).value_or(0) != reg::blocks::air().stateId);
    CHECK(world.state().blockState(0, 63, 0).value_or(1) == reg::blocks::air().stateId);
}

TEST_CASE("the clock and the weather advance with the world, and the rules can stop them") {
    const std::string root = scratch("loaded-world-time");
    LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(world.tickTime(random));
    CHECK(world.gameTime() == 1);
    CHECK(world.clock().totalTicks() == 1);

    world.gameRules().set(gameRules::kAdvanceTime, "false");
    CHECK_FALSE(world.tickTime(random));
    CHECK(world.gameTime() == 2);
    CHECK(world.clock().totalTicks() == 1);
}

TEST_CASE("time does not run in the Nether, whatever the rules say") {
    const std::string root = scratch("loaded-world-nether");
    WorldDeclaration declaration = declarationOf("nether");
    declaration.dimension = WorldDimension::Nether;
    LoadedWorld world{declaration, WorldSettings{}, metadataOf(), root, basalt::wire::common::Logger{"Test"}};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // parity: `dimension_type.default_clock`, which the Nether names none of.
    CHECK_FALSE(world.tickTime(random));
    CHECK(world.clock().totalTicks() == 0);
    CHECK(world.gameTime() == 1);
}

TEST_CASE("moving the spawn is remembered and written down") {
    const std::string root = scratch("loaded-world-spawn");
    LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};

    world.setSpawn(10, 70, -20, 90.0F);

    CHECK(world.spawn()[0] == doctest::Approx(10.5));
    CHECK(world.spawn()[1] == doctest::Approx(70.0));
    CHECK(world.spawn()[2] == doctest::Approx(-19.5));
    CHECK(world.spawnYaw() == doctest::Approx(90.0F));

    world.saveMetadata();
    const std::optional<WorldMetadata> saved = WorldMetadata::read(world.worldRoot());
    REQUIRE(saved.has_value());
    CHECK(saved->spawnX == 10);
    CHECK(saved->spawnY == 70);
    CHECK(saved->spawnZ == -20);
}

TEST_CASE("what the world was ticking is what a restart reads back") {
    const std::string root = scratch("loaded-world-metadata");
    rng::RandomSource random = rng::RandomSource::legacy(1);
    {
        LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                          basalt::wire::common::Logger{"Test"}};
        for (std::int32_t tick = 0; tick < 50; tick++) {
            static_cast<void>(world.tickTime(random));
        }
        world.setDifficulty("hard");
        world.setDifficultyLocked(true);
        world.setDefaultGamemode("adventure");
        world.saveMetadata();
    }

    const std::optional<WorldMetadata> saved = WorldMetadata::read(root + "/world");
    REQUIRE(saved.has_value());
    CHECK(saved->gameTime == 50);
    CHECK(saved->timeOfDay == 50);
    REQUIRE(saved->difficulty.has_value());
    CHECK(*saved->difficulty == "hard");
    CHECK(saved->difficultyLocked);
    REQUIRE(saved->defaultGamemode.has_value());
    CHECK(*saved->defaultGamemode == "adventure");
}

TEST_CASE("the difficulty falls back to the settings until a command moves it") {
    const std::string root = scratch("loaded-world-difficulty");
    WorldSettings settings;
    settings.difficulty = "normal";
    LoadedWorld world{declarationOf("world"), settings, metadataOf(), root, basalt::wire::common::Logger{"Test"}};

    CHECK(world.difficulty() == "normal");

    WorldMetadata stored = metadataOf();
    stored.difficulty = "peaceful";
    LoadedWorld remembered{declarationOf("world"), settings, stored, scratch("loaded-world-difficulty-2"),
                           basalt::wire::common::Logger{"Test"}};
    CHECK(remembered.difficulty() == "peaceful");
}

TEST_CASE("a chunk that was saved comes back off disk rather than being generated again") {
    const std::string root = scratch("loaded-world-save");
    {
        LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                          basalt::wire::common::Logger{"Test"}};
        static_cast<void>(world.state().getOrGenerateChunk(0, 0));
        REQUIRE(world.state().setBlockState(1, 40, 1, reg::blocks::stone().stateId));
        world.saveDirty();
        world.flushRegions();
    }

    LoadedWorld reopened{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                         basalt::wire::common::Logger{"Test"}};
    static_cast<void>(reopened.state().getOrGenerateChunk(0, 0));

    CHECK(reopened.state().blockState(1, 40, 1).value_or(0) == reg::blocks::stone().stateId);
}

TEST_CASE("chunks nobody can reach are dropped, and the ones that are kept stay") {
    const std::string root = scratch("loaded-world-unload");
    LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};
    for (std::int32_t chunkX = 0; chunkX < 6; chunkX++) {
        static_cast<void>(world.state().getOrGenerateChunk(chunkX, 0));
    }
    world.forcedChunks().insert(basalt::server::runtime::world::ChunkGenerationQueue::pack(5, 0));

    const std::size_t evicted = world.unloadUnreachable({}, 0, 0);

    CHECK(evicted > 0);
    // The forced one is still there, and so is the spawn chunk.
    CHECK(world.state().isLoaded(5, 0));
    CHECK(world.state().isLoaded(0, 0));
    CHECK_FALSE(world.state().isLoaded(3, 0));
}

TEST_CASE("a player keeps the chunks around them loaded") {
    const std::string root = scratch("loaded-world-retain");
    WorldSettings settings;
    settings.viewDistance = 2;
    LoadedWorld world{declarationOf("world"), settings, metadataOf(), root, basalt::wire::common::Logger{"Test"}};
    for (std::int32_t chunkX = 8; chunkX < 14; chunkX++) {
        static_cast<void>(world.state().getOrGenerateChunk(chunkX, 0));
    }

    static_cast<void>(world.unloadUnreachable({{10, 0}}, 0, 0));

    CHECK(world.state().isLoaded(10, 0));
    CHECK(world.state().isLoaded(12, 0));
    CHECK_FALSE(world.state().isLoaded(13, 0));
}

TEST_CASE("the spawn of a void world is whatever its metadata says") {
    const std::string root = scratch("loaded-world-void-spawn");
    WorldMetadata metadata = metadataOf();
    metadata.spawnX = 100;
    metadata.spawnY = 65;
    metadata.spawnZ = -50;
    LoadedWorld world{declarationOf("world", WorldType::Void), WorldSettings{}, metadata, root,
                      basalt::wire::common::Logger{"Test"}};

    world.warmSpawnArea(0);

    CHECK(world.spawn()[0] == doctest::Approx(100.5));
    CHECK(world.spawn()[1] == doctest::Approx(65.0));
    CHECK(world.spawn()[2] == doctest::Approx(-49.5));
}

TEST_CASE("a flat world puts its spawn on the first block that can hold a player") {
    const std::string root = scratch("loaded-world-flat-spawn");
    LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};

    world.warmSpawnArea(1);

    CHECK(world.spawn()[1] > -64.0);
    CHECK(world.state().isLoaded(0, 0));
}

TEST_CASE("every world is ticked in a region, and the owner is the region's") {
    const std::string root = scratch("loaded-world-owner");
    LoadedWorld world{declarationOf("world"), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"}};

    CHECK(world.region().worldName() == "world");
    CHECK(&world.ownerOf(0, 0) == &world.owner());
    CHECK(&world.ownerOf(1000, -1000) == &world.owner());
}

TEST_CASE("a world with a scheduler of its own shares it with everything in that scheduler") {
    const std::string root = scratch("loaded-world-scheduler");
    basalt::server::runtime::sched::RegionScheduler scheduler;
    LoadedWorldOptions options;
    options.scheduler = &scheduler;
    LoadedWorld first{
        declarationOf("first"), WorldSettings{}, metadataOf(), root, basalt::wire::common::Logger{"Test"}, options};
    LoadedWorld second{declarationOf("second", WorldType::Flat, false),
                       WorldSettings{},
                       metadataOf(),
                       root,
                       basalt::wire::common::Logger{"Test"},
                       options};

    CHECK(&first.owner() != &second.owner());
    CHECK(scheduler.regionsOf("first").size() == 1);
    CHECK(scheduler.regionsOf("second").size() == 1);
}

TEST_CASE("a generator handed in is what the world generates with") {
    const std::string root = scratch("loaded-world-generator");
    std::int32_t calls = 0;
    LoadedWorldOptions options;
    options.normalGenerator = [&calls](std::int32_t chunkX, std::int32_t chunkZ, std::int64_t, std::string_view,
                                       const std::function<void(Chunk&)>&) {
        calls++;
        Chunk chunk{chunkX, chunkZ};
        chunk.setBlockStateId(0, 0, 0, reg::blocks::bedrock().stateId);
        return chunk;
    };
    LoadedWorld world{declarationOf("world", WorldType::Normal), WorldSettings{}, metadataOf(), root,
                      basalt::wire::common::Logger{"Test"},      options};

    static_cast<void>(world.state().getOrGenerateChunk(0, 0));

    CHECK(calls == 1);
    CHECK(world.state().blockState(0, 0, 0).value_or(0) == reg::blocks::bedrock().stateId);
}
