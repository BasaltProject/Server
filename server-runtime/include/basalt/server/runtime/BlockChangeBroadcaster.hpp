#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::random {
class RandomSource;
} // namespace basalt::server::random

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

namespace neighbourUpdates {
struct Context;
} // namespace neighbourUpdates
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster {
public:
    struct Hooks {
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           bool extending, std::string_view direction)>
            pistonAllowed;

        /// parity: `CarvedPumpkinBlock.trySpawnGolem`'s `addFreshEntity`.
        std::function<void(world::LoadedWorld& world, std::string_view type, double x, double y, double z)> summonGolem;
    };

    struct Options {
        double pistonReach = 14.0;
        std::function<std::int64_t()> nextSeed;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
    };

    BlockChangeBroadcaster(player::PlayerRegistry& players, world::RealLootTables& lootTables, Hooks hooks,
                           Options options);

    /// parity: `FallingBlockEntity.tick`'s `spawnAtLocation(level, block)`.
    void dropFallenBlock(world::LoadedWorld& world, double x, double y, double z, std::int32_t stateId);

    /// `phases` are the `neighbourUpdates::phase` bits; `removed` is the state that left, when it is known.
    void settleBlock(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::optional<std::int32_t> removed, std::uint32_t phases);

    /// parity: `BlockState.handleNeighborChanged` on the block at (x, y, z).
    void neighbourChanged(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          std::int32_t x, std::int32_t y, std::int32_t z, std::optional<std::int32_t> source);

    /// parity: `Block.updateFromNeighbourShapes`.
    [[nodiscard]] std::int32_t validState(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                          std::int32_t stateId);

    /// parity: `BlockState.updateIndirectNeighbourShapes`.
    void reshapeIndirect(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                         std::int32_t y, std::int32_t z, std::int32_t stateId);

    void notifyAround(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                      std::int32_t y, std::int32_t z, std::optional<std::int32_t> removed);

    /// parity: `Level.setBlock`.
    void reshapeAround(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                       std::int32_t y, std::int32_t z);

    void announceBlock(const std::vector<player::PlayerSession*>& watchers, const world::BlockChange& change) const;

    void sendEffects(const std::vector<player::PlayerSession*>& watchers, std::int32_t x, std::int32_t y,
                     std::int32_t z, std::int32_t stateId, const std::vector<world::BlockEffect>& effects) const;

    void announceRedstone(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          const world::BlockChange& change) const;

    /// parity: `PistonBaseBlock.triggerEvent`.
    void runPistonEvent(world::LoadedWorld& world, const world::BlockEvent& event,
                        const std::vector<player::PlayerSession*>& watchers);

    /// parity: `PistonMovingBlockEntity.moveCollidedEntities`.
    [[nodiscard]] std::vector<world::BlockChange> finishPiston(world::LoadedWorld& world,
                                                               const std::array<std::int32_t, 3>& position,
                                                               const std::vector<player::PlayerSession*>& watchers);

    /// parity: `WaterFluid.beforeDestroyingBlock`.
    void dropWashedAwayBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                             std::int32_t destroyedState);

    /// parity: `Block.updateOrDestroy`.
    void dropBrokenBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                         std::int32_t destroyedState, bool byEntity = false);

private:
    [[nodiscard]] world::neighbourUpdates::Context updateContext(world::LoadedWorld& world,
                                                                 random::RandomSource& random) const;

    void settleFrom(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                    std::int32_t y, std::int32_t z, bool arrived, world::redstone::Asked asked) const;

    [[nodiscard]] std::int64_t nextSeed() const;

    player::PlayerRegistry* players_;
    world::RealLootTables* lootTables_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
