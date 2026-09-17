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

/// What a world remembers about itself between restarts: `worlds/<name>/level.nbt`. The seed a
/// world was generated with is state, not configuration: left in a config file, editing it
/// regenerates different terrain next to the terrain already on disk.
///
/// NBT because nobody hand-edits it, and `level.nbt` rather than `level.dat` for the same reason
/// regions are `.linear`: it is not vanilla's format and should not claim to be.
struct WorldMetadata {
    static constexpr std::int32_t kFormatVersion = 1;

    /// Where a brand new world puts its spawn until real spawn selection exists.
    static constexpr std::array<std::int32_t, 3> kDefaultSpawn{8, 64, 8};

    std::int64_t seed = 0;
    std::int32_t spawnX = 0;
    std::int32_t spawnY = 0;
    std::int32_t spawnZ = 0;
    std::int64_t timeOfDay = 0;

    /// Ticks since the world was created, which is what `/time query gametime` answers with.
    std::int64_t gameTime = 0;

    /// The clock's rate and paused flag, as `/time rate` and `/time pause` leave them.
    float clockRate = 1.0F;
    bool clockPaused = false;
    bool raining = false;
    std::int32_t rainTime = 0;
    bool thundering = false;
    std::int32_t thunderTime = 0;
    std::int32_t clearWeatherTime = 0;
    GameRuleValues gameRules;

    /// What `/difficulty` and `/defaultgamemode` last set, or nothing while nobody has: the world
    /// then follows `worlds.toml` and `server.toml`.
    ///
    /// parity: vanilla stores both in `level.dat`, and that file wins over `server.properties` for
    /// exactly this reason: an in-game change has to outlive a restart without editing
    /// configuration the operator owns.
    std::optional<std::string> difficulty;
    bool difficultyLocked = false;
    std::optional<std::string> defaultGamemode;

    /// parity: `TicketType.FORCED` has `persist()`, so `/forceload` survives a restart: the jar
    /// writes its tickets as the `chunk_tickets` saved data. Packed the way the generation queue
    /// packs a chunk key.
    std::vector<std::int64_t> forcedChunks;

    /// parity: `WanderingTraderData`, persisted independently of the spawner's 1,200-tick counter.
    std::int32_t wanderingTraderSpawnDelay = 24000;
    std::int32_t wanderingTraderSpawnChance = 25;

    /// parity: the `ender_dragon_fight` saved data, kept only by a world that has the fight.
    std::optional<wire::nbt::NbtCompound> dragonFight;

    friend bool operator==(const WorldMetadata&, const WorldMetadata&) = default;

    [[nodiscard]] static std::string pathFor(const std::string& worldRoot) { return worldRoot + "/level.nbt"; }

    bool writeTo(const std::string& worldRoot) const;

    [[nodiscard]] static std::optional<WorldMetadata> read(const std::string& worldRoot);

    /// The metadata for a world that is about to be loaded, creating it on first use.
    ///
    /// `declaredSeed` is what `worlds.toml` asks for; the file is what the world was actually
    /// generated with. **The file wins**, because the terrain on disk was made with it: changing
    /// the seed of a world that already exists is an operator mistake, not an instruction, and
    /// obeying it would generate mismatched terrain against the chunks already saved. Saying so in
    /// the log is the useful thing to do, not silently picking either one.
    [[nodiscard]] static WorldMetadata loadOrCreate(const std::string& worldRoot,
                                                    std::optional<std::int64_t> declaredSeed,
                                                    std::array<std::int32_t, 3> defaultSpawn,
                                                    const wire::common::Logger& logger);
};

} // namespace basalt::server::runtime::world
