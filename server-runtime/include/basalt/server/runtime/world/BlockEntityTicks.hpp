#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The block entities that do something on their own, ticked once each per tick. parity:
/// `LevelChunk.tickersInLevel`, so a tick visits the few that want one instead of every chest in the
/// world.
///
/// Only the chunks the caller passes are visited, so a furnace far from every player is as still as
/// vanilla's.
namespace blockEntityTicks {

/// Something a block entity handed back to the world, which the caller has to spawn.
struct Dropped {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    server::world::loot::ItemStack stack;
};

/// What each machine needs from outside this file.
///
/// `recipeFor` answers what a cooker makes of a stack: the world's recipe book is what knows, and
/// this has no business reaching for it. The two event hooks are how a plugin says no; both live in
/// `server-plugins`, which the runtime core does not depend on, so they arrive as callbacks. An
/// unset hook means nobody is listening, which is one comparison per machine.
struct Hooks {
    std::function<const server::world::crafting::Recipe*(server::world::crafting::Cooker,
                                                         const server::world::loot::ItemStack&)>
        recipeFor;
    std::function<void(const Dropped&)> onDropped;
    /// parity: the Bukkit `InventoryMoveItemEvent` a hopper fires before it moves anything.
    std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z)> allowHopperMove;
    /// parity: `InventorySmeltEvent`, which carries what goes in and what comes out.
    std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z, std::string_view cooker,
                       const server::world::loot::ItemStack* input, const server::world::loot::ItemStack* result)>
        allowSmelt;
    /// parity: `ChiseledBookShelfBlockEntity.acceptsItemType`, which asks the `bookshelf_books`
    /// item tag: the tags are the datapack's, and this file has no business loading one.
    std::function<bool(const wire::common::Identifier& item)> isBookshelfBook;
    /// parity: `JukeboxSong.lengthInTicks`, which is data in the operator's own jar.
    std::function<std::int64_t(const std::string& song)> songLength;
    /// parity: the `beacon_base_blocks` block tag, which is the datapack's and not this file's.
    std::function<bool(const wire::common::Identifier& block)> isBeaconBase;
    /// A beacon that just counted its pyramid and has an effect to hand out. Who is standing in it
    /// is the caller's business: this module knows no players.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t levels, const std::string& primary,
                       const std::string& secondary)>
        beaconPulse;
    /// A beacon that was dark and is now lit, with the tiers it stands on. parity:
    /// `BeaconBlockEntity.tick`, which raises the criterion only on that edge.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t levels)> beaconLit;
    /// The turn a song ran out on: the block keeps its disc but stops feeding the redstone, so
    /// whatever it was powering has to be told. parity: `JukeboxBlockEntity.onSongChanged`.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> songStopped;
    /// A noise that reached a shrieker: whether it shrieks depends on the players, which this
    /// module does not know. Called after every block entity has had its turn.
    std::function<void(const vibrations::ShriekerArrival& arrival)> shriekerHeard;
    /// The chunks that tick this turn, which a shrieker's noise waits on. Unset, every loaded one.
    WorldState::Ticking ticking;
};

/// Ticks every block entity in `chunks` that has something to do, answering the blocks that changed.
/// `random` is the world's, needed by the one that rolls for anything: a hopper feeding a composter.
/// `gameTime` is the turn counter, not the time of day. `worldSeed` is what a geyser forks its
/// rhythm from, and `skyDarken` is the world's stored reading. parity: `Level.getSkyDarken`.
[[nodiscard]] std::vector<BlockChange> tick(WorldState& world, std::string_view worldName, const WorldClock& clock,
                                            std::int32_t skyDarken, std::int64_t gameTime,
                                            const std::vector<std::array<std::int32_t, 2>>& chunks,
                                            random::RandomSource& random, const Hooks& hooks = {},
                                            bool hasSkylight = true, bool sunMoves = true, std::int64_t worldSeed = 0);

} // namespace blockEntityTicks

} // namespace basalt::server::runtime::world
