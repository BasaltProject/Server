#pragma once

#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::storage {
struct StoredItemStack;
}

namespace basalt::server::runtime {
class BlockChangeBroadcaster;
} // namespace basalt::server::runtime

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealEnchantments;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::player {

class PlayerSession;
class PlayerRegistry;

/// What breaking a block needs of the server, without needing the connection it was broken on. The
/// loot, the neighbours, the sensor and the wear are the same everywhere; only the digger's clock
/// and acknowledgement differ, and those are callbacks. Apart from `BlockInteractionHandler` so a
/// player who never had a `ClientSession` can still break something.
struct BreakContext {
    BlockChangeBroadcaster* blocks = nullptr;
    PlayerRegistry* registry = nullptr;
    world::RealLootTables* lootTables = nullptr;
    const world::realBlockTags::TagMembers* blockTags = nullptr;
    world::RealEnchantments* enchantments = nullptr;
    const world::realItemTags::TagMembers* itemTags = nullptr;

    /// The rolls a break makes, none of which is part of the world's own stream.
    std::function<double()> nextDouble;
    std::function<std::int64_t()> nextSeed;

    /// What a plugin gets a say in. Unset means nobody objects.
    std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId)>
        breakAllowed;
    std::function<bool(std::string_view worldName, double x, double y, double z, const std::string& item,
                       std::int32_t count, std::string_view cause)>
        itemSpawnAllowed;
    std::function<bool(const PlayerSession& player, const std::string& item, std::int32_t amount, bool broken)>
        itemDamageAllowed;

    /// Puts the block back on the breaker's own screen, which is what a refusal costs: their client
    /// took it away the moment they started.
    std::function<void(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId)>
        putBack;
    /// Tells the breaker one slot of theirs changed, however their edition says it.
    std::function<void(const PlayerSession& player, std::int32_t slot)> slotChanged;
    /// Said the moment the block goes, before anybody is told: the Java door answers the dig here.
    std::function<void()> acknowledge;
    /// Experience left as orbs at a point.
    std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount)> awardExperience;
    /// Stops the digging clock, which only a door that runs one has.
    std::function<void()> stopDigging;
};

/// Everyone in that world, which is who hears a block go.
[[nodiscard]] std::vector<PlayerSession*> playersIn(const BreakContext& context, std::string_view worldName);

/// One block, its new state, to everyone who can see it.
void announceBlock(const BreakContext& context, std::string_view worldName, std::int32_t x, std::int32_t y,
                   std::int32_t z, std::int32_t stateId);

/// The neighbours of a block that changed, told the way the world tells them.
void notifyAround(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                  std::int32_t z, std::optional<std::int32_t> removed);

/// The six squares around a change, redrawn and nothing else: the shape half of `notifyAround`, for
/// a write that goes in without `UPDATE_NEIGHBORS`.
void reshapeAround(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                   std::int32_t z);

/// What a vibration-sensitive block hears, and whatever that shakes loose.
void heardBySculk(const BreakContext& context, world::LoadedWorld& world, std::string_view event, std::int32_t x,
                  std::int32_t y, std::int32_t z, const world::vibrations::Source& source = {});

/// Puts a stack into the world. Telling anyone about it belongs to each connection's own thread.
void spawnDroppedItem(const BreakContext& context, world::LoadedWorld& world, const wire::common::Identifier& item,
                      std::int32_t count, double x, double y, double z, double velocityX, double velocityY,
                      double velocityZ, std::int32_t pickupDelay, wire::nbt::NbtCompound components);

/// What the block leaves on the ground.
void dropLootFor(const BreakContext& context, world::LoadedWorld& world, const wire::common::Identifier& block,
                 std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z,
                 const wire::nbt::NbtCompound* carried, const storage::StoredItemStack* tool = nullptr);

/// What ice leaves behind where it melts rather than breaks, or nothing when it simply breaks.
[[nodiscard]] std::optional<std::int32_t> meltedIce(const BreakContext& context, world::LoadedWorld& world,
                                                    std::optional<std::int32_t> broken, std::int32_t x, std::int32_t y,
                                                    std::int32_t z);

/// Redraws the snow layer under a block that went, which is what stops grass staying white.
void refreshSnowAt(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                   std::int32_t z);

/// Wears the pick down by what this block costs it.
void wearHeldItemOnBreak(const BreakContext& context, PlayerSession& player, const wire::common::Identifier& block);

/// parity: `ItemStack.hurtAndBreak` on the held item, for a tool used on something other than a block.
void wearHeldItem(const BreakContext& context, PlayerSession& player, std::int32_t cost);

/// Takes a block away as a player breaking it does: the loot, the neighbours, the statistics and
/// everything that hears it go. Answers whether the block actually went.
bool breakBlockFor(PlayerSession& player, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                   const BreakContext& context);

} // namespace basalt::server::runtime::player
