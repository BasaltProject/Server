#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/ThrownItemTriggers.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>
#include <limits>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;

namespace mth = wire::common::mth;

/// parity: `Dolphin.ItemGoal.drop`, forty ticks before it may be taken again.
constexpr std::int32_t kThrownPickupDelay = 40;
/// parity: `findNearestMapStructure`'s fifty rings.
constexpr std::int32_t kTreasureRadius = 50;

[[nodiscard]] bool inWater(const world::WorldState& world, double x, double y, double z) {
    const std::optional<std::int32_t> state = world.blockState(mth::floor(x), mth::floor(y), mth::floor(z));
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

} // namespace

std::vector<entity::ai::MobActions::ItemView> MobActionSystem::itemsInWater(const ServerEntity& mob, double grow) {
    std::vector<ItemView> found;
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return found;
    }
    const double half = mob.width() / 2.0;
    const double reach = half + grow + 1.0;
    for (const world::DroppedItem& item :
         world->state().nearbyItems(mob.x, mob.y, mob.z, std::sqrt(3.0) * (reach + mob.height()))) {
        // parity: `ALLOWED_ITEMS`: no pickup delay, alive, and in water, in the grown box.
        constexpr double kItemHalf = 0.125;
        if (!item.canBePickedUpByMob() || item.count <= 0 || !inWater(world->state(), item.x, item.y, item.z) ||
            item.x + kItemHalf < mob.x - half - grow || item.x - kItemHalf > mob.x + half + grow ||
            item.z + kItemHalf < mob.z - half - grow || item.z - kItemHalf > mob.z + half + grow ||
            item.y + 0.25 < mob.y - grow || item.y > mob.y + mob.height() + grow) {
            continue;
        }
        found.push_back(ItemView{.entityId = item.entityId, .x = item.x, .y = item.y, .z = item.z});
    }
    return found;
}

bool MobActionSystem::throwHeldItem(ServerEntity& mob) {
    const std::optional<server::world::loot::ItemStack> held = entity::copperGolems::mainHand(mob);
    world::LoadedWorld* world = worldOf(mob);
    if (!held.has_value() || world == nullptr) {
        return false;
    }
    random::RandomSource random = freshRandom();
    const float radians = std::numbers::pi_v<float> / 180.0F;
    const float direction = random.nextFloat() * (std::numbers::pi_v<float> * 2.0F);
    const float spread = 0.02F * random.nextFloat();
    const float yaw = mob.yaw * radians;
    const float pitch = mob.pitch * radians;
    world::DroppedItem dropped{.entityId = players_->allocateEntityId(),
                               .uuid = wire::common::Uuid::random(),
                               .item = held->item,
                               .count = held->count,
                               .x = mob.x,
                               .y = mob.y + entity::mobBreathing::eyeHeightOf(mob) - static_cast<double>(0.3F),
                               .z = mob.z,
                               .components = held->components};
    dropped.velocityX = static_cast<double>((0.3F * -mth::sin(yaw) * mth::cos(pitch)) + (mth::cos(direction) * spread));
    dropped.velocityY = static_cast<double>(0.3F * mth::sin(pitch) * 1.5F);
    dropped.velocityZ = static_cast<double>((0.3F * mth::cos(yaw) * mth::cos(pitch)) + (mth::sin(direction) * spread));
    dropped.pickupDelay = kThrownPickupDelay;
    dropped.thrower = mob.uuid();
    world->state().spawnItem(std::move(dropped));
    entity::copperGolems::setMainHand(mob, std::nullopt, false);
    heldItemChanged(mob);
    return true;
}

std::optional<std::array<std::int32_t, 3>> MobActionSystem::nearestTreasure(const ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return std::nullopt;
    }
    // parity: `#dolphin_located`, which is the ocean ruins and the shipwrecks.
    constexpr std::array kLocated{"ocean_ruin_cold", "ocean_ruin_warm", "shipwreck", "shipwreck_beached"};
    const std::int32_t bx = mth::floor(mob.x);
    const std::int32_t by = mth::floor(mob.y);
    const std::int32_t bz = mth::floor(mob.z);
    std::optional<std::array<std::int32_t, 3>> nearest;
    double best = std::numeric_limits<double>::max();
    for (const world::StructureLocator& locator : world->structureLocators()) {
        if (std::ranges::find(kLocated, locator.id.path()) == kLocated.end()) {
            continue;
        }
        const std::optional<std::pair<std::int32_t, std::int32_t>> start =
            locator.locate(world->metadata().seed, bx >> 4, bz >> 4, kTreasureRadius, {});
        if (!start.has_value()) {
            continue;
        }
        // parity: `StructurePlacement.getLocatePos`, the start chunk's corner at height zero.
        const std::array<std::int32_t, 3> at{start->first * 16, 0, start->second * 16};
        const double dx = at[0] - bx;
        const double dy = at[1] - by;
        const double dz = at[2] - bz;
        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
        if (distance < best) {
            best = distance;
            nearest = at;
        }
    }
    return nearest;
}

void MobActionSystem::entityEvent(ServerEntity& mob, std::int8_t event) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const wire::protocol::play::EntityEventPacket packet{.entityId = mob.entityId(), .eventId = event};
    for (PlayerSession* player : watchersOf(*world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void MobActionSystem::dolphinPicksUp(world::LoadedWorld& world, ServerEntity& dolphin) {
    if (!dolphin.canPickUpLoot || dolphin.dying ||
        !entity::mobOverrides::griefing(dolphin, world.gameRules().get(world::gameRules::kMobGriefing)) ||
        entity::copperGolems::mainHand(dolphin).has_value()) {
        return;
    }
    // parity: `Mob.ITEM_PICKUP_REACH`, one block across and none up or down.
    const double half = dolphin.width() / 2.0;
    for (const world::DroppedItem& item :
         world.state().nearbyItems(dolphin.x, dolphin.y, dolphin.z, 2.0 + dolphin.width() + dolphin.height())) {
        constexpr double kItemHalf = 0.125;
        if (!item.canBePickedUpByMob() || item.count <= 0 || item.x + kItemHalf < dolphin.x - half - 1.0 ||
            item.x - kItemHalf > dolphin.x + half + 1.0 || item.z + kItemHalf < dolphin.z - half - 1.0 ||
            item.z - kItemHalf > dolphin.z + half + 1.0 || item.y + 0.25 < dolphin.y ||
            item.y > dolphin.y + dolphin.height()) {
            continue;
        }
        const std::optional<world::DroppedItem> taken = world.state().removeItem(item.entityId);
        if (!taken.has_value()) {
            continue;
        }
        world::thrownItems::pickedUpByMob(options_.advancementTriggers, world, watchersOf(world), *taken, taken->count,
                                          dolphin);
        // parity: the whole stack into the mouth, and dropped on death whatever the odds.
        entity::copperGolems::setMainHand(
            dolphin,
            server::world::loot::ItemStack{.item = taken->item, .count = taken->count, .components = taken->components},
            true);
        heldItemChanged(dolphin);
        const wire::protocol::play::TakeItemEntityPacket packet{.collectedEntityId = taken->entityId,
                                                                .collectorEntityId = dolphin.entityId(),
                                                                .pickupItemCount = taken->count};
        for (PlayerSession* player : watchersOf(world)) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(wire::protocol::play::TakeItemEntityPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        return;
    }
}

} // namespace basalt::server::runtime
