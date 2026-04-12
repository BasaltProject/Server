#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <array>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using entity::mobEquipment::Slot;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;

/// parity: `EquipmentSlot.VALUES`, the order the first shearable piece is looked for in.
constexpr std::array<Slot, 8> kSlots{Slot::MainHand, Slot::OffHand, Slot::Feet, Slot::Legs,
                                     Slot::Chest,    Slot::Head,    Slot::Body, Slot::Saddle};

/// parity: the `can_be_sheared` equippables and their `shearing_sound`: saddles, harnesses,
/// llama carpets and the horse, wolf and nautilus armours.
[[nodiscard]] std::optional<std::string_view> shearingSound(const Identifier& item) {
    const std::string_view path = item.path();
    if (path == "saddle") {
        return "minecraft:item.saddle.unequip";
    }
    if (path.ends_with("_harness")) {
        return "minecraft:entity.happy_ghast.unequip";
    }
    if (path.ends_with("_carpet") && path != "moss_carpet" && path != "pale_moss_carpet") {
        return "minecraft:item.llama_carpet.unequip";
    }
    if (path.ends_with("_horse_armor")) {
        return "minecraft:item.horse_armor.unequip";
    }
    if (path == "wolf_armor") {
        return "minecraft:item.armor.unequip_wolf";
    }
    if (path.ends_with("_nautilus_armor")) {
        return "minecraft:item.armor.unequip_nautilus";
    }
    return std::nullopt;
}

/// parity: `getAttachments().getAverage(PASSENGER)` for the mobs that wear any of those.
[[nodiscard]] std::array<double, 3> passengerPoint(const ServerEntity& mob) {
    const std::string_view path = mob.type().path();
    if (path == "horse") {
        return {0.0, static_cast<double>(1.44375F), 0.0};
    }
    if (path == "donkey") {
        return {0.0, static_cast<double>(1.1125F), 0.0};
    }
    if (path == "mule") {
        return {0.0, static_cast<double>(1.2125F), 0.0};
    }
    if (path == "skeleton_horse" || path == "zombie_horse") {
        return {0.0, static_cast<double>(1.31875F), 0.0};
    }
    if (path == "llama" || path == "trader_llama") {
        return {0.0, 1.37, -0.3};
    }
    if (path == "pig") {
        return {0.0, static_cast<double>(0.86875F), 0.0};
    }
    if (path == "wolf") {
        return {0.0, 0.81875, static_cast<double>(-0.0625F)};
    }
    if (path == "nautilus" || path == "zombie_nautilus") {
        return {0.0, static_cast<double>(1.1375F), 0.0};
    }
    if (path == "happy_ghast") {
        return {0.0, 4.0, 0.0};
    }
    return {0.0, mob.height(), 0.0};
}

} // namespace

bool CombatHandler::shearEquipment(PlayerSession& session, ServerEntity& mob, std::int32_t handSlot) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<storage::StoredItemStack> shears = session.inventory.at(handSlot);
    if (world == nullptr || !shears.has_value() || shears->id != "minecraft:shears" || session.sneaking ||
        !server::registry::entityAttributes::isLiving(mob.type()) || mob.type().path() == "armor_stand") {
        return false;
    }
    // parity: `canShearEquipment`, nobody riding it, and a wolf only for its owner.
    if (mob.type().path() == "wolf" ? mob.owner != session.uuid().toString() : !mob.passengerIds.empty()) {
        return false;
    }
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    for (const Slot slot : kSlots) {
        const std::optional<server::world::loot::ItemStack> worn = entity::mobEquipment::get(mob, slot);
        const std::optional<std::string_view> sound = worn.has_value() ? shearingSound(worn->item) : std::nullopt;
        if (!sound.has_value()) {
            continue;
        }
        // parity: `PREVENT_ARMOR_CHANGE`, binding, keeps it on for anyone but a creative player.
        const storage::StoredItemStack stored{
            .id = worn->item.toString(), .count = worn->count, .components = worn->components};
        bool bound = false;
        for (const auto& [id, level] : player::enchantmentLevels(stored)) {
            const wire::nbt::NbtCompound* definition =
                enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
            const wire::nbt::NbtCompound* effects =
                definition == nullptr ? nullptr : definition->getCompound("effects");
            bound = bound || (effects != nullptr && effects->contains("minecraft:prevent_armor_change"));
        }
        if (bound && session.gamemode != Gamemode::Creative) {
            continue;
        }
        // parity: `shearItem`: the shears worn, the piece off, `SHEAR` and the piece dropped at the
        // rider's seat, then the trigger and the equippable's own sound.
        player::wearItemIn(owner_->blockInteraction().breakContext(), session, handSlot, 1);
        if (slot == Slot::Body || slot == Slot::Saddle) {
            equipMount(mob, slot, std::nullopt);
        } else {
            entity::mobEquipment::set(mob, slot, std::nullopt, false);
        }
        world::vibrations::emit(world->state(), "shear", mob.x, mob.y, mob.z, world->gameTime(),
                                {.entity = session.uuid()});
        const std::array<double, 3> seat = passengerPoint(mob);
        const double spreadX = options_.nextDouble ? options_.nextDouble() : 0.5;
        const double spreadZ = options_.nextDouble ? options_.nextDouble() : 0.5;
        owner_->blockInteraction().spawnDroppedItem(worn->item, worn->count, mob.x + seat[0], mob.y + seat[1],
                                                    mob.z + seat[2], (spreadX * 0.2) - 0.1, 0.2, (spreadZ * 0.2) - 0.1,
                                                    world::kSpawnedPickupDelay, worn->components);
        if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
            world::TriggerEvent event;
            event.entities.emplace("entity", world::triggerFacts::ofEntity(*world, mob));
            event.items.emplace(
                "item", world::TriggerItem{.item = worn->item, .count = worn->count, .components = worn->components});
            triggers->fire("player_sheared_equipment", session.name(), session.uuid().toString(), event);
        }
        mountSound(mob, *sound, 1.0F, 1.0F);
        return true;
    }
    return false;
}

} // namespace basalt::server::runtime
