#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
struct BlockEvent;
class LoadedWorld;
class RealLootTables;

namespace redstone {
enum class Asked : std::uint8_t;
} // namespace redstone
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

/// Everything a block change owes the clients watching it: the update itself, the sound or particle
/// that rides along with it, and what redstone and pistons do because of it.
class BlockChangeBroadcaster {
public:
    /// What a plugin gets a say in. A callback rather than the plugin bus itself: this has no
    /// business knowing what a plugin is, and an unset hook means nobody objects.
    struct Hooks {
        /// Whether a piston at this position may finish its move.
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            pistonAllowed;

        /// parity: `CarvedPumpkinBlock.trySpawnGolem`'s `addFreshEntity`. A callback because making
        /// a mob belongs to the entity side and this only knows which blocks went.
        std::function<void(world::LoadedWorld& world, std::string_view type, double x, double y, double z)> summonGolem;
    };

    struct Options {
        /// How far from a piston a player can be and still be in the way of what it pushes.
        double pistonReach = 14.0;
        /// The seed a washed-away block's loot roll and a block sound's own random start from.
        std::function<std::int64_t()> nextSeed;
        /// What answers `minecraft:copper`, the copper golem's body. Null where no jar is loaded,
        /// and then no copper golem is ever built.
        const world::realBlockTags::TagMembers* blockTags = nullptr;
    };

    BlockChangeBroadcaster(player::PlayerRegistry& players, world::RealLootTables& lootTables, Hooks hooks,
                           Options options);

    /// The neighbour update any block written by the world itself owes the blocks around it.
    ///
    /// `removed` is what stood there before, for the half of the cascade that belongs to the block
    /// that has gone. Nothing means the caller does not know, which is not the same as nothing having
    /// been removed, so it has no default.
    /// parity: `FallingBlockEntity.tick`'s `spawnAtLocation(level, block)` when the block cannot be
    /// placed where it stopped: the block's **own** item, not its loot table.
    void dropFallenBlock(world::LoadedWorld& world, double x, double y, double z, std::int32_t stateId);

    void notifyAround(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                      std::int32_t y, std::int32_t z, std::optional<std::int32_t> removed);

    /// Redraws the six squares around one that changed and tells nobody else.
    ///
    /// parity: `Level.setBlock` runs the shape updates under `(flags & 16) == 0`, which is outside
    /// the `UPDATE_NEIGHBORS` gate, so a component writing its own state with flag two redraws the
    /// shapes without telling a single neighbour.
    void reshapeAround(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                       std::int32_t y, std::int32_t z);

    /// One block, its new state, and whatever it asked the clients to hear or see while they are at
    /// it: a note, a bell, a door swinging, a dispenser clicking at nothing.
    void announceBlock(const std::vector<player::PlayerSession*>& watchers, const world::BlockChange& change) const;

    /// The effect half on its own, for the blocks that make a noise without changing state.
    void sendEffects(const std::vector<player::PlayerSession*>& watchers, std::int32_t x, std::int32_t y,
                     std::int32_t z, std::int32_t stateId, const std::vector<world::BlockEffect>& effects) const;

    /// Sends a block that redstone moved to everyone watching, and lets its neighbours react.
    void announceRedstone(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          const world::BlockChange& change) const;

    /// One booked piston move starting. parity: `PistonBaseBlock.triggerEvent`, run from
    /// `ServerLevel.runBlockEvents` rather than where the signal arrived.
    void runPistonEvent(world::LoadedWorld& world, const world::BlockEvent& event,
                        const std::vector<player::PlayerSession*>& watchers);

    /// The piston half of a scheduled tick, with the players close enough to be shoved by it.
    ///
    /// parity: `PistonMovingBlockEntity.moveCollidedEntities`. A player who gets pushed is
    /// teleported, because a client only learns it moved when the server says so.
    [[nodiscard]] std::vector<world::BlockChange> finishPiston(world::LoadedWorld& world,
                                                               const std::array<std::int32_t, 3>& position,
                                                               const std::vector<player::PlayerSession*>& watchers);

    /// Drops what a flow washed away. parity: `WaterFluid.beforeDestroyingBlock` rolls the block's
    /// own loot table, and lava's plays the fizz and drops nothing.
    void dropWashedAwayBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                             std::int32_t destroyedState);

    /// Drops what a block change destroyed, whatever took its place. parity: `Block.updateOrDestroy`
    /// ends in `destroyBlock(pos, true)` unless the flags say otherwise, so the loot table is rolled
    /// as breaking it by hand would. `byEntity` is `LootContextParams.THIS_ENTITY`: a projectile has
    /// one, a neighbour update has none.
    void dropBrokenBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                         std::int32_t destroyedState, bool byEntity = false);

private:
    /// The half of `announceRedstone` that is about a square rather than about a block: the dust
    /// around it settles, what reacts reacts, and the components nearby book their turn. `arrived`
    /// is `scheduleAround`'s, which only the square the change happened on can claim, and `asked`
    /// is `settle`'s own: the jar's `updateNeighborsAt` only ever tells the six around a square,
    /// never the square itself, so the extra square a rod or a hook reaches settles
    /// `NeighboursOnly`.
    void settleFrom(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                    std::int32_t y, std::int32_t z, bool arrived, world::redstone::Asked asked) const;

    [[nodiscard]] std::int64_t nextSeed() const;

    player::PlayerRegistry* players_;
    world::RealLootTables* lootTables_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
