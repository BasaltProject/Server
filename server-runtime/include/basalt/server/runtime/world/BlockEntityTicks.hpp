#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

namespace blockEntityTicks {

struct Dropped {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    server::world::loot::ItemStack stack;
};

struct Hooks {
    std::function<const server::world::crafting::Recipe*(server::world::crafting::Cooker,
                                                         const server::world::loot::ItemStack&)>
        recipeFor;
    std::function<void(const Dropped&)> onDropped;
    /// parity: the Bukkit `InventoryMoveItemEvent` a hopper fires before it moves anything.
    std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z)> allowHopperMove;
    std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z, std::string_view cooker,
                       const server::world::loot::ItemStack& input, server::world::loot::ItemStack& result,
                       std::string_view recipe)>
        allowSmelt;
    std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z, std::string_view cooker,
                       const server::world::loot::ItemStack& fuel, std::int32_t& burnTicks)>
        furnaceBurn;
    std::function<void(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z, std::string_view cooker,
                       const server::world::loot::ItemStack& source, std::int32_t totalTicks, std::string_view recipe)>
        furnaceStarted;
    std::function<bool(const wire::common::Identifier& item)> isBookshelfBook;
    std::function<bool(const wire::common::Identifier& block)> doesNotBlockHoppers;
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> gatewayBeam;
    std::function<std::int64_t(const std::string& song)> songLength;
    /// parity: the `beacon_base_blocks` block tag.
    std::function<bool(const wire::common::Identifier& block)> isBeaconBase;
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t levels, const std::string& primary,
                       const std::string& secondary)>
        beaconPulse;
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t levels)> beaconLit;
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> songStopped;
    /// module does not know. parity: `VibrationSystem.Ticker.tick`'s `receiveVibration`, which
    std::function<void(const vibrations::ShriekerArrival& arrival)> shriekerHeard;
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& pending)> beehive;
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& pending)>
        creakingHeart;
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)> sculkCatalyst;
    std::function<bool(wire::nbt::NbtCompound& data, std::int32_t x, std::int32_t y, std::int32_t z)> conduit;
    WorldState::Ticking ticking;
    std::function<bool(wire::nbt::NbtCompound& data, std::int32_t x, std::int32_t y, std::int32_t z)> spawnerTick;
};

[[nodiscard]] std::vector<BlockChange> tick(WorldState& world, std::string_view worldName, const WorldClock& clock,
                                            std::int32_t skyDarken, std::int64_t gameTime,
                                            const std::vector<std::array<std::int32_t, 2>>& chunks,
                                            random::RandomSource& random, const Hooks& hooks = {},
                                            bool hasSkylight = true, bool sunMoves = true, std::int64_t worldSeed = 0);

} // namespace blockEntityTicks

} // namespace basalt::server::runtime::world
