#pragma once

#include <basalt/server/plugins/PluginEntityOps.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginPlayerAccess.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
class PluginWindow;
class PluginWorldEnv;

class PluginWorldRef {
public:
    PluginWorldRef() = default;
    PluginWorldRef(const PluginWorldRef&) = delete;
    PluginWorldRef& operator=(const PluginWorldRef&) = delete;
    PluginWorldRef(PluginWorldRef&&) = delete;
    PluginWorldRef& operator=(PluginWorldRef&&) = delete;
    virtual ~PluginWorldRef() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    [[nodiscard]] virtual std::int64_t time() const = 0;

    [[nodiscard]] virtual bool raining() const = 0;

    [[nodiscard]] virtual bool thundering() const = 0;

    [[nodiscard]] virtual std::string difficulty() const = 0;

    [[nodiscard]] virtual std::optional<std::string> block(std::int32_t x, std::int32_t y, std::int32_t z) const = 0;

    /// The block with its properties, as `minecraft:oak_stairs[facing=east,half=top]`.
    [[nodiscard]] virtual std::optional<std::string> blockState(std::int32_t x, std::int32_t y,
                                                                std::int32_t z) const = 0;

    /// Takes an id with optional properties, as `blockState` returns them.
    virtual bool setBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view block) = 0;

    [[nodiscard]] virtual std::vector<std::shared_ptr<PluginEntityRef>> entities() const = 0;

    virtual std::shared_ptr<PluginHologramRef> hologram(std::string_view text, double x, double y, double z) = 0;

    virtual std::shared_ptr<PluginNpcRef> npc(std::string_view name, double x, double y, double z,
                                              std::function<void(const std::string&)> onClick) = 0;

    virtual std::shared_ptr<PluginEntityRef> spawn(std::string_view type, double x, double y, double z) = 0;

    /// Puts a stack (its NBT) on the ground; answers the new entity id, or -1.
    virtual std::int32_t dropItem(std::span<const std::uint8_t> stack, double x, double y, double z, double velocityX,
                                  double velocityY, double velocityZ) {
        static_cast<void>(stack);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(velocityX);
        static_cast<void>(velocityY);
        static_cast<void>(velocityZ);
        return -1;
    }

    /// An explosion of the given power; the source is an entity id or -1. Blocks are spared when `breakBlocks` is off.
    virtual bool explode(double x, double y, double z, float power, bool breakBlocks, std::int32_t sourceEntityId) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(power);
        static_cast<void>(breakBlocks);
        static_cast<void>(sourceEntityId);
        return false;
    }

    /// Like `spawn`, with a facing and the reason the spawn event reports.
    virtual std::shared_ptr<PluginEntityRef> spawnAs(std::string_view type, double x, double y, double z, float yaw,
                                                     std::string_view reason) {
        static_cast<void>(yaw);
        static_cast<void>(reason);
        return spawn(type, x, y, z);
    }

    [[nodiscard]] virtual std::optional<std::string> gameRule(std::string_view name) const = 0;

    virtual bool setGameRule(std::string_view name, std::string_view value) = 0;

    [[nodiscard]] virtual std::vector<std::string> gameRules() const = 0;

    /// The dimension id, as `minecraft:overworld`.
    [[nodiscard]] virtual std::string dimension() const { return {}; }

    [[nodiscard]] virtual std::int64_t seed() const { return 0; }

    [[nodiscard]] virtual std::int32_t minHeight() const { return 0; }

    /// The number of blocks from the lowest to the highest.
    [[nodiscard]] virtual std::int32_t height() const { return 0; }

    [[nodiscard]] virtual std::array<std::int32_t, 3> spawn() const { return {0, 64, 0}; }

    virtual void setSpawn(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
    }

    /// The time of day, 0 to 23999, which is not the same as the age of the world.
    [[nodiscard]] virtual std::int64_t dayTime() const { return 0; }

    virtual void setDayTime(std::int64_t ticks) { static_cast<void>(ticks); }

    virtual void setWeather(bool raining, bool thundering) {
        static_cast<void>(raining);
        static_cast<void>(thundering);
    }

    virtual bool setDifficulty(std::string_view name) {
        static_cast<void>(name);
        return false;
    }

    /// The block state id at a position, or -1 when the chunk is not loaded and `load` is false.
    [[nodiscard]] virtual std::int32_t stateAt(std::int32_t x, std::int32_t y, std::int32_t z, bool load) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(load);
        return -1;
    }

    /// `notify` tells the players near it; false is for a caller that resends the chunk itself.
    virtual bool setStateAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool notify) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(stateId);
        static_cast<void>(notify);
        return false;
    }

    /// Reads many positions at once: `positions` holds x, y, z triples and `out` one state id (or -1) per triple.
    virtual std::int32_t readStates(std::span<const std::int32_t> positions, std::span<std::int32_t> out, bool load) {
        static_cast<void>(positions);
        static_cast<void>(out);
        static_cast<void>(load);
        return 0;
    }

    /// Writes many positions at once, returns how many changed.
    virtual std::int32_t writeStates(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                                     bool notify) {
        static_cast<void>(positions);
        static_cast<void>(states);
        static_cast<void>(notify);
        return 0;
    }

    /// A 16 by 16 by 16 section as 4096 state ids ordered x fastest, then z, then y (index = y * 256 + z * 16 + x).
    virtual bool readSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, bool load,
                             std::span<std::int32_t> out) {
        static_cast<void>(chunkX);
        static_cast<void>(sectionY);
        static_cast<void>(chunkZ);
        static_cast<void>(load);
        static_cast<void>(out);
        return false;
    }

    virtual bool writeSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                              std::span<const std::int32_t> states, bool notify) {
        static_cast<void>(chunkX);
        static_cast<void>(sectionY);
        static_cast<void>(chunkZ);
        static_cast<void>(states);
        static_cast<void>(notify);
        return false;
    }

    /// Writes whole sections of one chunk (4096 ids each, negative keeps); returns blocks changed, -1 if out of range.
    virtual std::int32_t applySections(std::int32_t chunkX, std::int32_t chunkZ,
                                       std::span<const std::int32_t> sectionYs, std::span<const std::int32_t> states,
                                       bool resend, std::span<std::int32_t> previous) {
        static_cast<void>(previous);
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        static_cast<void>(sectionYs);
        static_cast<void>(states);
        static_cast<void>(resend);
        return -1;
    }

    /// Many chunks at once: (chunkX, chunkZ, sectionCount) triples, sectionYs, states; returns blocks changed or -1.
    virtual std::int32_t applyChunks(std::span<const std::int32_t> chunks, std::span<const std::int32_t> sectionYs,
                                     std::span<const std::int32_t> states, bool resend,
                                     std::span<std::int32_t> previous) {
        static_cast<void>(chunks);
        static_cast<void>(sectionYs);
        static_cast<void>(states);
        static_cast<void>(resend);
        static_cast<void>(previous);
        return -1;
    }

    /// Every section of a chunk, lowest first, 4096 ids each; false when the chunk is not there.
    virtual bool readSections(std::int32_t chunkX, std::int32_t chunkZ, bool load, std::span<std::int32_t> out) {
        constexpr std::size_t kBlocks = 4096;
        const std::int32_t first = minHeight() >> 4;
        const std::size_t count = static_cast<std::size_t>(std::max(height(), 0)) >> 4;
        if (count == 0 || out.size() < count * kBlocks) {
            return false;
        }
        for (std::size_t at = 0; at < count; ++at) {
            if (!readSection(chunkX, first + static_cast<std::int32_t>(at), chunkZ, load && at == 0,
                             out.subspan(at * kBlocks, kBlocks))) {
                return false;
            }
        }
        return true;
    }

    /// Where a chunk stands: 0 not loaded, 1 asked for and on its way, 2 loaded, 3 loaded and ticking.
    [[nodiscard]] virtual std::int32_t chunkStatus(std::int32_t chunkX, std::int32_t chunkZ) const {
        return chunkLoaded(chunkX, chunkZ) ? 2 : 0;
    }

    /// Asks for a chunk; `done` runs on the world's thread once loaded (false if never). Lower `priority` first.
    virtual void requestChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate, std::int32_t priority,
                              std::function<void(bool)> done) {
        static_cast<void>(priority);
        done(loadChunk(chunkX, chunkZ, generate));
    }

    /// Keeps chunks (x, z pairs) loaded for `token` until released; false when held chunks are not supported.
    virtual bool holdChunks(std::string_view token, std::span<const std::int32_t> chunks) {
        static_cast<void>(token);
        static_cast<void>(chunks);
        return false;
    }

    /// Lets go of everything `token` held.
    virtual void releaseChunks(std::string_view token) { static_cast<void>(token); }

    /// What a change of blocks sets off beyond the blocks themselves (bits of `effects`).
    struct BlockEffects {
        /// The neighbors are told one of their neighbors changed (`Level.updateNeighborsAt`).
        static constexpr std::uint32_t kNeighbors = 1;
        /// The blocks around reshape to fit the new one (a fence joining, stairs turning).
        static constexpr std::uint32_t kShapes = 2;
        /// The light there is worked out again.
        static constexpr std::uint32_t kLight = 4;
        /// What a container that is replaced holds spills out as items.
        static constexpr std::uint32_t kEntities = 8;
        /// Points of interest follow the change.
        static constexpr std::uint32_t kPoi = 16;
        /// The new block books its scheduled ticks and fluid updates (`onPlace`).
        static constexpr std::uint32_t kTicks = 32;
        /// The players who see the blocks are shown their new state.
        static constexpr std::uint32_t kClients = 64;
        /// The rest of `onPlace`: golems, dust, signals, pistons and the like arriving.
        static constexpr std::uint32_t kPlace = 128;
        /// What the block that left sets off on its way out (`affectNeighborsAfterRemoval`).
        static constexpr std::uint32_t kDeparture = 256;
    };

    /// Runs `effects` for blocks already written at (x, y, z) triples; returns how many positions were loaded.
    virtual std::int32_t settleBlocks(std::span<const std::int32_t> positions, std::span<const std::int32_t> previous,
                                      std::uint32_t effects) {
        static_cast<void>(positions);
        static_cast<void>(previous);
        static_cast<void>(effects);
        return 0;
    }

    /// `settleBlocks` without knowing what was there before.
    std::int32_t updateNeighbors(std::span<const std::int32_t> positions, std::uint32_t effects) {
        return settleBlocks(positions, {}, effects);
    }

    /// Tells the block at each position a neighbour changed; returns how many positions were loaded.
    virtual std::int32_t neighborChanged(std::span<const std::int32_t> positions,
                                         std::span<const std::int32_t> sources) {
        static_cast<void>(positions);
        static_cast<void>(sources);
        return 0;
    }

    /// What the state would settle to if it were put at the position (fences join, stairs turn), written nowhere.
    [[nodiscard]] virtual std::int32_t validState(std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return stateId;
    }

    /// Lets blocks around each position reshape to fit it; `states` limits it to dust lines. Returns positions reached.
    virtual std::int32_t updateShapes(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                                      bool indirect) {
        static_cast<void>(positions);
        static_cast<void>(states);
        static_cast<void>(indirect);
        return 0;
    }

    /// A player's use of item `item` on face 0 to 5 of the block at (x, y, z); returns how many blocks were put down.
    virtual std::int32_t useItemOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face,
                                   std::string_view item, float yaw, float pitch) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(face);
        static_cast<void>(item);
        static_cast<void>(yaw);
        static_cast<void>(pitch);
        return 0;
    }

    /// Books a tick of the block (or its fluid) `delay` ticks away; false when nothing there ticks.
    virtual bool scheduleTick(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t delay, bool fluid) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(delay);
        static_cast<void>(fluid);
        return false;
    }

    /// Writes states at (x, y, z) triples, then runs `effects` for the ones that changed; returns how many changed.
    virtual std::int32_t applyChanges(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                                      std::uint32_t effects) {
        const std::size_t count = std::min(positions.size() / 3, states.size());
        std::vector<std::int32_t> changed;
        std::vector<std::int32_t> before;
        for (std::size_t index = 0; index < count; ++index) {
            const std::int32_t was =
                stateAt(positions[index * 3], positions[index * 3 + 1], positions[index * 3 + 2], true);
            if (was != states[index] && setStateAt(positions[index * 3], positions[index * 3 + 1],
                                                   positions[index * 3 + 2], states[index], false)) {
                changed.insert(changed.end(), positions.begin() + static_cast<std::ptrdiff_t>(index * 3),
                               positions.begin() + static_cast<std::ptrdiff_t>(index * 3 + 3));
                before.push_back(was);
            }
        }
        static_cast<void>(settleBlocks(changed, before, effects));
        return static_cast<std::int32_t>(changed.size() / 3);
    }

    /// A block entity as a named NBT compound with id, x, y and z added; nothing when the position holds none.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> blockEntityNbt(std::int32_t x, std::int32_t y,
                                                                                  std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return std::nullopt;
    }

    /// Where the block entities of a loaded chunk are, as x, y, z triples in world coordinates.
    [[nodiscard]] virtual std::vector<std::int32_t> blockEntityPositions(std::int32_t chunkX, std::int32_t chunkZ) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        return {};
    }

    /// Replaces the block entity data at a position from a named NBT compound; false when the block owns none.
    virtual bool setBlockEntityNbt(std::int32_t x, std::int32_t y, std::int32_t z, std::span<const std::uint8_t> nbt) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(nbt);
        return false;
    }

    /// Gives the block a block entity from a named NBT compound (empty: defaults); false when it owns none.
    virtual bool createBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, std::span<const std::uint8_t> nbt) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(nbt);
        return false;
    }

    /// Takes the block entity at a position away without touching the block; false when there was none.
    virtual bool removeBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return false;
    }

    /// An entity as the named NBT compound the world saves it as (id, Pos, Rotation, Motion, UUID...).
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> entityNbt(std::int32_t entityId) {
        static_cast<void>(entityId);
        return std::nullopt;
    }

    /// Brings an entity back from `entityNbt`; returns its new id, or -1 (also when entity.spawn is cancelled).
    virtual std::int32_t spawnEntityNbt(std::span<const std::uint8_t> nbt, std::string_view cause) {
        static_cast<void>(nbt);
        static_cast<void>(cause);
        return -1;
    }

    /// The ender dragon fight of an End world: won before, exit portal, respawn stage and gateways.
    struct DragonFightFacts {
        bool previouslyKilled = false;
        std::optional<std::array<std::int32_t, 3>> portal;
        /// "start", "preparing_to_summon_pillars", "summoning_pillars", "summoning_dragon", "end", or empty.
        std::string respawnStage;
        std::int32_t gatewaysLeft = 0;
        std::string dragonUuid;
        float barProgress = 1.0F;
        bool barVisible = true;
        /// Entity ids of the players who see the fight's boss bar.
        std::vector<std::int32_t> barPlayers;
        /// Uuids of the crystals the respawn uses, and of those that stand on the pillars.
        std::vector<std::string> respawnCrystals;
        std::vector<std::string> healingCrystals;
    };

    [[nodiscard]] virtual std::optional<DragonFightFacts> dragonFight() { return std::nullopt; }

    virtual bool setDragonPreviouslyKilled(bool value) {
        static_cast<void>(value);
        return false;
    }

    /// Drives the fight: "respawn", "stage" (value is the stage index), "portal", "gateway" or "crystals".
    virtual bool dragonFightAction(std::string_view action, std::int32_t value) {
        static_cast<void>(action);
        static_cast<void>(value);
        return false;
    }

    /// Puts a delayed end gateway at the position, with the spawn effect, whether or not a fight is on.
    virtual bool placeEndGateway(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return false;
    }

    /// The block x and z of the start of the nearest structure of this kind, searching `cells` placement cells out.
    [[nodiscard]] virtual std::optional<std::array<std::int32_t, 2>>
    locateStructure(std::string_view structure, std::int32_t x, std::int32_t z, std::int32_t cells, bool skipExisting) {
        static_cast<void>(structure);
        static_cast<void>(x);
        static_cast<void>(z);
        static_cast<void>(cells);
        static_cast<void>(skipExisting);
        return std::nullopt;
    }

    /// The nearest block of one of these biomes, looking out in rings of `step` blocks up to `radius`.
    [[nodiscard]] virtual std::optional<std::array<std::int32_t, 3>> locateBiome(const std::vector<std::string>& biomes,
                                                                                 std::int32_t x, std::int32_t y,
                                                                                 std::int32_t z, std::int32_t radius,
                                                                                 std::int32_t step) {
        static_cast<void>(biomes);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(radius);
        static_cast<void>(step);
        return std::nullopt;
    }

    /// What plugins keep on the world, as a named NBT compound (empty compound when nothing is kept).
    [[nodiscard]] virtual std::vector<std::uint8_t> pluginData() { return {}; }

    virtual bool setPluginData(std::span<const std::uint8_t> nbt) {
        static_cast<void>(nbt);
        return false;
    }

    /// What plugins keep on a loaded chunk, saved with it; nothing when the chunk is not loaded.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> chunkPluginData(std::int32_t chunkX,
                                                                                   std::int32_t chunkZ) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        return std::nullopt;
    }

    virtual bool setChunkPluginData(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::uint8_t> nbt) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        static_cast<void>(nbt);
        return false;
    }

    /// What plugins keep on a block entity, saved with it; nothing when the position holds none.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> blockEntityPluginData(std::int32_t x, std::int32_t y,
                                                                                         std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return std::nullopt;
    }

    virtual bool setBlockEntityPluginData(std::int32_t x, std::int32_t y, std::int32_t z,
                                          std::span<const std::uint8_t> nbt) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(nbt);
        return false;
    }

    /// What plugins keep on an entity, saved with it; nothing when the entity is gone.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> entityPluginData(std::int32_t entityId) {
        static_cast<void>(entityId);
        return std::nullopt;
    }

    virtual bool setEntityPluginData(std::int32_t entityId, std::span<const std::uint8_t> nbt) {
        static_cast<void>(entityId);
        static_cast<void>(nbt);
        return false;
    }

    /// Takes the entity with this uuid out of the world; false when no live entity has it.
    virtual bool removeEntityByUuid(std::string_view uuid) {
        const std::shared_ptr<PluginEntityRef> found = entityByUuid(uuid);
        if (found == nullptr) {
            return false;
        }
        found->remove();
        return true;
    }

    /// Biomes (biome list positions) of a chunk's 4x4x4 cells, (cellY * 4 + cellZ) * 4 + cellX; false when not loaded.
    virtual bool readBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<std::int32_t> out) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        static_cast<void>(out);
        return false;
    }

    /// Writes biome ids in the same order (a negative id keeps the cell) and shows the chunk again to its watchers.
    virtual bool writeBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::int32_t> ids) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        static_cast<void>(ids);
        return false;
    }

    /// Recomputes light of chunks (x, z pairs) off the tick thread and resends them; `done` gets how many were loaded.
    virtual void lightJob(std::span<const std::int32_t> chunks, std::function<void(std::int32_t)> done) {
        static_cast<void>(chunks);
        done(0);
    }

    /// `lightJob` without waiting for the end of it.
    void relightChunks(std::span<const std::int32_t> chunks) {
        lightJob(chunks, [](std::int32_t) {});
    }

    /// Light at a position (layer 0 block, 1 sky), brought up to date; -1 when not loaded.
    [[nodiscard]] virtual std::int32_t lightAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t layer) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(layer);
        return -1;
    }

    /// A section's light as 2048 nibble-packed bytes (y * 256 + z * 16 + x); false when not loaded or lit yet.
    virtual bool readLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer,
                           bool load, std::span<std::uint8_t> out) {
        static_cast<void>(chunkX);
        static_cast<void>(sectionY);
        static_cast<void>(chunkZ);
        static_cast<void>(layer);
        static_cast<void>(load);
        static_cast<void>(out);
        return false;
    }

    /// Replaces a section's light with 2048 bytes in the same layout; it stands until blocks around change.
    virtual bool writeLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer,
                            std::span<const std::uint8_t> nibbles) {
        static_cast<void>(chunkX);
        static_cast<void>(sectionY);
        static_cast<void>(chunkZ);
        static_cast<void>(layer);
        static_cast<void>(nibbles);
        return false;
    }

    /// Queues the light around blocks (x, y, z triples) to be worked out again, as when a block there changed.
    virtual void checkLight(std::span<const std::int32_t> positions) { static_cast<void>(positions); }

    /// Changes whenever a block or chunk of this world does, so a section copy stays valid; zero when unsupported.
    [[nodiscard]] virtual std::uint64_t blockVersion() const { return 0; }

    /// Sends the chunks (x, z pairs) again to every player in this world that is near them.
    virtual void resendChunks(std::span<const std::int32_t> chunks) { static_cast<void>(chunks); }

    [[nodiscard]] virtual std::optional<std::string> biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return std::nullopt;
    }

    virtual bool setBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view biome) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(biome);
        return false;
    }

    /// The highest y whose block counts for `kind` (`surface`, `ocean_floor`, `motion_blocking`), or minHeight - 1.
    [[nodiscard]] virtual std::int32_t highest(std::int32_t x, std::int32_t z, std::string_view kind, bool load) {
        static_cast<void>(x);
        static_cast<void>(z);
        static_cast<void>(kind);
        static_cast<void>(load);
        return 0;
    }

    [[nodiscard]] virtual bool chunkLoaded(std::int32_t chunkX, std::int32_t chunkZ) const {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        return false;
    }

    /// Loads or generates the chunk, and returns whether it is there afterwards.
    virtual bool loadChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        static_cast<void>(generate);
        return false;
    }

    /// Saves and drops a chunk nobody is using; false when a player still sees it.
    virtual bool unloadChunk(std::int32_t chunkX, std::int32_t chunkZ) {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        return false;
    }

    /// Loaded chunks as x, z pairs.
    [[nodiscard]] virtual std::vector<std::int32_t> loadedChunks() const { return {}; }

    [[nodiscard]] virtual std::shared_ptr<PluginEntityRef> entityById(std::int32_t id) const {
        static_cast<void>(id);
        return nullptr;
    }

    [[nodiscard]] virtual std::shared_ptr<PluginEntityRef> entityByUuid(std::string_view uuid) const {
        static_cast<void>(uuid);
        return nullptr;
    }

    /// Entities in the chunk, which are the ones it will save.
    [[nodiscard]] virtual std::vector<std::shared_ptr<PluginEntityRef>> entitiesIn(std::int32_t chunkX,
                                                                                   std::int32_t chunkZ) const {
        static_cast<void>(chunkX);
        static_cast<void>(chunkZ);
        return {};
    }

    virtual void save() {}

    /// The world folder name on disk, under `worlds/`.
    [[nodiscard]] virtual std::string folder() const { return {}; }

    /// The physical-world operations (effects, border, drops, signals); null where the world offers none.
    [[nodiscard]] virtual PluginWorldEnv* env() { return nullptr; }
};
} // namespace basalt::server::plugins
