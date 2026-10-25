#pragma once

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/RandomTicks.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/server/world/loot/Loot.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class LoadedWorld;
class RealRecipes;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::sched {
class Region;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class ChunkSimulationView;
class MobSpawnerSystem;
class TrialSpawnerSystem;

class RedstoneTickSystem {
public:
    struct Hooks {
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view cause)>
            igniteAllowed;
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t toX, std::int32_t toY, std::int32_t toZ)>
            flowAllowed;
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId, std::int32_t fromX, std::int32_t fromY, std::int32_t fromZ)>
            burnAllowed;
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            dispenseAllowed;
        world::randomTicks::MayChange mayChange;
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId)>
            structureAllowed;
        std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z)> allowHopperMove;
        std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view cooker, const server::world::loot::ItemStack& input,
                           server::world::loot::ItemStack& result, std::string_view recipe)>
            allowSmelt;
        std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view cooker, const server::world::loot::ItemStack& fuel,
                           std::int32_t& burnTicks)>
            furnaceBurn;
        std::function<void(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view cooker, const server::world::loot::ItemStack& source,
                           std::int32_t totalTicks, std::string_view recipe)>
            furnaceStarted;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        world::jukeboxes::SongOutputs* jukeboxes = nullptr;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        std::function<void(world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x, std::int32_t y,
                           std::int32_t z)>
            steppedOn;
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           std::int32_t x, std::int32_t y, std::int32_t z)>
            foreignTick;
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)> skullPlaced;
        std::function<void(world::LoadedWorld& world, double x, double y, double z, double dirX, double dirY,
                           double dirZ, const server::world::loot::ItemStack& stack)>
            launchFirework;
        std::function<void(world::LoadedWorld& world, const world::scheduledBlocks::Hatchling& hatched)> spawnHatchling;
        world::AdvancementTriggers* advancementTriggers = nullptr;
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId)>
            releaseHive;
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<world::BlockChange>& pending)>
            tickBeehive;
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<world::BlockChange>& pending)>
            tickCreakingHeart;
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           wire::nbt::NbtCompound& data)>
            tickBell;
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            tickSculkCatalyst;
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           wire::nbt::NbtCompound& data)>
            tickConduit;
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            triggerGatewayCooldown;
        std::function<void(world::LoadedWorld& world, const world::vibrations::ShriekerArrival& arrival)> shriekerHeard;
        /// parity: `Armadillo.brushOffScute` from the dispenser's brush behaviour.
        std::function<bool(world::LoadedWorld& world, entity::ServerEntity& armadillo)> brushArmadillo;
        MobSpawnerSystem* mobSpawners = nullptr;
        TrialSpawnerSystem* trialSpawners = nullptr;
    };

    RedstoneTickSystem(player::PlayerRegistry& players, world::RealRecipes& recipes,
                       BlockChangeBroadcaster& broadcaster, ChunkSimulationView& chunkSimulation,
                       world::commandBlocks::RunCommand runCommand, Hooks hooks, Options options);

    /// parity: `AbstractMinecart.getDefaultGravity`.
    static constexpr double kMinecartGravity = 0.04;

    [[nodiscard]] world::WorldState::Ticking tickingChunks(const world::LoadedWorld& world, const sched::Region& region,
                                                           std::int32_t extraRadius) const;

    void tickFluids(world::LoadedWorld& world, const sched::Region& region);

    void openTurn(world::LoadedWorld& world, const sched::Region& region);

    void tickScheduledBlocks(world::LoadedWorld& world, const sched::Region& region);

    void tickBlockEvents(world::LoadedWorld& world, const sched::Region& region);

    void tickPressurePlates(world::LoadedWorld& world, const sched::Region& region);

    void tickArrowButtons(world::LoadedWorld& world, const sched::Region& region);

    void pressArrowButton(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          std::int32_t x, std::int32_t y, std::int32_t z, double arrowX, double arrowY, double arrowZ,
                          double half, double height);

    void tickFootsteps(world::LoadedWorld& world);

    void tickTripwires(world::LoadedWorld& world, const sched::Region& region);

    void tickFallingBlocks(world::LoadedWorld& world, const sched::Region& region);

    void tickRandomBlocks(world::LoadedWorld& world, const sched::Region& region);

    /// One random tick on one block, as a plugin asks; true when the block changed.
    bool randomTickAt(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    void tickBlockEntities(world::LoadedWorld& world, const sched::Region& region);

    void tickInhabitedTime(world::LoadedWorld& world, const sched::Region& region);

private:
    [[nodiscard]] world::randomTicks::Context randomContext(world::LoadedWorld& world);

    bool tickSpawner(world::LoadedWorld& world, wire::nbt::NbtCompound& data, std::int32_t x, std::int32_t y,
                     std::int32_t z);

    bool runSpawnerEvent(world::LoadedWorld& world, const world::BlockEvent& event,
                         const std::vector<player::PlayerSession*>& watchers);

    void releaseHatchlings(world::LoadedWorld& world, std::vector<world::scheduledBlocks::Hatchling>& hatched);

    void crafterCrafted(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                        const server::world::crafting::CraftingInput& input, const world::blockInventory::Slots& grid);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    [[nodiscard]] static std::int32_t standingOn(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                 std::int32_t z, const std::vector<player::PlayerSession*>& watchers,
                                                 const std::vector<world::DroppedItem>& items,
                                                 std::size_t movedThrough);

    [[nodiscard]] random::RandomSource freshRandom() const;

    player::PlayerRegistry* players_;
    world::RealRecipes* recipes_;
    BlockChangeBroadcaster* broadcaster_;
    ChunkSimulationView* chunkSimulation_;
    Hooks hooks_;
    Options options_;
    world::commandBlocks::RunCommand runCommand_;
    std::map<std::int32_t, std::int64_t> lastFootstep_;
    world::RandomTicks randomTicks_;
};

} // namespace basalt::server::runtime
