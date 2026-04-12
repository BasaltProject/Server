#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/EquipmentTables.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace basalt::server::runtime::world::equipmentTables {
namespace {

using server::world::loot::ItemStack;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

/// parity: the item heads and skulls, all `equippableUnswappable(HEAD)`.
constexpr auto kHeads =
    std::to_array<std::string_view>({"carved_pumpkin", "skeleton_skull", "wither_skeleton_skull", "player_head",
                                     "zombie_head", "creeper_head", "dragon_head", "piglin_head"});

/// parity: `DropChances.DEFAULT_EQUIPMENT_DROP_CHANCE`, which is never written down.
constexpr float kDefaultDropChance = 0.085F;

/// parity: `EquipmentSlot.countLimit`, one for everything but the two hands.
[[nodiscard]] bool limitsToOne(std::string_view slot) { return slot != "mainhand" && slot != "offhand"; }

[[nodiscard]] NbtCompound compoundAt(const NbtCompound& root, std::string_view key) {
    const NbtCompound* found = root.getCompound(key);
    return found == nullptr ? NbtCompound{} : *found;
}

/// The slot an item's own `equippable` component names, before any stack changes it.
[[nodiscard]] std::optional<std::string_view> defaultSlot(const wire::common::Identifier& item) {
    if (item.namespaceName() != wire::common::Identifier::kVanillaNamespace) {
        return std::nullopt;
    }
    if (const server::registry::ArmourPiece* armour = server::registry::itemArmour::of(item); armour != nullptr) {
        return armour->slot;
    }
    const std::string_view path = item.path();
    if (std::ranges::find(kHeads, path) != kHeads.end()) {
        return "head";
    }
    if (path == "elytra") {
        return "chest";
    }
    if (path == "shield") {
        return "offhand";
    }
    if (path == "saddle") {
        return "saddle";
    }
    // parity: `Equippable.harness` and `Equippable.llamaSwag`, one per dye colour. The moss carpets
    // are not dyed and are not worn.
    if (path.ends_with("_harness") ||
        (path.ends_with("_carpet") && path != "moss_carpet" && path != "pale_moss_carpet")) {
        return "body";
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string_view> equippableSlot(const ItemStack& stack) {
    // parity: a stack's own components win over the item's: a removed `equippable` takes the slot
    // away, and a patched one names its own.
    if (stack.components.contains("!minecraft:equippable")) {
        return std::nullopt;
    }
    if (const NbtCompound* patched = stack.components.getCompound("minecraft:equippable"); patched != nullptr) {
        const std::string_view slot = patched->getString("slot");
        const auto known = std::ranges::find(baseSpawnerData::kSlots, slot);
        return known == baseSpawnerData::kSlots.end() ? std::nullopt : std::optional{*known};
    }
    return defaultSlot(stack.item);
}

void equip(NbtCompound& data, const EquipmentTable& table, const std::vector<ItemStack>& stacks) {
    std::vector<std::string_view> filled;
    NbtCompound equipment = compoundAt(data, "equipment");
    NbtCompound chances = compoundAt(data, "drop_chances");
    for (const ItemStack& stack : stacks) {
        if (stack.count <= 0 || stack.item.path() == "air") {
            continue;
        }
        const std::optional<std::string_view> worn = equippableSlot(stack);
        const std::string_view slot = worn.value_or("mainhand");
        if (std::ranges::find(filled, slot) != filled.end()) {
            continue;
        }
        NbtCompound item;
        item.putString("id", stack.item.toString());
        item.putInt("count", limitsToOne(slot) ? 1 : stack.count);
        if (!stack.components.empty()) {
            item.set("components", NbtTag::ofCompound(stack.components));
        }
        equipment.set(slot, NbtTag::ofCompound(std::move(item)));
        if (const std::optional<float> chance = table.dropChanceOf(slot); chance.has_value() && *chance >= 0.0F) {
            if (*chance == kDefaultDropChance) {
                static_cast<void>(chances.remove(slot));
            } else {
                chances.putFloat(slot, *chance);
            }
        }
        filled.push_back(slot);
    }
    if (filled.empty()) {
        return;
    }
    data.set("equipment", NbtTag::ofCompound(std::move(equipment)));
    if (chances.empty()) {
        static_cast<void>(data.remove("drop_chances"));
    } else {
        data.set("drop_chances", NbtTag::ofCompound(std::move(chances)));
    }
}

} // namespace basalt::server::runtime::world::equipmentTables
