#pragma once

#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
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

struct BreakContext {
    BlockChangeBroadcaster* blocks = nullptr;
    PlayerRegistry* registry = nullptr;
    world::RealLootTables* lootTables = nullptr;
    const world::realBlockTags::TagMembers* blockTags = nullptr;
    world::RealEnchantments* enchantments = nullptr;
    world::AdvancementTriggers* triggers = nullptr;
    const world::realItemTags::TagMembers* itemTags = nullptr;

    std::function<bool()> minecartImprovements;

    std::function<double()> nextDouble;
    std::function<std::int64_t()> nextSeed;

    std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId)>
        breakAllowed;
    std::function<bool(std::string_view worldName, double x, double y, double z, const std::string& item,
                       std::int32_t count, std::string_view cause)>
        itemSpawnAllowed;
    std::function<bool(const PlayerSession& player, const std::string& item, std::int32_t amount, bool broken)>
        itemDamageAllowed;

    std::function<void(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId)>
        putBack;
    std::function<void(const PlayerSession& player, std::int32_t slot)> slotChanged;
    /// Said the moment the block goes, before anybody is told: the Java door answers the dig here.
    std::function<void()> acknowledge;
    std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount)> awardExperience;
    std::function<void()> stopDigging;
    std::function<void(world::LoadedWorld& world, const PlayerSession& player, std::int32_t x, std::int32_t y,
                       std::int32_t z, std::int32_t stateId, const wire::nbt::NbtCompound& hive, bool silkTouch)>
        hiveBroken;
    /// parity: `Block.playerWillDestroy`'s `guarded_by_piglins` check, before the block goes.
    std::function<void(world::LoadedWorld& world, const PlayerSession& player, std::int32_t stateId)> guardedBroken;
    std::function<void(world::LoadedWorld& world, const PlayerSession& player, std::int32_t x, std::int32_t y,
                       std::int32_t z)>
        creakingHeartBroken;
    std::function<void(PlayerSession& player)> dismount;
    std::function<void(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)> bedLeft;
    std::function<bool(const PlayerSession& player, std::string_view state, std::int32_t hookId, std::int32_t& exp)>
        fishing;
    std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId)>
        fertilizeAllowed;
    std::function<bool(const PlayerSession& player, std::string_view type, std::int32_t x, std::int32_t y,
                       std::int32_t z, std::string_view face)>
        hangingAllowed;
};

[[nodiscard]] std::vector<PlayerSession*> playersIn(const BreakContext& context, std::string_view worldName);

void announceBlock(const BreakContext& context, std::string_view worldName, std::int32_t x, std::int32_t y,
                   std::int32_t z, std::int32_t stateId);

void notifyAround(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                  std::int32_t z, std::optional<std::int32_t> removed);

void reshapeAround(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                   std::int32_t z);

void heardBySculk(const BreakContext& context, world::LoadedWorld& world, std::string_view event, std::int32_t x,
                  std::int32_t y, std::int32_t z, const world::vibrations::Source& source = {});

/// Puts a stack into the world. Telling anyone about it belongs to each connection's own thread.
void spawnDroppedItem(const BreakContext& context, world::LoadedWorld& world, const wire::common::Identifier& item,
                      std::int32_t count, double x, double y, double z, double velocityX, double velocityY,
                      double velocityZ, std::int32_t pickupDelay, wire::nbt::NbtCompound components,
                      std::optional<wire::common::Uuid> thrower = std::nullopt);

void dropLootFor(const BreakContext& context, world::LoadedWorld& world, const wire::common::Identifier& block,
                 std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z,
                 const wire::nbt::NbtCompound* carried, const storage::StoredItemStack* tool = nullptr);

[[nodiscard]] std::optional<std::int32_t> meltedIce(const BreakContext& context, world::LoadedWorld& world,
                                                    std::optional<std::int32_t> broken, std::int32_t x, std::int32_t y,
                                                    std::int32_t z);

void refreshSnowAt(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                   std::int32_t z);

void wearHeldItemOnBreak(const BreakContext& context, PlayerSession& player, const wire::common::Identifier& block);

void wearHeldItem(const BreakContext& context, PlayerSession& player, std::int32_t cost);

void wearItemIn(const BreakContext& context, PlayerSession& player, std::int32_t slot, std::int32_t cost);

bool breakBlockFor(PlayerSession& player, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                   const BreakContext& context);

} // namespace basalt::server::runtime::player
