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

/// The `data` compound of a 26.x saved-data file, the file being `data/minecraft/<name>.dat`.
[[nodiscard]] std::optional<NbtCompound> savedData(const std::string& worldRoot, std::string_view name) {
    const std::optional<NbtCompound> file = readGzipped(worldRoot + "/data/minecraft/" + std::string{name} + ".dat");
    const NbtCompound* data = file.has_value() ? file->getCompound("data") : nullptr;
    return data == nullptr ? std::nullopt : std::optional{*data};
}

[[nodiscard]] std::string gamemodeName(std::int32_t id) {
    static constexpr std::array<std::string_view, 4> kNames{"survival", "creative", "adventure", "spectator"};
    return std::string{kNames[static_cast<std::size_t>(id >= 0 && id < 4 ? id : 0)]};
}

} // namespace

std::optional<WorldMetadata> WorldMetadata::fromVanilla(const std::string& worldRoot) {
    const std::optional<NbtCompound> levelFile = readGzipped(worldRoot + "/level.dat");
    const std::optional<NbtCompound> settings = savedData(worldRoot, "world_gen_settings");
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
    if (const std::optional<NbtCompound> clocks = savedData(worldRoot, "world_clocks"); clocks.has_value()) {
        if (const NbtCompound* overworld = clocks->getCompound("minecraft:overworld"); overworld != nullptr) {
            metadata.timeOfDay = overworld->getLong("total_ticks");
        }
    }
    if (const std::optional<NbtCompound> weather = savedData(worldRoot, "weather"); weather.has_value()) {
        metadata.raining = weather->getBoolean("raining");
        metadata.rainTime = weather->getInt("rain_time");
        metadata.thundering = weather->getBoolean("thundering");
        metadata.thunderTime = weather->getInt("thunder_time");
        metadata.clearWeatherTime = weather->getInt("clear_weather_time");
    }
    if (const std::optional<NbtCompound> rules = savedData(worldRoot, "game_rules"); rules.has_value()) {
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
