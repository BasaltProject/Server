#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <array>
#include <string>

namespace basalt::server::runtime::entity::mobEquipment {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr std::array<std::string_view, 8> kKeys{"mainhand", "offhand", "feet", "legs",
                                                "chest",    "head",    "body", "saddle"};
/// parity: `Mob`'s guaranteed drop chance, which `setItemSlotAndDropWhenKilled` writes.
constexpr float kGuaranteedDrop = 2.0F;

} // namespace

std::string_view keyOf(Slot slot) { return kKeys[static_cast<std::size_t>(slot)]; }

std::optional<server::world::loot::ItemStack> get(const ServerEntity& mob, Slot slot) {
    const NbtCompound* equipment = mob.sourceNbt.has_value() ? mob.sourceNbt->getCompound("equipment") : nullptr;
    const NbtCompound* held = equipment == nullptr ? nullptr : equipment->getCompound(keyOf(slot));
    return held == nullptr ? std::nullopt : world::blockInventory::readStack(*held);
}

void set(ServerEntity& mob, Slot slot, const std::optional<server::world::loot::ItemStack>& stack, bool guaranteed) {
    if (!mob.sourceNbt.has_value()) {
        mob.sourceNbt.emplace();
    }
    NbtCompound equipment;
    if (const NbtCompound* existing = mob.sourceNbt->getCompound("equipment"); existing != nullptr) {
        equipment = *existing;
    }
    const std::string key{keyOf(slot)};
    if (stack.has_value() && stack->count > 0) {
        equipment.set(key, NbtTag::ofCompound(world::blockInventory::writeStack(*stack)));
    } else {
        static_cast<void>(equipment.remove(key));
    }
    mob.sourceNbt->set("equipment", NbtTag::ofCompound(std::move(equipment)));
    if (guaranteed) {
        NbtCompound chances;
        if (const NbtCompound* existing = mob.sourceNbt->getCompound("drop_chances"); existing != nullptr) {
            chances = *existing;
        }
        chances.putFloat(key, kGuaranteedDrop);
        mob.sourceNbt->set("drop_chances", NbtTag::ofCompound(std::move(chances)));
    }
}

bool holds(const ServerEntity& mob, std::string_view itemPath) {
    for (const Slot slot : {Slot::MainHand, Slot::OffHand}) {
        if (const std::optional<server::world::loot::ItemStack> held = get(mob, slot);
            held.has_value() && held->item.path() == itemPath) {
            return true;
        }
    }
    return false;
}

} // namespace basalt::server::runtime::entity::mobEquipment
