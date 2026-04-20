#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <utility>

namespace basalt::server::runtime::player::playerAttributes {
namespace {

namespace reg = server::registry;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

[[nodiscard]] std::string fullName(std::string_view attribute) {
    return attribute.find(':') == std::string_view::npos ? "minecraft:" + std::string{attribute}
                                                         : std::string{attribute};
}

[[nodiscard]] std::optional<Identifier> idOf(std::string_view text) {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(text);
    return id.has_value() ? std::optional{*id} : std::nullopt;
}

[[nodiscard]] Operation operationOf(std::string_view name) {
    if (name == "add_multiplied_base") {
        return Operation::AddMultipliedBase;
    }
    return name == "add_multiplied_total" ? Operation::AddMultipliedTotal : Operation::AddValue;
}

/// parity: `EquipmentSlotGroup.test`.
[[nodiscard]] bool groupHolds(std::string_view group, std::string_view slot) {
    if (group.empty() || group == "any") {
        return true;
    }
    if (group == "hand") {
        return slot == "mainhand" || slot == "offhand";
    }
    if (group == "armor") {
        return slot == "head" || slot == "chest" || slot == "legs" || slot == "feet" || slot == "body";
    }
    return group == slot;
}

/// parity: `ArmorType.getName`, which names the default armour modifier.
[[nodiscard]] std::string_view armourTypeOf(std::string_view slot) {
    if (slot == "head") {
        return "helmet";
    }
    if (slot == "chest") {
        return "chestplate";
    }
    if (slot == "legs") {
        return "leggings";
    }
    return slot == "feet" ? "boots" : "body";
}

/// Puts a modifier in, replacing one of the same id. parity: `AttributeInstance.addTransientModifier`.
void put(Modifiers& into, const std::string& attribute, Modifier modifier) {
    std::vector<Modifier>& list = into[attribute];
    std::erase_if(list, [&modifier](const Modifier& held) { return held.id == modifier.id; });
    list.push_back(std::move(modifier));
}

/// parity: the default `attribute_modifiers` of armour and of weapons and tools.
void addDefaults(const Identifier& item, std::string_view slot, Modifiers& into) {
    if (const reg::ArmourPiece* piece = reg::itemArmour::of(item); piece != nullptr && piece->slot == slot) {
        const Identifier id = Identifier::vanilla("armor." + std::string{armourTypeOf(slot)});
        put(into, "minecraft:armor",
            Modifier{.id = id, .amount = static_cast<double>(piece->armour), .operation = Operation::AddValue});
        put(into, "minecraft:armor_toughness",
            Modifier{.id = id, .amount = static_cast<double>(piece->toughness), .operation = Operation::AddValue});
        if (piece->knockbackResistance > 0.0F) {
            put(into, "minecraft:knockback_resistance",
                Modifier{.id = id,
                         .amount = static_cast<double>(piece->knockbackResistance),
                         .operation = Operation::AddValue});
        }
    }
    if (const reg::WeaponStats* weapon = reg::itemWeapons::of(item); weapon != nullptr && slot == "mainhand") {
        put(into, "minecraft:attack_damage",
            Modifier{.id = Identifier::vanilla("base_attack_damage"),
                     .amount = weapon->attackDamage,
                     .operation = Operation::AddValue});
        put(into, "minecraft:attack_speed",
            Modifier{.id = Identifier::vanilla("base_attack_speed"),
                     .amount = weapon->attackSpeed,
                     .operation = Operation::AddValue});
    }
}

/// parity: `LevelBasedValue.calculate`, a float.
[[nodiscard]] float levelValue(const NbtTag* value, std::int32_t level) {
    if (value == nullptr) {
        return 0.0F;
    }
    if (const float* constant = value->get<float>(); constant != nullptr) {
        return *constant;
    }
    if (const double* constant = value->get<double>(); constant != nullptr) {
        return static_cast<float>(*constant);
    }
    if (const std::int32_t* constant = value->get<std::int32_t>(); constant != nullptr) {
        return static_cast<float>(*constant);
    }
    const NbtCompound* shaped = value->get<NbtCompound>();
    if (shaped == nullptr) {
        return 0.0F;
    }
    const std::string_view type = shaped->getString("type");
    if (type == "minecraft:linear") {
        return shaped->getFloat("base") + (shaped->getFloat("per_level_above_first") * static_cast<float>(level - 1));
    }
    if (type == "minecraft:levels_squared") {
        return static_cast<float>(level * level) + shaped->getFloat("added");
    }
    if (type == "minecraft:clamped") {
        return std::clamp(levelValue(shaped->find("value"), level), shaped->getFloat("min"), shaped->getFloat("max"));
    }
    if (type == "minecraft:fraction") {
        const float denominator = levelValue(shaped->find("denominator"), level);
        return denominator == 0.0F ? 0.0F : levelValue(shaped->find("numerator"), level) / denominator;
    }
    return shaped->getFloat("value");
}

/// parity: `EnchantmentHelper.forEachModifier(stack, slot)`.
void addEnchantments(const storage::StoredItemStack& stack, std::string_view slot, const DefinitionOf& definitionOf,
                     Modifiers& into) {
    if (!definitionOf) {
        return;
    }
    for (const auto& [enchantment, level] : enchantmentLevels(stack)) {
        const NbtCompound* definition = definitionOf(enchantment);
        const NbtList* slots = definition == nullptr ? nullptr : definition->getList("slots");
        const NbtCompound* effects = definition == nullptr ? nullptr : definition->getCompound("effects");
        const NbtList* attributes = effects == nullptr ? nullptr : effects->getList("minecraft:attributes");
        if (slots == nullptr || attributes == nullptr) {
            continue;
        }
        const bool matches = std::ranges::any_of(*slots, [slot](const NbtTag& group) {
            const std::string* name = group.get<std::string>();
            return name != nullptr && groupHolds(*name, slot);
        });
        if (!matches) {
            continue;
        }
        for (const NbtTag& entry : *attributes) {
            const NbtCompound* effect = entry.get<NbtCompound>();
            if (effect == nullptr) {
                continue;
            }
            const std::optional<Identifier> id = idOf(std::string{effect->getString("id")} + "/" + std::string{slot});
            if (!id.has_value() || effect->getString("attribute").empty()) {
                continue;
            }
            put(into, fullName(effect->getString("attribute")),
                Modifier{.id = *id,
                         .amount = static_cast<double>(levelValue(effect->find("amount"), level)),
                         .operation = operationOf(effect->getString("operation"))});
        }
    }
}

/// parity: `MobEffects`' attribute templates, each worth its amount times the level.
struct EffectTemplate {
    std::string_view effect;
    std::string_view attribute;
    std::string_view id;
    float amount;
    Operation operation;
};

constexpr std::array<EffectTemplate, 12> kEffectModifiers{{
    {"speed", "minecraft:movement_speed", "effect.speed", 0.2F, Operation::AddMultipliedTotal},
    {"slowness", "minecraft:movement_speed", "effect.slowness", -0.15F, Operation::AddMultipliedTotal},
    {"haste", "minecraft:attack_speed", "effect.haste", 0.1F, Operation::AddMultipliedTotal},
    {"mining_fatigue", "minecraft:attack_speed", "effect.mining_fatigue", -0.1F, Operation::AddMultipliedTotal},
    {"strength", "minecraft:attack_damage", "effect.strength", 3.0F, Operation::AddValue},
    {"jump_boost", "minecraft:safe_fall_distance", "effect.jump_boost", 1.0F, Operation::AddValue},
    {"invisibility", "minecraft:waypoint_transmit_range", "effect.waypoint_transmit_range_hide", -1.0F,
     Operation::AddMultipliedTotal},
    {"weakness", "minecraft:attack_damage", "effect.weakness", -4.0F, Operation::AddValue},
    {"health_boost", "minecraft:max_health", "effect.health_boost", 4.0F, Operation::AddValue},
    {"absorption", "minecraft:max_absorption", "effect.absorption", 4.0F, Operation::AddValue},
    {"luck", "minecraft:luck", "effect.luck", 1.0F, Operation::AddValue},
    {"unluck", "minecraft:luck", "effect.unluck", -1.0F, Operation::AddValue},
}};

/// parity: `RangedAttribute`'s bounds for the attributes this server reads.
[[nodiscard]] std::pair<double, double> rangeOf(std::string_view attribute) {
    if (attribute == "minecraft:armor") {
        return {0.0, 30.0};
    }
    if (attribute == "minecraft:armor_toughness") {
        return {0.0, 20.0};
    }
    if (attribute == "minecraft:knockback_resistance") {
        return {0.0, 1.0};
    }
    if (attribute == "minecraft:max_health") {
        return {1.0, 1024.0};
    }
    if (attribute == "minecraft:attack_damage") {
        return {0.0, 2048.0};
    }
    if (attribute == "minecraft:luck") {
        return {-1024.0, 1024.0};
    }
    return {-std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
}

} // namespace

void addStackModifiers(const storage::StoredItemStack& stack, std::string_view slot, const DefinitionOf& definitionOf,
                       Modifiers& into) {
    const std::optional<Identifier> item = idOf(stack.id);
    if (stack.count <= 0 || !item.has_value()) {
        return;
    }
    if (const NbtList* entries = stack.components.getList("minecraft:attribute_modifiers"); entries != nullptr) {
        for (const NbtTag& element : *entries) {
            const NbtCompound* entry = element.get<NbtCompound>();
            const std::optional<Identifier> id = entry == nullptr ? std::nullopt : idOf(entry->getString("id"));
            if (!id.has_value() || !groupHolds(entry->getString("slot"), slot)) {
                continue;
            }
            put(into, fullName(entry->getString("type")),
                Modifier{.id = *id,
                         .amount = entry->getDouble("amount"),
                         .operation = operationOf(entry->getString("operation"))});
        }
    } else {
        addDefaults(*item, slot, into);
    }
    addEnchantments(stack, slot, definitionOf, into);
}

Modifiers modifiersOf(const PlayerSession& player, const DefinitionOf& definitionOf) {
    Modifiers modifiers;
    const PlayerInventory& inventory = player.inventory;
    const std::array<std::pair<std::int32_t, std::string_view>, 6> slots{{
        {PlayerInventory::kHotbarStart + inventory.heldSlot(), "mainhand"},
        {PlayerInventory::kOffhandSlot, "offhand"},
        {PlayerInventory::kArmourStart + 3, "feet"},
        {PlayerInventory::kArmourStart + 2, "legs"},
        {PlayerInventory::kArmourStart + 1, "chest"},
        {PlayerInventory::kArmourStart, "head"},
    }};
    for (const auto& [index, slot] : slots) {
        const std::optional<storage::StoredItemStack>& stack = inventory.at(index);
        // parity: a broken stack gives nothing, `collectEquipmentChanges`' `isBroken` check.
        if (stack.has_value() && !(item::itemWear::isDamageable(*stack) &&
                                   item::itemWear::damage(*stack) >= item::itemWear::maxDamage(*stack))) {
            addStackModifiers(*stack, slot, definitionOf, modifiers);
        }
    }
    for (const auto& [id, active] : player.effects) {
        for (const EffectTemplate& effect : kEffectModifiers) {
            if (id.namespaceName() == "minecraft" && id.path() == effect.effect) {
                put(modifiers, std::string{effect.attribute},
                    Modifier{.id = Identifier::vanilla(std::string{effect.id}),
                             .amount = static_cast<double>(effect.amount) * static_cast<double>(active.amplifier + 1),
                             .operation = effect.operation});
            }
        }
    }
    return modifiers;
}

double baseOf(const PlayerSession& player, std::string_view attribute) {
    const std::string name = fullName(attribute);
    if (const auto set = player.attributeBases.find(name); set != player.attributeBases.end()) {
        return set->second;
    }
    return reg::entityAttributes::baseValue(Identifier::vanilla("player"), name).value_or(0.0);
}

double calculate(std::string_view attribute, double base, const std::vector<Modifier>& modifiers) {
    double added = base;
    for (const Modifier& modifier : modifiers) {
        if (modifier.operation == Operation::AddValue) {
            added += modifier.amount;
        }
    }
    double value = added;
    for (const Modifier& modifier : modifiers) {
        if (modifier.operation == Operation::AddMultipliedBase) {
            value += added * modifier.amount;
        }
    }
    for (const Modifier& modifier : modifiers) {
        if (modifier.operation == Operation::AddMultipliedTotal) {
            value *= 1.0 + modifier.amount;
        }
    }
    const auto [low, high] = rangeOf(fullName(attribute));
    return std::isnan(value) ? low : std::clamp(value, low, high);
}

double valueOf(const PlayerSession& player, std::string_view attribute, const DefinitionOf& definitionOf) {
    const std::string name = fullName(attribute);
    const Modifiers modifiers = modifiersOf(player, definitionOf);
    const auto found = modifiers.find(name);
    return calculate(name, baseOf(player, name), found == modifiers.end() ? std::vector<Modifier>{} : found->second);
}

} // namespace basalt::server::runtime::player::playerAttributes
