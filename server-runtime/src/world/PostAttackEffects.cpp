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

/// parity: `LevelBasedValue`: a constant, `linear`, or `lookup` with its fallback.
[[nodiscard]] float levelBased(const NbtTag* tag, std::int32_t level) {
    if (const std::optional<double> constant = numberOf(tag); constant.has_value()) {
        return static_cast<float>(*constant);
    }
    const auto* value = tag == nullptr ? nullptr : tag->get<NbtCompound>();
    if (value == nullptr) {
        return 0.0F;
    }
    if (const std::string* type = stringAt(*value, "type"); type != nullptr && *type == "minecraft:lookup") {
        const NbtTag* found = value->find("values");
        const auto* values = found == nullptr ? nullptr : found->get<NbtList>();
        if (values != nullptr && level >= 1 && static_cast<std::size_t>(level) <= values->size()) {
            return static_cast<float>(numberOf(&(*values)[static_cast<std::size_t>(level - 1)]).value_or(0.0));
        }
        return levelBased(value->find("fallback"), level);
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

[[nodiscard]] bool inRange(const NbtTag* bounds, double value) {
    if (const std::optional<double> exact = numberOf(bounds); exact.has_value()) {
        return value == *exact;
    }
    const auto* range = bounds == nullptr ? nullptr : bounds->get<NbtCompound>();
    if (range == nullptr) {
        return false;
    }
    const std::optional<double> min = numberOf(range->find("min"));
    const std::optional<double> max = numberOf(range->find("max"));
    return (!min.has_value() || value >= *min) && (!max.has_value() || value <= *max);
}

/// The attacker's flags and movement, which is all wind burst asks of them; anything else is unmet.
[[nodiscard]] bool attackerIs(const NbtCompound& predicate, const Hit& hit) {
    for (std::size_t at = 0; at < predicate.size(); ++at) {
        const std::string& key = predicate.names()[at];
        const auto* inner = predicate.values()[at].get<NbtCompound>();
        if (inner == nullptr) {
            return false;
        }
        for (std::size_t field = 0; field < inner->size(); ++field) {
            const std::string& name = inner->names()[field];
            const NbtTag& value = inner->values()[field];
            if (key == "minecraft:flags" && name == "is_flying") {
                const auto* flag = value.get<std::int8_t>();
                if (flag == nullptr || (*flag != 0) != hit.attackerFlying) {
                    return false;
                }
            } else if (key == "minecraft:movement" && name == "fall_distance") {
                if (!inRange(&value, hit.attackerFallDistance)) {
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    return true;
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
        if (entity == nullptr || predicate == nullptr) {
            return false;
        }
        if (*entity == "this") {
            return victimIs(*predicate, hit.victim);
        }
        return (*entity == "attacker" || *entity == "direct_attacker") && attackerIs(*predicate, hit);
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
    } else if (*type == "minecraft:explode") {
        const auto particle = [effect](std::string_view key) {
            const NbtCompound* options = compoundAt(*effect, key);
            const std::string* name = options == nullptr ? nullptr : stringAt(*options, "type");
            return name == nullptr ? std::string{} : *name;
        };
        const std::string* interaction = stringAt(*effect, "block_interaction");
        const std::string* immune = stringAt(*effect, "immune_blocks");
        const std::string* sound = stringAt(*effect, "sound");
        const NbtTag* multiplier = effect->find("knockback_multiplier");
        outcome.explosions.push_back(Explosion{
            .target = target,
            .radius = std::max(levelBased(effect->find("radius"), level), 0.0F),
            .knockbackMultiplier = multiplier == nullptr ? std::nullopt : std::optional{levelBased(multiplier, level)},
            .triggersBlocks = interaction != nullptr && *interaction == "trigger",
            .immuneBlocks = immune == nullptr ? std::string{} : *immune,
            .smallParticle = particle("small_particle"),
            .largeParticle = particle("large_particle"),
            .sound = sound == nullptr ? std::string{} : *sound});
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
