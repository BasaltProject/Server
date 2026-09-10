#pragma once

#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/RandomTicks.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
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
class LoadedWorld;
class RealRecipes;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::sched {
class Region;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class ChunkSimulationView;

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
    void tickFluids(world::LoadedWorld& world, const sched::Region& region);

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

    /// parity: `Warden.increaseAngerAt` from a vibration: a step it hears is worth thirty-five.
    static void hearFootstep(world::LoadedWorld& world, const player::PlayerSession& player);

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
