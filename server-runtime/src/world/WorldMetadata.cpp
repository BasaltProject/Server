#include <basalt/server/runtime/world/WorldMetadata.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

constexpr std::string_view kVersionKey = "Version";

} // namespace

bool WorldMetadata::writeTo(const std::string& worldRoot) const {
    wire::nbt::NbtCompound root;
    root.putInt(kVersionKey, kFormatVersion);
    root.putLong("Seed", seed);
    root.putInt("SpawnX", spawnX);
    root.putInt("SpawnY", spawnY);
    root.putInt("SpawnZ", spawnZ);
    root.putLong("Time", timeOfDay);
    root.putLong("GameTime", gameTime);
    root.putFloat("ClockRate", clockRate);
    root.putBoolean("ClockPaused", clockPaused);
    root.putBoolean("Raining", raining);
    root.putInt("RainTime", rainTime);
    root.putBoolean("Thundering", thundering);
    root.putInt("ThunderTime", thunderTime);
    root.putInt("ClearWeatherTime", clearWeatherTime);
    if (difficulty.has_value()) {
        root.putString("Difficulty", *difficulty);
    }
    root.putBoolean("DifficultyLocked", difficultyLocked);
    if (defaultGamemode.has_value()) {
        root.putString("DefaultGamemode", *defaultGamemode);
    }
    wire::nbt::NbtCompound trader;
    if (!forcedChunks.empty()) {
        root.set("ForcedChunks", wire::nbt::NbtTag::ofLongArray(forcedChunks));
    }
    trader.putInt("spawn_delay", wanderingTraderSpawnDelay);
    trader.putInt("spawn_chance", wanderingTraderSpawnChance);
    root.set("WanderingTrader", wire::nbt::NbtTag::ofCompound(std::move(trader)));
    if (dragonFight.has_value()) {
        root.set("DragonFight", wire::nbt::NbtTag::ofCompound(*dragonFight));
    }
    gameRules.writeTo(root);

    wire::io::fileIO::createDirectories(worldRoot);
    return wire::io::fileIO::writeBytes(pathFor(worldRoot),
                                        wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(root))));
}

std::optional<WorldMetadata> WorldMetadata::read(const std::string& worldRoot) {
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(pathFor(worldRoot));
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> named = wire::nbt::nbtIO::read(*bytes);
    if (!named.has_value()) {
        return std::nullopt;
    }
    const wire::nbt::NbtCompound* root = named->tag.get<wire::nbt::NbtCompound>();
    if (root == nullptr) {
        return std::nullopt;
    }
    const std::int32_t version = root->getInt(kVersionKey);
    if (version != kFormatVersion) {
        throw std::runtime_error{pathFor(worldRoot) + " is format version " + std::to_string(version) +
                                 ", this build reads " + std::to_string(kFormatVersion)};
    }

    WorldMetadata metadata;
    metadata.seed = root->getLong("Seed");
    metadata.spawnX = root->getInt("SpawnX");
    metadata.spawnY = root->getInt("SpawnY");
    metadata.spawnZ = root->getInt("SpawnZ");
    metadata.timeOfDay = root->getLong("Time");
    metadata.gameTime = root->getLong("GameTime");
    // A world saved before the clock could be slowed runs at the normal rate, not at a stop.
    metadata.clockRate = root->contains("ClockRate") ? root->getFloat("ClockRate") : 1.0F;
    metadata.clockPaused = root->getBoolean("ClockPaused");
    metadata.raining = root->getBoolean("Raining");
    metadata.rainTime = root->getInt("RainTime");
    metadata.thundering = root->getBoolean("Thundering");
    metadata.thunderTime = root->getInt("ThunderTime");
    metadata.clearWeatherTime = root->getInt("ClearWeatherTime");
    metadata.gameRules = GameRuleValues::readFrom(*root);
    if (root->contains("Difficulty")) {
        metadata.difficulty = std::string{root->getString("Difficulty")};
    }
    metadata.difficultyLocked = root->getBoolean("DifficultyLocked");
    if (root->contains("DefaultGamemode")) {
        metadata.defaultGamemode = std::string{root->getString("DefaultGamemode")};
    }
    if (root->contains("ForcedChunks")) {
        const std::span<const std::int64_t> forced = root->getLongArray("ForcedChunks");
        metadata.forcedChunks.assign(forced.begin(), forced.end());
    }
    if (const wire::nbt::NbtCompound* trader = root->getCompound("WanderingTrader"); trader != nullptr) {
        metadata.wanderingTraderSpawnDelay = trader->getInt("spawn_delay", 24000);
        metadata.wanderingTraderSpawnChance = trader->getInt("spawn_chance", 25);
    }
    if (const wire::nbt::NbtCompound* fight = root->getCompound("DragonFight"); fight != nullptr) {
        metadata.dragonFight = *fight;
    }
    return metadata;
}

WorldMetadata WorldMetadata::loadOrCreate(const std::string& worldRoot, std::optional<std::int64_t> declaredSeed,
                                          std::array<std::int32_t, 3> defaultSpawn,
                                          const wire::common::Logger& logger) {
    if (std::optional<WorldMetadata> existing = read(worldRoot); existing.has_value()) {
        if (declaredSeed.has_value() && *declaredSeed != existing->seed) {
            logger.warn([&worldRoot, declaredSeed, &existing] {
                return "Configured seed " + std::to_string(*declaredSeed) + " for '" + worldRoot +
                       "' does not match the seed it was generated with (" + std::to_string(existing->seed) +
                       "); keeping the generated one. Delete the world directory to regenerate it with the "
                       "configured seed.";
            });
        }
        return *std::move(existing);
    }

    if (std::optional<WorldMetadata> vanilla = fromVanilla(worldRoot); vanilla.has_value()) {
        logger.info([&worldRoot] { return "Imported the settings of the vanilla world in '" + worldRoot + "'."; });
        if (declaredSeed.has_value() && *declaredSeed != vanilla->seed) {
            logger.warn([&worldRoot, &vanilla] {
                return "The vanilla world in '" + worldRoot + "' was generated with seed " +
                       std::to_string(vanilla->seed) + "; that one is kept.";
            });
        }
        static_cast<void>(vanilla->writeTo(worldRoot));
        return *std::move(vanilla);
    }

    std::int64_t seed = 0;
    if (declaredSeed.has_value()) {
        seed = *declaredSeed;
    } else {
        static thread_local std::mt19937_64 generator{std::random_device{}()};
        seed = static_cast<std::int64_t>(generator());
        logger.info([&worldRoot, seed] {
            return "Generating '" + worldRoot + "' with a random seed: " + std::to_string(seed) + ".";
        });
    }
    WorldMetadata created;
    created.seed = seed;
    created.spawnX = defaultSpawn[0];
    created.spawnY = defaultSpawn[1];
    created.spawnZ = defaultSpawn[2];
    static_cast<void>(created.writeTo(worldRoot));
    return created;
}

} // namespace basalt::server::runtime::world
