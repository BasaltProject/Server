#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

[[nodiscard]] const NbtCompound* compoundAt(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    return found == nullptr ? nullptr : found->get<NbtCompound>();
}

[[nodiscard]] double doubleAt(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    if (found == nullptr) {
        return 0.0;
    }
    if (const auto* value = found->get<double>(); value != nullptr) {
        return *value;
    }
    if (const auto* value = found->get<float>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    return 0.0;
}

/// parity: `LevelBasedValue.Linear`, the shape every damage effect uses.
[[nodiscard]] float amountOf(const NbtCompound* value, std::int32_t level) {
    if (value == nullptr) {
        return 0.0F;
    }
    const auto base = static_cast<float>(doubleAt(*value, "base"));
    return base + static_cast<float>(doubleAt(*value, "per_level_above_first")) * static_cast<float>(level - 1);
}

[[nodiscard]] float apply(const NbtCompound* effect, float damage, std::int32_t level) {
    if (effect == nullptr) {
        return damage;
    }
    const float amount = amountOf(compoundAt(*effect, "value"), level);
    const NbtTag* type = effect->find("type");
    const auto* name = type == nullptr ? nullptr : type->get<std::string>();
    if (name == nullptr) {
        return damage;
    }
    if (*name == "minecraft:add") {
        return damage + amount;
    }
    if (*name == "minecraft:multiply") {
        return damage * amount;
    }
    return damage;
}

/// parity: the `entity_properties` requirement on `this` that the damage enchantments carry: an
/// entity type or a tag of them. Any other requirement is not something this knows how to read.
[[nodiscard]] bool meets(const NbtCompound* requirement, const weaponDamage::Victim* victim) {
    if (requirement == nullptr) {
        return true;
    }
    const NbtTag* kind = requirement->find("type");
    const auto* name = kind == nullptr ? nullptr : kind->get<std::string>();
    const NbtCompound* predicate = compoundAt(*requirement, "predicate");
    const NbtTag* entity = requirement->find("entity");
    const auto* who = entity == nullptr ? nullptr : entity->get<std::string>();
    if (victim == nullptr || name == nullptr || *name != "minecraft:entity_properties" || predicate == nullptr ||
        who == nullptr || *who != "this") {
        return false;
    }
    const NbtTag* type = predicate->find("minecraft:entity_type");
    if (type == nullptr) {
        type = predicate->find("type");
    }
    const auto* wanted = type == nullptr ? nullptr : type->get<std::string>();
    if (wanted == nullptr || predicate->size() != 1) {
        return false;
    }
    if (wanted->starts_with('#')) {
        const auto tag = wire::common::Identifier::parse(std::string_view{*wanted}.substr(1));
        return tag.has_value() && victim->inTag && victim->inTag(*tag);
    }
    const auto id = wire::common::Identifier::parse(*wanted);
    return id.has_value() && *id == victim->type;
}

constexpr std::array<std::string_view, 4> kArmourSuffixes{"_helmet", "_chestplate", "_leggings", "_boots"};
constexpr std::array<std::string_view, 6> kToolSuffixes{"_sword", "_shovel", "_pickaxe", "_axe", "_hoe", "_spear"};

[[nodiscard]] bool endsWithAny(std::string_view path, std::span<const std::string_view> suffixes) {
    return std::ranges::any_of(suffixes, [path](std::string_view suffix) { return path.ends_with(suffix); });
}

[[nodiscard]] std::optional<std::int32_t>
valueByMaterial(std::string_view path, std::span<const std::pair<std::string_view, std::int32_t>> materials) {
    for (const auto& [prefix, value] : materials) {
        if (path.starts_with(prefix)) {
            return value;
        }
    }
    return std::nullopt;
}

} // namespace

namespace weaponDamage {

float of(const std::optional<storage::StoredItemStack>& stack, float base, const DefinitionOf& definitionOf,
         const Victim* victim) {
    if (!stack.has_value() || !definitionOf) {
        return base;
    }
    float damage = base;
    for (const auto& [id, level] : player::enchantmentLevels(*stack)) {
        const NbtCompound* definition = definitionOf(id);
        if (definition == nullptr) {
            continue;
        }
        const NbtCompound* effects = compoundAt(*definition, "effects");
        if (effects == nullptr) {
            continue;
        }
        const NbtTag* found = effects->find("minecraft:damage");
        const auto* entries = found == nullptr ? nullptr : found->get<NbtList>();
        if (entries == nullptr) {
            continue;
        }
        for (const NbtTag& entry : entries->elements()) {
            const auto* compound = entry.get<NbtCompound>();
            if (compound == nullptr || !meets(compoundAt(*compound, "requirements"), victim)) {
                continue;
            }
            damage = apply(compoundAt(*compound, "effect"), damage, level);
        }
    }
    return damage;
}

float valueEffect(const storage::StoredItemStack& stack, std::string_view component, float base,
                  const DefinitionOf& definitionOf) {
    float value = base;
    if (!definitionOf) {
        return value;
    }
    for (const auto& [id, level] : player::enchantmentLevels(stack)) {
        const NbtCompound* definition = definitionOf(id);
        const NbtCompound* effects = definition == nullptr ? nullptr : compoundAt(*definition, "effects");
        const NbtTag* found = effects == nullptr ? nullptr : effects->find(component);
        if (found == nullptr) {
            continue;
        }
        // parity: a plain value effect is one object; a conditional one is a list of `effect` entries.
        if (const auto* single = found->get<NbtCompound>(); single != nullptr) {
            value = apply(single, value, level);
            continue;
        }
        if (const auto* entries = found->get<NbtList>(); entries != nullptr) {
            for (const NbtTag& entry : entries->elements()) {
                const auto* compound = entry.get<NbtCompound>();
                if (compound != nullptr && compound->find("requirements") == nullptr) {
                    value = apply(compoundAt(*compound, "effect"), value, level);
                }
            }
        }
    }
    return value;
}

std::optional<std::string> highestLevelEntry(const storage::StoredItemStack& stack, std::string_view component,
                                             const DefinitionOf& definitionOf) {
    std::optional<std::string> best;
    std::int32_t bestLevel = 0;
    if (!definitionOf) {
        return best;
    }
    for (const auto& [id, level] : player::enchantmentLevels(stack)) {
        const NbtCompound* definition = definitionOf(id);
        const NbtCompound* effects = definition == nullptr ? nullptr : compoundAt(*definition, "effects");
        const NbtTag* found = effects == nullptr ? nullptr : effects->find(component);
        const auto* entries = found == nullptr ? nullptr : found->get<NbtList>();
        if (entries == nullptr || entries->empty() || level <= bestLevel) {
            continue;
        }
        // parity: the entry for the level, or the last one past the end of the list.
        const auto index =
            static_cast<std::size_t>(std::clamp(level - 1, 0, static_cast<std::int32_t>(entries->size()) - 1));
        if (const auto* sound = (*entries)[index].get<std::string>(); sound != nullptr) {
            best = *sound;
            bestLevel = level;
        }
    }
    return best;
}

} // namespace weaponDamage

namespace vanillaItemEnchantability {

std::optional<std::int32_t> valueOf(std::string_view item) {
    static const std::map<std::string_view, std::int32_t> kSpecial{{"book", 1},         {"bow", 1},   {"crossbow", 1},
                                                                   {"fishing_rod", 1},  {"mace", 15}, {"trident", 1},
                                                                   {"turtle_helmet", 9}};
    static constexpr std::array<std::pair<std::string_view, std::int32_t>, 7> kArmour{{{"leather_", 15},
                                                                                       {"copper_", 8},
                                                                                       {"chainmail_", 12},
                                                                                       {"iron_", 9},
                                                                                       {"golden_", 25},
                                                                                       {"diamond_", 10},
                                                                                       {"netherite_", 15}}};
    static constexpr std::array<std::pair<std::string_view, std::int32_t>, 7> kTools{{{"wooden_", 15},
                                                                                      {"stone_", 5},
                                                                                      {"copper_", 13},
                                                                                      {"iron_", 14},
                                                                                      {"golden_", 22},
                                                                                      {"diamond_", 10},
                                                                                      {"netherite_", 15}}};

    std::string_view path = item;
    if (path.starts_with("minecraft:")) {
        path.remove_prefix(10);
    }
    if (const auto found = kSpecial.find(path); found != kSpecial.end()) {
        return found->second;
    }
    if (endsWithAny(path, kArmourSuffixes)) {
        return valueByMaterial(path, kArmour);
    }
    if (endsWithAny(path, kToolSuffixes)) {
        return valueByMaterial(path, kTools);
    }
    return std::nullopt;
}

} // namespace vanillaItemEnchantability

} // namespace basalt::server::runtime::world
