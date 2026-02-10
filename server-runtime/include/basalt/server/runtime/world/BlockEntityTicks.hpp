#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>

#include <array>
#include <cstdint>
#include <functional>
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
    /// The turn a song ran out on: the block keeps its disc but stops feeding the redstone, so
    /// whatever it was powering has to be told. parity: `JukeboxBlockEntity.onSongChanged`.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> songStopped;
};

/// Ticks every block entity in `chunks` that has something to do, answering the blocks that changed
/// so the caller can tell whoever is watching.
///
/// `random` is the world's own, needed by the one block entity that rolls for anything: a hopper
/// feeding a composter. Required rather than defaulted, so a new caller cannot forget it.
///
/// `gameTime` is the world's turn counter, not the clock's time of day: a hopper filling a shelf
/// raises a game event, and a vibration is booked for the turn it arrives.
[[nodiscard]] std::vector<BlockChange> tick(WorldState& world, std::string_view worldName, const WorldClock& clock,
                                            const WorldWeather& weather, std::int64_t gameTime,
                                            const std::vector<std::array<std::int32_t, 2>>& chunks,
                                            random::RandomSource& random, const Hooks& hooks = {},
                                            bool hasSkylight = true);

} // namespace blockEntityTicks

} // namespace basalt::server::runtime::world
