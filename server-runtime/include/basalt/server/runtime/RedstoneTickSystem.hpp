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

namespace basalt::server::runtime::sched {
class Region;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class ChunkSimulationView;
class MobSpawnerSystem;

/// Everything that runs on the world's own clock rather than a player's: scheduled block ticks
/// (fire, redstone, pistons, buttons), fluids, random block ticks, block entities, falling blocks,
/// pressure plates, tripwires, footsteps and a chunk's inhabited time.
class RedstoneTickSystem {
public:
    /// What a plugin gets a say in. Callbacks rather than the plugin bus itself, and unset means
    /// nobody objects.
    struct Hooks {
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view cause)>
            igniteAllowed;
        std::function<bool(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            dispenseAllowed;
        /// Passed straight to the random-tick sweep, which asks before every block it changes.
        world::randomTicks::MayChange mayChange;
        /// What a block entity asks before it moves or smelts anything.
        std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z)> allowHopperMove;
        std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view cooker, const server::world::loot::ItemStack* input,
                           const server::world::loot::ItemStack* result)>
            allowSmelt;
    };

    /// The rolls a world tick makes that are not part of any block's own stream.
    struct Options {
        std::function<std::int64_t()> nextSeed;
        /// The jar's own song lengths, so a jukebox knows when to go quiet.
        world::jukeboxes::SongOutputs* jukeboxes = nullptr;
        /// The item tags the datapack loaded, which is what a chiseled bookshelf asks before it
        /// takes what a hopper is holding.
        const world::realItemTags::TagMembers* itemTags = nullptr;
        /// The block tags, which is what a dispensed water bottle asks before it makes mud.
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        /// A player's step onto a block, for the blocks whose `stepOn` is not redstone's.
        std::function<void(world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x, std::int32_t y,
                           std::int32_t z)>
            steppedOn;
        /// A booked turn of a block this system does not own.
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           std::int32_t x, std::int32_t y, std::int32_t z)>
            foreignTick;
        /// A wither skull a dispenser put down, for whoever knows how to raise the boss.
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)> skullPlaced;
        /// A firework rocket a dispenser shot, from where and which way.
        std::function<void(world::LoadedWorld& world, double x, double y, double z, double dirX, double dirY,
                           double dirZ, const server::world::loot::ItemStack& stack)>
            launchFirework;
        /// What a hatching block left behind, put in the world. parity: `spawnGhastling` and the two
        /// like it, which make their entity inside the block's own turn.
        std::function<void(world::LoadedWorld& world, const world::scheduledBlocks::Hatchling& hatched)> spawnHatchling;
        /// Where the advancement for lighting a beacon goes, which is to whoever is standing near it.
        world::AdvancementTriggers* advancementTriggers = nullptr;
        /// A hive a dispenser took the honey from. parity: `releaseBeesAndResetHoneyLevel`.
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId)>
            releaseHive;
        /// A beehive's turn, which lets bees out. parity: `BeehiveBlockEntity.serverTick`.
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<world::BlockChange>& pending)>
            tickBeehive;
        /// A creaking heart's turn. parity: `CreakingHeartBlockEntity.serverTick`.
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<world::BlockChange>& pending)>
            tickCreakingHeart;
        /// A sculk catalyst's spreader. parity: `SculkCatalystBlockEntity.serverTick`.
        std::function<bool(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z)>
            tickSculkCatalyst;
        /// A noise that reached a shrieker, handed to whoever knows the players.
        std::function<void(world::LoadedWorld& world, const world::vibrations::ShriekerArrival& arrival)> shriekerHeard;
        /// What runs a monster spawner block's turn. Null leaves every spawner idle.
        MobSpawnerSystem* mobSpawners = nullptr;
    };

    /// `runCommand` is what a command block runs its command with. It is a constructor parameter and
    /// not a field of `Options` on purpose: an unset one would leave every command block silently
    /// doing nothing, and this way the compiler names the site that forgot it.
    RedstoneTickSystem(player::PlayerRegistry& players, world::RealRecipes& recipes,
                       BlockChangeBroadcaster& broadcaster, ChunkSimulationView& chunkSimulation,
                       world::commandBlocks::RunCommand runCommand, Hooks hooks, Options options);

    /// parity: `AbstractMinecart.getDefaultGravity`, the same 0.04 as anything else that is not
    /// alive.
    static constexpr double kMinecartGravity = 0.04;

    /// Runs every fluid tick that came due and tells the players what moved.
    /// The chunks whose scheduled turns run this tick: the simulated ones, which is what the jar
    /// hands a `LevelTicks` container to.
    /// `extraRadius` is 1 for anything that belongs to `BLOCK_TICKING` and 0 for anything that
    /// belongs to `ENTITY_TICKING`, which stops one chunk sooner.
    [[nodiscard]] world::WorldState::Ticking tickingChunks(const world::LoadedWorld& world, const sched::Region& region,
                                                           std::int32_t extraRadius) const;

    void tickFluids(world::LoadedWorld& world, const sched::Region& region);

    /// Opens the turn for the scheduled turns of this region, before any of them runs.
    ///
    /// parity: the jar books an absolute `triggerTick`; counting every turn down once, here, says
    /// the same with a countdown and keeps a pass from shortening what its own turn booked.
    void openTurn(world::LoadedWorld& world, const sched::Region& region);

    /// Runs the block ticks that have come due: buttons letting themselves back up, redstone
    /// components spending their delay, pistons landing, plates coming back up.
    void tickScheduledBlocks(world::LoadedWorld& world, const sched::Region& region);

    /// parity: `ServerLevel.runBlockEvents`. What a block booked for itself rather than doing on the
    /// spot: today the pistons, which is why a frozen world leaves one standing where it is.
    void tickBlockEvents(world::LoadedWorld& world, const sched::Region& region);

    /// Presses the plates whatever is standing on them.
    void tickPressurePlates(world::LoadedWorld& world, const sched::Region& region);

    /// Presses the wooden buttons an arrow is stuck in.
    void tickArrowButtons(world::LoadedWorld& world, const sched::Region& region);

    /// One square an arrow's box touches, pressed if it turns out to be a button it can press.
    void pressArrowButton(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          std::int32_t x, std::int32_t y, std::int32_t z, double arrowX, double arrowY, double arrowZ,
                          double half, double height);

    /// The footsteps a sculk sensor hears.
    void tickFootsteps(world::LoadedWorld& world);

    /// Presses the tripwires something is standing in.
    void tickTripwires(world::LoadedWorld& world, const sched::Region& region);

    void tickFallingBlocks(world::LoadedWorld& world, const sched::Region& region);

    /// Lets the blocks vanilla picks at random act: crops grow, ice melts, cane climbs.
    void tickRandomBlocks(world::LoadedWorld& world, const sched::Region& region);

    /// Lets the block entities that have something to do act: today the three that smelt.
    void tickBlockEntities(world::LoadedWorld& world, const sched::Region& region);

    /// parity: every loaded chunk inside simulation distance advances its own local-difficulty age.
    void tickInhabitedTime(world::LoadedWorld& world, const sched::Region& region);

private:
    /// parity: `SpawnerBlockEntity.serverTick`, with the block's own event and block entity update.
    /// Answers whether the spawner's data changed.
    bool tickSpawner(world::LoadedWorld& world, wire::nbt::NbtCompound& data, std::int32_t x, std::int32_t y,
                     std::int32_t z);

    /// parity: `ServerLevel.doBlockEvent` for a spawner: the event is the block entity's, and the
    /// players within sixty-four blocks are told. Answers whether the event was the spawner's.
    bool runSpawnerEvent(world::LoadedWorld& world, const world::BlockEvent& event,
                         const std::vector<player::PlayerSession*>& watchers);

    /// Puts what a hatching block left behind in the world, and empties the list.
    void releaseHatchlings(world::LoadedWorld& world, std::vector<world::scheduledBlocks::Hatchling>& hatched);

    /// parity: `CrafterBlock.dispenseFrom`'s `CRAFTER_RECIPE_CRAFTED`, for every player in the
    /// seventeen-block box around it.
    void crafterCrafted(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                        const server::world::crafting::CraftingInput& input, const world::blockInventory::Slots& grid);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    /// How many things are on the plate at these coordinates, the way its own sensitivity counts.
    ///
    /// `movedThrough` is how many of `items` have already taken their turn this tick. Vanilla counts
    /// from inside `Entity.move`, so anything that has not moved yet is still where it was, and that
    /// is the whole reason four items landing together read one.
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
    /// What a command block runs its command with.
    world::commandBlocks::RunCommand runCommand_;
    /// Which block each player was last heard stepping in, so one step is one vibration.
    std::map<std::int32_t, std::int64_t> lastFootstep_;
    /// The block ticks vanilla schedules at random per chunk: crops growing, ice melting and so on.
    world::RandomTicks randomTicks_;
};

} // namespace basalt::server::runtime
