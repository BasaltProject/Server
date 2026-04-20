#pragma once

#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

struct WorldMetadata {
    static constexpr std::int32_t kFormatVersion = 1;

    static constexpr std::array<std::int32_t, 3> kDefaultSpawn{8, 64, 8};

    std::int64_t seed = 0;
    std::int32_t spawnX = 0;
    std::int32_t spawnY = 0;
    std::int32_t spawnZ = 0;
    std::int64_t timeOfDay = 0;

    std::int64_t gameTime = 0;

    float clockRate = 1.0F;
    bool clockPaused = false;
    bool raining = false;
    std::int32_t rainTime = 0;
    bool thundering = false;
    std::int32_t thunderTime = 0;
    std::int32_t clearWeatherTime = 0;
    GameRuleValues gameRules;

    /// parity: `level.dat`.
    std::optional<std::string> difficulty;
    bool difficultyLocked = false;
    std::optional<std::string> defaultGamemode;

    /// parity: `TicketType.FORCED`.
    std::vector<std::int64_t> forcedChunks;

    /// parity: `WanderingTraderData`.
    std::int32_t wanderingTraderSpawnDelay = 24000;
    std::int32_t wanderingTraderSpawnChance = 25;

    /// parity: the `ender_dragon_fight` saved data, kept only by a world that has the fight.
    std::optional<wire::nbt::NbtCompound> dragonFight;

    friend bool operator==(const WorldMetadata&, const WorldMetadata&) = default;

    [[nodiscard]] static std::string pathFor(const std::string& worldRoot) { return worldRoot + "/level.nbt"; }

    bool writeTo(const std::string& worldRoot) const;

    [[nodiscard]] static std::optional<WorldMetadata> read(const std::string& worldRoot);

    [[nodiscard]] static WorldMetadata loadOrCreate(const std::string& worldRoot,
                                                    std::optional<std::int64_t> declaredSeed,
                                                    std::array<std::int32_t, 3> defaultSpawn,
                                                    const wire::common::Logger& logger);
};

} // namespace basalt::server::runtime::world
