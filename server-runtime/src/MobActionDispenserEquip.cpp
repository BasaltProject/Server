#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/EquipmentTables.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime {

namespace {

namespace equipment = entity::mobEquipment;

using server::world::loot::ItemStack;

struct SlotName {
    std::string_view name;
    equipment::Slot slot;
};

constexpr std::array<SlotName, 4> kWorn{{{"feet", equipment::Slot::Feet},
                                         {"legs", equipment::Slot::Legs},
                                         {"chest", equipment::Slot::Chest},
                                         {"head", equipment::Slot::Head}}};

[[nodiscard]] std::optional<equipment::Slot> wornSlot(const ItemStack& stack) {
    const std::optional<std::string_view> name = world::equipmentTables::equippableSlot(stack);
    if (!name.has_value()) {
        return std::nullopt;
    }
    const auto found = std::ranges::find(kWorn, *name, &SlotName::name);
    return found == kWorn.end() ? std::nullopt : std::optional{found->slot};
}

[[nodiscard]] bool standDisables(const entity::ServerEntity& stand, equipment::Slot slot) {
    constexpr std::array<std::int32_t, 4> kBits{1, 2, 3, 4};
    const std::int32_t disabled = stand.sourceNbt.has_value() ? stand.sourceNbt->getInt("DisabledSlots") : 0;
    return ((disabled >> kBits[static_cast<std::size_t>(slot) - 2]) & 1) != 0;
}

} // namespace

std::optional<std::uint8_t> MobActionSystem::dispenserEquipSlot(const entity::ServerEntity& target,
                                                                const ItemStack& stack) const {
    if (target.removed || target.dying || !target.livingType()) {
        return std::nullopt;
    }
    const std::optional<equipment::Slot> slot = wornSlot(stack);
    if (!slot.has_value() || equipment::get(target, *slot).has_value()) {
        return std::nullopt;
    }
    // parity: `LivingEntity.canEquipWithDispenser`: a stand obeys its disabled slots, any other mob must pick up loot.
    const bool stand = target.type().path() == "armor_stand";
    if (stand ? standDisables(target, *slot) : !target.canPickUpLoot) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(*slot);
}

void MobActionSystem::dispenserEquip(entity::ServerEntity& target, std::uint8_t slot, const ItemStack& stack) {
    const ItemStack one{.item = stack.item, .count = 1, .components = stack.components};
    // parity: `EquipmentDispenseItemBehavior`: `setGuaranteedDrop` for a mob, nothing for a stand.
    equipment::set(target, static_cast<equipment::Slot>(slot), one, target.type().path() != "armor_stand");
    equipmentChanged(target, slot);
}

} // namespace basalt::server::runtime
