#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <string_view>

namespace basalt::server::runtime {
namespace {

using server::world::loot::ItemStack;
namespace equipment = entity::mobEquipment;

[[nodiscard]] equipment::Slot slotOf(const ItemStack& stack) {
    const std::string_view path = stack.item.path();
    if (path.ends_with("_helmet") || path == "carved_pumpkin") {
        return equipment::Slot::Head;
    }
    if (path.ends_with("_chestplate") || path == "elytra") {
        return equipment::Slot::Chest;
    }
    if (path.ends_with("_leggings")) {
        return equipment::Slot::Legs;
    }
    if (path.ends_with("_boots")) {
        return equipment::Slot::Feet;
    }
    return path == "shield" ? equipment::Slot::OffHand : equipment::Slot::MainHand;
}

[[nodiscard]] bool handledElsewhere(std::string_view type) {
    return type == "dolphin" || type == "piglin" || type == "piglin_brute" || type == "allay" ||
           type == "copper_golem" || type == "villager";
}

} // namespace

void MobActionSystem::mobPicksUpLoot(world::LoadedWorld& world, entity::ServerEntity& mob) {
    if (mob.tuning == nullptr || mob.tuning->canPickUpLoot != true || mob.dying ||
        handledElsewhere(mob.type().path()) ||
        !entity::mobOverrides::griefing(mob, world.gameRules().get(world::gameRules::kMobGriefing))) {
        return;
    }
    // parity: `Mob.ITEM_PICKUP_REACH`, a block across and none up or down.
    const double half = mob.width() / 2.0;
    for (const world::DroppedItem& item :
         world.state().nearbyItems(mob.x, mob.y, mob.z, 2.0 + mob.width() + mob.height())) {
        constexpr double kItemHalf = 0.125;
        if (!item.canBePickedUpByMob() || item.count <= 0 || item.x + kItemHalf < mob.x - half - 1.0 ||
            item.x - kItemHalf > mob.x + half + 1.0 || item.z + kItemHalf < mob.z - half - 1.0 ||
            item.z - kItemHalf > mob.z + half + 1.0 || item.y + 0.25 < mob.y || item.y > mob.y + mob.height()) {
            continue;
        }
        const ItemStack whole{.item = item.item, .count = item.count, .components = item.components};
        const equipment::Slot slot = slotOf(whole);
        if (equipment::get(mob, slot).has_value()) {
            continue;
        }
        equipment::set(mob, slot, whole, true);
        equipmentChanged(mob, static_cast<std::uint8_t>(slot));
        const wire::protocol::play::TakeItemEntityPacket packet{
            .collectedEntityId = item.entityId, .collectorEntityId = mob.entityId(), .pickupItemCount = whole.count};
        for (player::PlayerSession* player : watchersOf(world)) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(wire::protocol::play::TakeItemEntityPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        static_cast<void>(world.state().removeItem(item.entityId));
        return;
    }
}

} // namespace basalt::server::runtime
