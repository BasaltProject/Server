#include <basalt/metamorph/datafix/LevelData.hpp>
#include <basalt/server/runtime/world/SavedDataFiles.hpp>
#include <basalt/server/runtime/world/WorldMetadata.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <array>
#include <string>

namespace basalt::server::runtime::world {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

[[nodiscard]] std::optional<NbtCompound> readGzipped(const std::string& path) {
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<std::vector<std::uint8_t>, wire::io::CompressionError> plain =
        wire::io::gzip::decompress(*bytes);
    if (!plain.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> parsed = wire::nbt::nbtIO::read(*plain);
    const NbtCompound* root = parsed.has_value() ? parsed->tag.get<NbtCompound>() : nullptr;
    return root == nullptr ? std::nullopt : std::optional{*root};
}

[[nodiscard]] std::optional<NbtCompound> saved(const std::string& worldRoot, std::string_view name,
                                               std::string_view dimension = {}) {
    return world::savedData::read(worldRoot, name, dimension);
}

[[nodiscard]] std::string gamemodeName(std::int32_t id) {
    static constexpr std::array<std::string_view, 4> kNames{"survival", "creative", "adventure", "spectator"};
    return std::string{kNames[static_cast<std::size_t>(id >= 0 && id < 4 ? id : 0)]};
}

} // namespace

std::optional<WorldMetadata> WorldMetadata::fromVanilla(const std::string& worldRoot) {
    const std::optional<NbtCompound> levelFile = readGzipped(worldRoot + "/level.dat");
    const std::optional<NbtCompound> settings = saved(worldRoot, "world_gen_settings");
    if (!levelFile.has_value() && !settings.has_value()) {
        return std::nullopt;
    }
    const NbtCompound* level = levelFile.has_value() ? levelFile->getCompound("Data") : nullptr;

    WorldMetadata metadata;
    // parity: `world_gen_settings` since the level data moved to saved data, `Data.WorldGenSettings` before.
    if (settings.has_value()) {
        metadata.seed = settings->getLong("seed");
    } else if (const NbtCompound* old = level == nullptr ? nullptr : level->getCompound("WorldGenSettings");
               old != nullptr) {
        metadata.seed = old->getLong("seed");
    }
    if (level != nullptr) {
        if (const NbtCompound* spawn = level->getCompound("spawn"); spawn != nullptr) {
            const auto position = spawn->getIntArray("pos");
            if (position.size() == 3) {
                metadata.spawnX = position[0];
                metadata.spawnY = position[1];
                metadata.spawnZ = position[2];
            }
        } else {
            metadata.spawnX = level->getInt("SpawnX");
            metadata.spawnY = level->getInt("SpawnY");
            metadata.spawnZ = level->getInt("SpawnZ");
        }
        metadata.gameTime = level->getLong("Time");
        metadata.timeOfDay = level->getLong("DayTime");
        if (const NbtCompound* difficulty = level->getCompound("difficulty_settings"); difficulty != nullptr) {
            metadata.difficulty = std::string{difficulty->getString("difficulty")};
            metadata.difficultyLocked = difficulty->getBoolean("locked");
        } else if (level->contains("Difficulty")) {
            static constexpr std::array<std::string_view, 4> kDifficulties{"peaceful", "easy", "normal", "hard"};
            metadata.difficulty =
                std::string{kDifficulties[static_cast<std::size_t>(level->getByte("Difficulty") & 3)]};
            metadata.difficultyLocked = level->getBoolean("DifficultyLocked");
        }
        if (level->contains("GameType")) {
            metadata.defaultGamemode = gamemodeName(level->getInt("GameType"));
        }
        metadata.raining = level->getBoolean("raining");
        metadata.rainTime = level->getInt("rainTime");
        metadata.thundering = level->getBoolean("thundering");
        metadata.thunderTime = level->getInt("thunderTime");
        metadata.clearWeatherTime = level->getInt("clearWeatherTime");
    }
    if (std::optional<NbtCompound> fight = saved(worldRoot, "ender_dragon_fight", "the_end"); fight.has_value()) {
        metadata.dragonFight = std::move(*fight);
    } else if (const NbtCompound* old = level == nullptr ? nullptr : level->getCompound("DragonFight");
               old != nullptr) {
        metadata.dragonFight = metamorph::datafix::dragonFightOfLegacy(*old);
    }
    if (const std::optional<NbtCompound> trader = saved(worldRoot, "wandering_trader"); trader.has_value()) {
        metadata.wanderingTraderSpawnDelay = trader->getInt("spawn_delay", 24000);
        metadata.wanderingTraderSpawnChance = trader->getInt("spawn_chance", 25);
    } else if (level != nullptr) {
        metadata.wanderingTraderSpawnDelay = level->getInt("WanderingTraderSpawnDelay", 24000);
        metadata.wanderingTraderSpawnChance = level->getInt("WanderingTraderSpawnChance", 25);
    }
    if (std::optional<NbtCompound> border = saved(worldRoot, "world_border", "overworld"); border.has_value()) {
        metadata.worldBorder = std::move(*border);
    } else if (level != nullptr && level->contains("BorderSize")) {
        metadata.worldBorder = metamorph::datafix::worldBorderOfLegacy(*level);
    }
    if (const std::optional<NbtCompound> clocks = saved(worldRoot, "world_clocks"); clocks.has_value()) {
        if (const NbtCompound* overworld = clocks->getCompound("minecraft:overworld"); overworld != nullptr) {
            metadata.timeOfDay = overworld->getLong("total_ticks");
        }
    }
    if (const std::optional<NbtCompound> weather = saved(worldRoot, "weather"); weather.has_value()) {
        metadata.raining = weather->getBoolean("raining");
        metadata.rainTime = weather->getInt("rain_time");
        metadata.thundering = weather->getBoolean("thundering");
        metadata.thunderTime = weather->getInt("thunder_time");
        metadata.clearWeatherTime = weather->getInt("clear_weather_time");
    }
    std::optional<NbtCompound> rules = saved(worldRoot, "game_rules");
    if (!rules.has_value() && level != nullptr) {
        // parity: the rules lived in `level.dat`, as strings under `GameRules` until `GameRuleRegistryFix`.
        if (const NbtCompound* current = level->getCompound("game_rules"); current != nullptr) {
            rules = *current;
        } else if (const NbtCompound* old = level->getCompound("GameRules"); old != nullptr) {
            rules = metamorph::datafix::gameRulesOfLegacy(*old);
        }
    }
    if (rules.has_value()) {
        for (std::size_t i = 0; i < rules->size(); ++i) {
            const GameRule* rule = gameRules::byId(rules->names()[i]);
            if (rule == nullptr) {
                continue;
            }
            const NbtTag& value = rules->values()[i];
            if (rule->kind == GameRule::Kind::Boolean) {
                metadata.gameRules.set(*rule, rules->getBoolean(rules->names()[i]) ? "true" : "false");
            } else if (const auto* number = value.get<std::int32_t>(); number != nullptr) {
                metadata.gameRules.set(*rule, std::to_string(*number));
            }
        }
    }
    return metadata;
}

} // namespace basalt::server::runtime::world
