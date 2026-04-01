#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/player/PlayerBeehives.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <map>
#include <string>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;
namespace beehives = world::beehives;
using wire::common::Identifier;

/// parity: `BuiltInLootTables.HARVEST_BEEHIVE`, three honeycombs.
constexpr std::int32_t kHoneycombs = 3;

[[nodiscard]] double roll(const BreakContext& context) { return context.nextDouble ? context.nextDouble() : 0.5; }

/// parity: `Level.playSound` at the player, for everybody or everybody but them.
void soundAtPlayer(const BreakContext& context, world::LoadedWorld& world, const PlayerSession& player,
                   std::string_view sound, bool skipPlayer) {
    if (context.blocks == nullptr) {
        return;
    }
    std::vector<PlayerSession*> hearers = playersIn(context, world.name());
    if (skipPlayer) {
        std::erase(hearers, &player);
    }
    context.blocks->sendEffects(hearers, wire::common::mth::floor(player.x), wire::common::mth::floor(player.y),
                                wire::common::mth::floor(player.z), reg::blocks::air().stateId,
                                {world::blockEffect::Sound{.name = std::string{sound}}});
}

} // namespace

bool harvestHive(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                 std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                 const std::optional<storage::StoredItemStack>& held, MobActionSystem* mobs) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value() || !held.has_value()) {
        return false;
    }
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*stateId);
    const auto level = properties.find("honey_level");
    if (level == properties.end() || std::stoi(level->second) < beehives::kMaxHoney) {
        return false;
    }
    if (held->id == "minecraft:shears") {
        // parity: `dropHoneycomb` over `Block.popResource`, then the sound, the wear and `SHEAR`.
        if (world.gameRules().get(world::gameRules::kBlockDrops)) {
            constexpr double kItemHalfHeight = 0.125;
            const auto jitter = [&context] { return (roll(context) * 0.5) - 0.25; };
            const double dropX = x + 0.5 + jitter();
            const double dropY = y + 0.5 + jitter() - kItemHalfHeight;
            const double dropZ = z + 0.5 + jitter();
            spawnDroppedItem(context, world, Identifier::vanilla("honeycomb"), kHoneycombs, dropX, dropY, dropZ,
                             (roll(context) * 0.2) - 0.1, 0.2, (roll(context) * 0.2) - 0.1, 10, {});
        }
        soundAtPlayer(context, world, player, "block.beehive.shear", false);
        wearHeldItem(context, player, 1);
        heardBySculk(context, world, "shear", x, y, z, {.entity = player.uuid()});
    } else if (held->id == "minecraft:glass_bottle") {
        // parity: `itemStack.shrink(1)`, which a creative hand pays too.
        storage::StoredItemStack left = *held;
        left.count -= 1;
        const storage::StoredItemStack honey{.id = "minecraft:honey_bottle", .count = 1};
        soundAtPlayer(context, world, player, "item.bottle.fill", true);
        if (left.count <= 0) {
            player.inventory.set(heldSlot, honey);
        } else {
            player.inventory.set(heldSlot, left);
            if (player.inventory.add(honey) > 0) {
                spawnDroppedItem(context, world, Identifier::vanilla("honey_bottle"), 1, player.x, player.y + 1.32,
                                 player.z, 0.0, 0.0, 0.0, 40, {});
            }
        }
        if (player.connection().sendInventoryAll) {
            player.connection().sendInventoryAll();
        }
        heardBySculk(context, world, "fluid_pickup", x, y, z, {.entity = player.uuid()});
    } else {
        return false;
    }
    statAwards::used(player, held->id);
    // parity: `resetHoneyLevel`, then out come the bees unless the hive is smoked.
    if (const std::optional<std::int32_t> empty = reg::blockStateDefinitions::stateIdOf(
            *reg::blockStateDefinitions::blockOf(*stateId), {{"honey_level", "0"}}, *stateId);
        empty.has_value() && world.state().setBlockState(x, y, z, *empty)) {
        announceBlock(context, world.name(), x, y, z, *empty);
        notifyAround(context, world, x, y, z, std::nullopt);
    }
    if (mobs != nullptr && !beehives::isSmokey(world.state(), {x, y, z})) {
        server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        if (entity != nullptr && beehives::occupantCount(entity->data()) > 0) {
            mobs->angerBeesAround(world, {x, y, z});
        }
        mobs->emptyHive(world, {x, y, z}, *stateId, &player, beehives::Release::Emergency);
    }
    return true;
}

} // namespace basalt::server::runtime::player
