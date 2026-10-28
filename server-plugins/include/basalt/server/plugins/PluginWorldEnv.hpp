#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginParticle {
    std::string type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::int32_t count = 1;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    float offsetZ = 0.0F;
    std::array<float, 3> speed{};
    /// 0 default, 1 alternative, 2 alternative with speed (`ClientboundLevelParticlesPacket.RandomizationType`).
    std::int32_t randomization = 0;
    bool force = false;
    /// Numbers the particle's options read in order: a state id, a colour, a scale, a target.
    std::vector<double> data;
    std::string item;
    /// The player who alone sees it, or empty for everyone in range.
    std::string receiver;
};

struct PluginBorder {
    double centerX = 0.0;
    double centerZ = 0.0;
    double size = 0.0;
    double targetSize = 0.0;
    std::int64_t lerpTicks = 0;
    double damagePerBlock = 0.0;
    double damageBuffer = 0.0;
    std::int32_t warningBlocks = 0;
    std::int32_t warningSeconds = 0;
};

struct PluginDrop {
    std::string item;
    std::int32_t count = 0;
    /// The stack's components as SNBT.
    std::string components;
};

struct PluginStructureStart {
    std::string structure;
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    /// min x y z, then max x y z, in blocks and inclusive.
    std::array<std::int32_t, 6> bounds{};
    std::vector<std::array<std::int32_t, 6>> pieces;
};

struct PluginRaid {
    std::int32_t id = 0;
    bool started = false;
    std::int64_t ticksActive = 0;
    std::int32_t omenLevel = 0;
    std::array<std::int32_t, 3> center{};
    std::string status;
    std::int32_t groupsSpawned = 0;
    std::int32_t totalGroups = 0;
    float totalHealth = 0.0F;
    std::vector<std::string> heroes;
    std::vector<std::int32_t> raiders;
    std::string barTitle;
    std::int32_t barColor = 2;
    std::int32_t barStyle = 2;
    std::int32_t barFlags = 0;
    float barProgress = 0.0F;
    bool barVisible = true;
    std::vector<std::int32_t> barPlayers;
};

struct PluginFluid {
    /// empty, water or lava.
    std::string fluid = "empty";
    bool source = false;
    std::int32_t level = 0;
    bool falling = false;
};

struct PluginPoi {
    std::string type;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t freeTickets = 0;
};

struct PluginPlacedBlock {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t before = 0;
    std::int32_t after = 0;
};

/// What a plugin changed about natural spawning; under zero is vanilla's. Indexed by `MobCategory` order.
struct PluginSpawnTunables {
    std::array<std::int32_t, 8> limits{-1, -1, -1, -1, -1, -1, -1, -1};
    std::array<std::int32_t, 8> ticksPer{-1, -1, -1, -1, -1, -1, -1, -1};
    bool allowMonsters = true;
    bool allowAnimals = true;
};

/// What a plugin can do to the physical world around it that the block and entity services do not cover.
class PluginWorldEnv {
public:
    PluginWorldEnv() = default;
    PluginWorldEnv(const PluginWorldEnv&) = delete;
    PluginWorldEnv& operator=(const PluginWorldEnv&) = delete;
    PluginWorldEnv(PluginWorldEnv&&) = delete;
    PluginWorldEnv& operator=(PluginWorldEnv&&) = delete;
    virtual ~PluginWorldEnv() = default;

    /// Plays a sound id to the players in range of the point, 16 blocks scaled by the volume.
    virtual void sound(std::string_view sound, std::string_view category, double x, double y, double z, float volume,
                       float pitch, std::int64_t seed) {
        static_cast<void>(sound);
        static_cast<void>(category);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(volume);
        static_cast<void>(pitch);
        static_cast<void>(seed);
    }

    virtual void particle(const PluginParticle& particle) { static_cast<void>(particle); }

    /// A level event (a sound or a burst of particles the client knows by number); `global` reaches every player.
    virtual void levelEvent(std::int32_t type, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t data,
                            bool global) {
        static_cast<void>(type);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(data);
        static_cast<void>(global);
    }

    /// A block event (a chest lid, a bell, a note) sent to the players watching the block.
    virtual void blockEvent(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t first, std::int32_t second) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(first);
        static_cast<void>(second);
    }

    /// "shriek" makes the shrieker at the position shriek for a player uuid; "bloom" makes the catalyst spread.
    virtual bool sculk(std::string_view action, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t sourceX,
                       std::int32_t sourceY, std::int32_t sourceZ, std::int32_t value, std::string_view player) {
        static_cast<void>(action);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(sourceX);
        static_cast<void>(sourceY);
        static_cast<void>(sourceZ);
        static_cast<void>(value);
        static_cast<void>(player);
        return false;
    }

    /// "spawn" answers the new creaking's uuid, "resin" answers the clump's "x,y,z"; empty when nothing happened.
    [[nodiscard]] virtual std::string creakingHeart(std::string_view action, std::int32_t x, std::int32_t y,
                                                    std::int32_t z) {
        static_cast<void>(action);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return {};
    }

    /// Edits a raid's boss bar by action: "bar_title", "bar_color", "bar_progress", "bar_add", ...
    virtual bool raidBar(std::int32_t id, std::string_view action, std::string_view text, std::int32_t value) {
        static_cast<void>(id);
        static_cast<void>(action);
        static_cast<void>(text);
        static_cast<void>(value);
        return false;
    }

    /// The values a plugin keeps on a raid, as a named compound; empty when there is none.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> raidValues(std::int32_t id) {
        static_cast<void>(id);
        return std::nullopt;
    }

    virtual bool setRaidValues(std::int32_t id, std::span<const std::uint8_t> nbt) {
        static_cast<void>(id);
        static_cast<void>(nbt);
        return false;
    }

    [[nodiscard]] virtual PluginBorder border() { return {}; }

    /// Applies the border settings that differ from the current ones; the size moves over `seconds` when positive.
    virtual void setBorder(const PluginBorder& border, double seconds) {
        static_cast<void>(border);
        static_cast<void>(seconds);
    }

    [[nodiscard]] virtual std::int32_t playerCount() { return 0; }

    [[nodiscard]] virtual std::int32_t blockEntityCount() { return 0; }

    /// What the block at the position drops when broken with `tool` (an item id, empty for a hand), no stack spawned.
    [[nodiscard]] virtual std::optional<std::vector<PluginDrop>>
    blockDrops(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tool,
               const std::vector<std::pair<std::string, std::int32_t>>& enchantments, bool byEntity) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(tool);
        static_cast<void>(enchantments);
        static_cast<void>(byEntity);
        return std::nullopt;
    }

    /// Breaks the block as a player would and drops what it gives; false when nothing broke.
    virtual bool breakBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tool,
                            const std::vector<std::pair<std::string, std::int32_t>>& enchantments, bool drops) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(tool);
        static_cast<void>(enchantments);
        static_cast<void>(drops);
        return false;
    }

    /// Spends bone meal on the block as a dispenser would; false when nothing there grows.
    virtual bool boneMeal(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return false;
    }

    /// The redstone signal a block gives toward `face` (0 to 5, down up north south west east), 0 to 15.
    [[nodiscard]] virtual std::int32_t signalFrom(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face,
                                                  bool direct) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(face);
        static_cast<void>(direct);
        return 0;
    }

    /// The climate humidity (downfall) of the biome at a point.
    [[nodiscard]] virtual double humidity(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return 0.4;
    }

    /// The climate temperature at a point, as the biome there gives it at that height.
    [[nodiscard]] virtual double temperature(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return 0.8;
    }

    /// How much of the block a player mines per tick with what they hold, `ServerPlayer.getDestroyProgress`.
    [[nodiscard]] virtual double breakSpeed(std::string_view player, std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(player);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return 0.0;
    }

    /// The structure starts that begin in a loaded chunk, with the box of each piece.
    [[nodiscard]] virtual std::vector<PluginStructureStart> structureStarts(std::int32_t chunkX, std::int32_t chunkZ) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        return {};
    }

    [[nodiscard]] virtual std::vector<PluginRaid> raids() { return {}; }

    /// Changes the omen level and the number of waves of a raid; false when there is no such raid.
    virtual bool setRaid(std::int32_t id, std::int32_t omenLevel, std::int32_t totalGroups) {
        static_cast<void>(id);
        static_cast<void>(omenLevel);
        static_cast<void>(totalGroups);
        return false;
    }

    [[nodiscard]] virtual PluginFluid fluidAt(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return {};
    }

    /// The unit vector a fluid at the block pushes things along; zero when there is no fluid or no current.
    [[nodiscard]] virtual std::array<double, 3> fluidFlow(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return {};
    }

    /// Where lightning striking near the column would land: a lightning rod in range, else the highest point.
    [[nodiscard]] virtual std::optional<std::array<std::int32_t, 3>> lightningTarget(std::int32_t x, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(z);
        return std::nullopt;
    }

    /// Raises a game event that sculk sensors in range hear; `source` is an entity uuid or empty.
    virtual void gameEvent(std::string_view event, double x, double y, double z, std::string_view source) {
        static_cast<void>(event);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(source);
    }

    /// Grows the tree of a sapling block (`oak_sapling`...) at the position; false when no tree fits.
    virtual bool growTree(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view sapling) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(sapling);
        return false;
    }

    /// Plants a named tree as the world generator would; with `apply` false it only returns the blocks it would set.
    [[nodiscard]] virtual std::optional<std::vector<PluginPlacedBlock>>
    placeTree(std::string_view tree, std::int32_t x, std::int32_t y, std::int32_t z, bool apply) {
        static_cast<void>(tree);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(apply);
        return std::nullopt;
    }

    /// One random tick on the block, as the random tick speed gives; true when something changed.
    virtual bool randomTick(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return false;
    }

    /// Loaded block entities that run code every tick (furnaces, hoppers, spawners, beacons...).
    [[nodiscard]] virtual std::int32_t tickableBlockEntityCount() { return 0; }

    /// Points of interest of the named types (empty: all) within `radius`; `occupancy` 0 free, 1 taken, 2 any.
    [[nodiscard]] virtual std::vector<PluginPoi> pointsOfInterest(const std::vector<std::string>& types, std::int32_t x,
                                                                  std::int32_t y, std::int32_t z, std::int32_t radius,
                                                                  std::int32_t occupancy) {
        static_cast<void>(types);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(radius);
        static_cast<void>(occupancy);
        return {};
    }

    [[nodiscard]] virtual PluginSpawnTunables spawnTunables() { return {}; }

    virtual void setSpawnTunables(const PluginSpawnTunables& tunables) { static_cast<void>(tunables); }

    /// Void damage (`void_damage` 0 or 1, `void_damage_amount`, `void_damage_offset`) and `simulation_distance`.
    [[nodiscard]] virtual std::optional<double> tunable(std::string_view key) {
        static_cast<void>(key);
        return std::nullopt;
    }

    virtual bool setTunable(std::string_view key, double value) {
        static_cast<void>(key);
        static_cast<void>(value);
        return false;
    }

    [[nodiscard]] virtual std::int32_t viewDistance() { return 10; }

    virtual void setViewDistance(std::int32_t chunks) { static_cast<void>(chunks); }

    [[nodiscard]] virtual std::int32_t seaLevel() { return 63; }

    /// The chunks kept loaded by `/forceload`, as x and z pairs.
    [[nodiscard]] virtual std::vector<std::int32_t> forcedChunks() { return {}; }

    virtual void setChunkForced(std::int32_t chunkX, std::int32_t chunkZ, bool forced) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        static_cast<void>(forced);
    }

    /// Ticks left of the rain, of the thunder and of the clear weather a command asked for.
    [[nodiscard]] virtual std::array<std::int32_t, 3> weatherTimes() { return {0, 0, 0}; }

    virtual void setWeatherTimes(std::int32_t rain, std::int32_t thunder, std::int32_t clear) {
        static_cast<void>(rain);
        static_cast<void>(thunder);
        static_cast<void>(clear);
    }

    /// How long players have stood in the chunk, in ticks; zero for a chunk that is not loaded.
    [[nodiscard]] virtual std::int64_t inhabitedTime(std::int32_t chunkX, std::int32_t chunkZ) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        return 0;
    }

    virtual void setInhabitedTime(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t ticks) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        static_cast<void>(ticks);
    }

    /// Hands the chunks of a void world to `fill`: a state id per block of each section, a biome id per 4x4x4 cell.
    using ChunkFill = std::function<void(std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::int32_t>& blocks,
                                         std::vector<std::int32_t>& biomes)>;

    virtual bool useChunkFill(ChunkFill fill) {
        static_cast<void>(fill);
        return false;
    }

    /// What of the world's own generation stays when a plugin fills the chunks.
    struct ChunkFillTraits {
        /// `blocks` and `biomes` arrive holding the vanilla chunk and the plugin builds on it.
        bool vanillaTerrain = false;
        bool decorations = false;
        bool structures = false;
        bool mobs = false;
        /// With `vanillaTerrain`, which of its three steps still run.
        bool noise = true;
        bool surface = true;
        bool caves = true;
    };

    virtual bool useChunkFill(ChunkFill fill, const ChunkFillTraits& traits) {
        static_cast<void>(traits);
        return useChunkFill(std::move(fill));
    }

    /// Whether the state would survive standing at the position (`BlockState.canSurvive`).
    [[nodiscard]] virtual bool survives(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(stateId);
        return true;
    }
};

} // namespace basalt::server::plugins
