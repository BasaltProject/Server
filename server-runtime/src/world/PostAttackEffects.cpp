#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/PostAttackEffects.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace basalt::server::runtime::world::postAttack {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

[[nodiscard]] const NbtCompound* compoundAt(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    return found == nullptr ? nullptr : found->get<NbtCompound>();
}

[[nodiscard]] const std::string* stringAt(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    return found == nullptr ? nullptr : found->get<std::string>();
}

[[nodiscard]] std::optional<double> numberOf(const NbtTag* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    if (const auto* value = tag->get<double>(); value != nullptr) {
        return *value;
    }
    if (const auto* value = tag->get<float>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    if (const auto* value = tag->get<std::int32_t>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    if (const auto* value = tag->get<std::int8_t>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    return std::nullopt;
}

/// parity: `LevelBasedValue`: a constant, or `linear`.
[[nodiscard]] float levelBased(const NbtTag* tag, std::int32_t level) {
    if (const std::optional<double> constant = numberOf(tag); constant.has_value()) {
        return static_cast<float>(*constant);
    }
    const auto* value = tag == nullptr ? nullptr : tag->get<NbtCompound>();
    if (value == nullptr) {
        return 0.0F;
    }
    const auto base = static_cast<float>(numberOf(value->find("base")).value_or(0.0));
    const auto perLevel = static_cast<float>(numberOf(value->find("per_level_above_first")).value_or(0.0));
    return base + (perLevel * static_cast<float>(level - 1));
}

/// parity: `Mth.randomBetween`.
[[nodiscard]] float randomBetween(random::RandomSource& random, float min, float max) {
    return (random.nextFloat() * (max - min)) + min;
}

/// parity: `EquipmentSlotGroup.test`.
[[nodiscard]] bool groupHolds(std::string_view group, std::string_view slot) {
    if (group == "any") {
        return true;
    }
    if (group == "hand") {
        return slot == "mainhand" || slot == "offhand";
    }
    if (group == "armor") {
        return slot == "feet" || slot == "legs" || slot == "chest" || slot == "head" || slot == "body";
    }
    return group == slot;
}

[[nodiscard]] bool matchingSlot(const NbtCompound& definition, std::string_view slot) {
    const NbtTag* found = definition.find("slots");
    if (const auto* single = found == nullptr ? nullptr : found->get<std::string>(); single != nullptr) {
        return groupHolds(*single, slot);
    }
    const auto* list = found == nullptr ? nullptr : found->get<NbtList>();
    if (list == nullptr) {
        return false;
    }
    return std::ranges::any_of(list->elements(), [slot](const NbtTag& group) {
        const auto* name = group.get<std::string>();
        return name != nullptr && groupHolds(*name, slot);
    });
}

/// The `entity_properties` check on `this` the post-attack entries carry: an entity type or a tag.
[[nodiscard]] bool victimIs(const NbtCompound& predicate, const weaponDamage::Victim* victim) {
    const std::string* wanted = stringAt(predicate, "minecraft:entity_type");
    if (victim == nullptr || wanted == nullptr || predicate.size() != 1) {
        return false;
    }
    if (wanted->starts_with('#')) {
        const auto tag = Identifier::parse(std::string_view{*wanted}.substr(1));
        return tag.has_value() && victim->inTag && victim->inTag(*tag);
    }
    const auto id = Identifier::parse(*wanted);
    return id.has_value() && *id == victim->type;
}

/// parity: the loot conditions an enchanted-damage context can answer; anything else is unmet.
[[nodiscard]] bool meets(const NbtCompound* requirement, std::int32_t level, const Hit& hit,
                         random::RandomSource& random) {
    if (requirement == nullptr) {
        return true;
    }
    const std::string* type = stringAt(*requirement, "type");
    if (type == nullptr) {
        return false;
    }
    if (*type == "minecraft:all_of" || *type == "minecraft:any_of") {
        const NbtTag* found = requirement->find("terms");
        const auto* terms = found == nullptr ? nullptr : found->get<NbtList>();
        if (terms == nullptr) {
            return false;
        }
        const bool all = *type == "minecraft:all_of";
        for (const NbtTag& term : terms->elements()) {
            const bool met = meets(term.get<NbtCompound>(), level, hit, random);
            if (all != met) {
                return met;
            }
        }
        return all;
    }
    if (*type == "minecraft:inverted") {
        return !meets(compoundAt(*requirement, "term"), level, hit, random);
    }
    if (*type == "minecraft:random_chance") {
        const NbtTag* chance = requirement->find("chance");
        float odds = static_cast<float>(numberOf(chance).value_or(0.0));
        if (const auto* provider = chance == nullptr ? nullptr : chance->get<NbtCompound>(); provider != nullptr) {
            const std::string* kind = stringAt(*provider, "type");
            if (kind == nullptr || *kind != "minecraft:enchantment_level") {
                return false;
            }
            odds = levelBased(provider->find("amount"), level);
        }
        return random.nextFloat() < odds;
    }
    if (*type == "minecraft:damage_source_properties") {
        const NbtCompound* predicate = compoundAt(*requirement, "predicate");
        if (predicate == nullptr) {
            return false;
        }
        for (std::size_t at = 0; at < predicate->size(); ++at) {
            const auto* flag = predicate->values()[at].get<std::int8_t>();
            if (predicate->names()[at] != "is_direct" || flag == nullptr || (*flag != 0) != hit.direct) {
                return false;
            }
        }
        return true;
    }
    if (*type == "minecraft:entity_properties") {
        const std::string* entity = stringAt(*requirement, "entity");
        const NbtCompound* predicate = compoundAt(*requirement, "predicate");
        return entity != nullptr && *entity == "this" && predicate != nullptr && victimIs(*predicate, hit.victim);
    }
    return false;
}

void apply(const NbtCompound* effect, std::int32_t level, Side target, Side owner, std::int32_t slot,
           random::RandomSource& random, Outcome& outcome) {
    const std::string* type = effect == nullptr ? nullptr : stringAt(*effect, "type");
    if (type == nullptr) {
        return;
    }
    if (*type == "minecraft:all_of") {
        const NbtTag* found = effect->find("effects");
        const auto* effects = found == nullptr ? nullptr : found->get<NbtList>();
        for (std::size_t at = 0; effects != nullptr && at < effects->size(); ++at) {
            apply((*effects)[at].get<NbtCompound>(), level, target, owner, slot, random, outcome);
        }
    } else if (*type == "minecraft:ignite") {
        outcome.ignites.push_back(Ignite{
            .target = target,
            .ticks = static_cast<std::int32_t>(std::floor(levelBased(effect->find("duration"), level) * 20.0F))});
    } else if (*type == "minecraft:damage_entity") {
        const std::string* damageType = stringAt(*effect, "damage_type");
        if (damageType == nullptr) {
            return;
        }
        if (const auto id = Identifier::parse(*damageType); id.has_value()) {
            const float min = levelBased(effect->find("min_damage"), level);
            const float max = levelBased(effect->find("max_damage"), level);
            outcome.damages.push_back(Damage{.target = target, .type = *id, .amount = randomBetween(random, min, max)});
        }
    } else if (*type == "minecraft:change_item_damage") {
        outcome.wear.push_back(
            ItemWear{.owner = owner,
                     .slot = slot,
                     .amount = static_cast<std::int32_t>(levelBased(effect->find("amount"), level))});
    } else if (*type == "minecraft:apply_mob_effect") {
        // parity: `HolderSet.getRandomElement`, one of the listed effects.
        const NbtTag* found = effect->find("to_apply");
        std::optional<std::string> chosen;
        if (const auto* single = found == nullptr ? nullptr : found->get<std::string>(); single != nullptr) {
            chosen = *single;
        } else if (const auto* list = found == nullptr ? nullptr : found->get<NbtList>();
                   list != nullptr && list->size() > 0) {
            const auto* name =
                (*list)[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(list->size())))]
                    .get<std::string>();
            chosen = name == nullptr ? std::nullopt : std::optional{*name};
        }
        if (!chosen.has_value()) {
            return;
        }
        const auto id = Identifier::parse(*chosen);
        if (!id.has_value()) {
            return;
        }
        const auto ticks =
            static_cast<std::int32_t>(std::lround(randomBetween(random, levelBased(effect->find("min_duration"), level),
                                                                levelBased(effect->find("max_duration"), level)) *
                                                  20.0F));
        const auto amplifier = std::max(0, static_cast<std::int32_t>(std::lround(
                                               randomBetween(random, levelBased(effect->find("min_amplifier"), level),
                                                             levelBased(effect->find("max_amplifier"), level)))));
        outcome.effects.push_back(MobEffect{.target = target, .effect = *id, .ticks = ticks, .amplifier = amplifier});
    }
}

/// parity: `Enchantment.doPostAttack` for one stack and the side it belongs to.
void runOn(const Equipped& equipped, Side owner, const Hit& hit, const weaponDamage::DefinitionOf& definitionOf,
           random::RandomSource& random, Outcome& outcome) {
    const std::string_view enchanted = owner == Side::kVictim ? "victim" : "attacker";
    for (const auto& [id, level] : player::enchantmentLevels(equipped.stack)) {
        const NbtCompound* definition = definitionOf(id);
        if (definition == nullptr || !matchingSlot(*definition, equipped.equipmentSlot)) {
            continue;
        }
        const NbtCompound* effects = compoundAt(*definition, "effects");
        const NbtTag* found = effects == nullptr ? nullptr : effects->find("minecraft:post_attack");
        const auto* entries = found == nullptr ? nullptr : found->get<NbtList>();
        for (std::size_t at = 0; entries != nullptr && at < entries->size(); ++at) {
            const auto* entry = (*entries)[at].get<NbtCompound>();
            const std::string* side = entry == nullptr ? nullptr : stringAt(*entry, "enchanted");
            const std::string* affected = entry == nullptr ? nullptr : stringAt(*entry, "affected");
            if (side == nullptr || affected == nullptr || *side != enchanted ||
                !meets(compoundAt(*entry, "requirements"), level, hit, random)) {
                continue;
            }
            const Side target = *affected == "victim" ? Side::kVictim : Side::kAttacker;
            apply(compoundAt(*entry, "effect"), level, target, owner, equipped.slot, random, outcome);
        }
    }
}

} // namespace

Outcome of(const Hit& hit, const weaponDamage::DefinitionOf& definitionOf, random::RandomSource& random) {
    Outcome outcome;
    if (!definitionOf) {
        return outcome;
    }
    for (const Equipped& equipped : hit.victimEquipment) {
        runOn(equipped, Side::kVictim, hit, definitionOf, random, outcome);
    }
    if (hit.weapon.has_value()) {
        runOn(*hit.weapon, Side::kAttacker, hit, definitionOf, random, outcome);
    }
    return outcome;
}

} // namespace basalt::server::runtime::world::postAttack
