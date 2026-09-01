#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/WorldMetadata.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <doctest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

using namespace basalt::server::runtime::world;
namespace fileIO = basalt::wire::io::fileIO;
namespace nbt = basalt::wire::nbt;
using basalt::wire::common::Logger;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] WorldMetadata filled() {
    WorldMetadata metadata;
    metadata.seed = -3218976543210LL;
    metadata.spawnX = -120;
    metadata.spawnY = 71;
    metadata.spawnZ = 340;
    metadata.timeOfDay = 13000;
    metadata.gameTime = 987654;
    metadata.clockRate = 0.5F;
    metadata.clockPaused = true;
    metadata.raining = true;
    metadata.rainTime = 1200;
    metadata.thundering = true;
    metadata.thunderTime = 300;
    metadata.clearWeatherTime = 42;
    metadata.difficulty = "hard";
    metadata.difficultyLocked = true;
    metadata.defaultGamemode = "adventure";
    metadata.wanderingTraderSpawnDelay = 6000;
    metadata.wanderingTraderSpawnChance = 75;
    metadata.gameRules.set(gameRules::kKeepInventory, "true");
    return metadata;
}

} // namespace

TEST_CASE("everything a world remembers survives the trip to disk") {
    const std::string root = scratch("world-metadata");
    const WorldMetadata written = filled();
    CHECK(written.writeTo(root));
    CHECK(fileIO::exists(root + "/level.nbt"));
    CHECK(WorldMetadata::pathFor(root) == root + "/level.nbt");

    const std::optional<WorldMetadata> read = WorldMetadata::read(root);
    REQUIRE(read.has_value());
    CHECK(*read == written);
    CHECK(read->seed == -3218976543210LL);
    CHECK(read->clockRate == doctest::Approx(0.5));
    CHECK(read->gameRules.get(gameRules::kKeepInventory));
    CHECK(read->difficulty == "hard");
    CHECK(read->defaultGamemode == "adventure");
    CHECK(read->wanderingTraderSpawnChance == 75);

    // Written where a world lives rather than beside it, and the directory made if it is not there.
    CHECK(fileIO::exists(root));
}

TEST_CASE("a world nobody has changed says nothing rather than the defaults") {
    const std::string root = scratch("world-metadata-empty");
    WorldMetadata metadata;
    metadata.seed = 7;
    CHECK(metadata.writeTo(root));

    const std::optional<std::vector<std::uint8_t>> bytes = fileIO::readBytes(WorldMetadata::pathFor(root));
    REQUIRE(bytes.has_value());
    const std::expected<nbt::NamedTag, nbt::NbtError> named = nbt::nbtIO::read(*bytes);
    REQUIRE(named.has_value());
    const nbt::NbtCompound* stored = named->tag.get<nbt::NbtCompound>();
    REQUIRE(stored != nullptr);
    // What `/difficulty` never set is absent, which is what lets `worlds.toml` keep deciding it.
    CHECK_FALSE(stored->contains("Difficulty"));
    CHECK_FALSE(stored->contains("DefaultGamemode"));
    CHECK_FALSE(stored->contains("GameRules"));
    CHECK(stored->getInt("Version") == WorldMetadata::kFormatVersion);

    const std::optional<WorldMetadata> read = WorldMetadata::read(root);
    REQUIRE(read.has_value());
    CHECK_FALSE(read->difficulty.has_value());
    CHECK_FALSE(read->defaultGamemode.has_value());
    CHECK(*read == metadata);
}

TEST_CASE("a world that is not there reads as nothing, and one written by a newer build refuses") {
    const std::string root = scratch("world-metadata-missing");
    CHECK_FALSE(WorldMetadata::read(root).has_value());

    // Not NBT at all: a truncated or corrupted file is nothing rather than a crash.
    fileIO::createDirectories(root);
    fileIO::writeBytes(WorldMetadata::pathFor(root), std::vector<std::uint8_t>{1, 2, 3});
    CHECK_FALSE(WorldMetadata::read(root).has_value());

    // A file from a build that writes more than this one reads is refused rather than overwritten:
    // whoever wrote it knows more about the world on disk than this process does.
    nbt::NbtCompound newer;
    newer.putInt("Version", WorldMetadata::kFormatVersion + 1);
    newer.putLong("Seed", 5);
    fileIO::writeBytes(WorldMetadata::pathFor(root), nbt::nbtIO::write(nbt::NbtTag::ofCompound(std::move(newer))));
    CHECK_THROWS_AS(static_cast<void>(WorldMetadata::read(root)), std::runtime_error);
}

TEST_CASE("a world older than the clock runs at the normal rate") {
    const std::string root = scratch("world-metadata-old");
    nbt::NbtCompound old;
    old.putInt("Version", WorldMetadata::kFormatVersion);
    old.putLong("Seed", 11);
    fileIO::createDirectories(root);
    fileIO::writeBytes(WorldMetadata::pathFor(root), nbt::nbtIO::write(nbt::NbtTag::ofCompound(std::move(old))));

    const std::optional<WorldMetadata> read = WorldMetadata::read(root);
    REQUIRE(read.has_value());
    // A missing rate is one, not zero: a world saved before the clock could be slowed still runs.
    CHECK(read->clockRate == doctest::Approx(1.0));
    // The trader's own counters have their own defaults for the same reason.
    CHECK(read->wanderingTraderSpawnDelay == 24000);
    CHECK(read->wanderingTraderSpawnChance == 25);

    // And a half-written trader, from a build that stored one of the two, keeps the default for
    // the other rather than a zero delay that spawns a trader on the next tick.
    nbt::NbtCompound half;
    half.putInt("Version", WorldMetadata::kFormatVersion);
    nbt::NbtCompound trader;
    trader.putInt("spawn_chance", 90);
    half.set("WanderingTrader", nbt::NbtTag::ofCompound(std::move(trader)));
    fileIO::writeBytes(WorldMetadata::pathFor(root), nbt::nbtIO::write(nbt::NbtTag::ofCompound(std::move(half))));

    const std::optional<WorldMetadata> partial = WorldMetadata::read(root);
    REQUIRE(partial.has_value());
    CHECK(partial->wanderingTraderSpawnDelay == 24000);
    CHECK(partial->wanderingTraderSpawnChance == 90);
}

TEST_CASE("the seed on disk wins over the seed in the configuration") {
    const std::string root = scratch("world-metadata-seed");
    const Logger logger{"World"};

    // First run: nothing on disk, so the configured seed is what the world is generated with.
    const WorldMetadata created = WorldMetadata::loadOrCreate(root, 12345, WorldMetadata::kDefaultSpawn, logger);
    CHECK(created.seed == 12345);
    CHECK(created.spawnX == 8);
    CHECK(created.spawnY == 64);
    CHECK(created.spawnZ == 8);
    // Written on creation, so the next run finds it.
    CHECK(fileIO::exists(WorldMetadata::pathFor(root)));

    // Second run with a different seed in the configuration: the terrain already on disk was made
    // with the old one, so the old one stays.
    const WorldMetadata again = WorldMetadata::loadOrCreate(root, 999, WorldMetadata::kDefaultSpawn, logger);
    CHECK(again.seed == 12345);
    CHECK(again == created);

    // And with no seed configured at all, still the one the world has.
    CHECK(WorldMetadata::loadOrCreate(root, std::nullopt, WorldMetadata::kDefaultSpawn, logger).seed == 12345);
}

TEST_CASE("a world with no seed configured gets one of its own") {
    const Logger logger{"World"};
    const WorldMetadata first =
        WorldMetadata::loadOrCreate(scratch("world-metadata-random-a"), std::nullopt, {1, 2, 3}, logger);
    const WorldMetadata second =
        WorldMetadata::loadOrCreate(scratch("world-metadata-random-b"), std::nullopt, {1, 2, 3}, logger);
    // Two worlds created without a seed are two different worlds.
    CHECK(first.seed != second.seed);
    // The spawn asked for is the spawn stored, not the default one.
    CHECK(first.spawnX == 1);
    CHECK(first.spawnY == 2);
    CHECK(first.spawnZ == 3);
}
