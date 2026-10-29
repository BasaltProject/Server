#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/wire/nbt/NbtType.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>

namespace basalt::server::runtime::entity::mobStatusEffects {
namespace {

using entity::AttributeModifier;
using entity::AttributeOperation;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
namespace effects = world::mobEffects;

// parity: each effect's own `shouldApplyEffectTickThisTick`, halved per level.
constexpr std::int32_t kRegenerationInterval = 50;
constexpr std::int32_t kPoisonInterval = 25;
constexpr std::int32_t kWitherInterval = 40;
/// parity: `MobEffect.AMBIENT_ALPHA`, `Mth.floor(38.25F)`.
constexpr std::int32_t kAmbientAlpha = 38;

[[nodiscard]] bool onInterval(std::int32_t ticks, std::int32_t base, std::int32_t amplifier) {
    const std::int32_t interval = amplifier >= 31 ? 0 : base >> amplifier;
    return interval <= 0 || ticks % interval == 0;
}

struct Template {
    std::string_view effect;
    std::string_view attribute;
    std::string_view id;
    double amount;
    AttributeOperation operation;
};

/// parity: the `addAttributeModifier` calls in `MobEffects`, each scaled by the level.
constexpr std::array kTemplates{
    Template{"speed", "minecraft:movement_speed", "minecraft:effect.speed", static_cast<double>(0.2F),
             AttributeOperation::AddMultipliedTotal},
    Template{"slowness", "minecraft:movement_speed", "minecraft:effect.slowness", static_cast<double>(-0.15F),
             AttributeOperation::AddMultipliedTotal},
    Template{"haste", "minecraft:attack_speed", "minecraft:effect.haste", static_cast<double>(0.1F),
             AttributeOperation::AddMultipliedTotal},
    Template{"mining_fatigue", "minecraft:attack_speed", "minecraft:effect.mining_fatigue", static_cast<double>(-0.1F),
             AttributeOperation::AddMultipliedTotal},
    Template{"strength", "minecraft:attack_damage", "minecraft:effect.strength", 3.0, AttributeOperation::AddValue},
    Template{"jump_boost", "minecraft:safe_fall_distance", "minecraft:effect.jump_boost", 1.0,
             AttributeOperation::AddValue},
    Template{"weakness", "minecraft:attack_damage", "minecraft:effect.weakness", -4.0, AttributeOperation::AddValue},
    Template{"health_boost", "minecraft:max_health", "minecraft:effect.health_boost", 4.0,
             AttributeOperation::AddValue},
    Template{"absorption", "minecraft:max_absorption", "minecraft:effect.absorption", 4.0,
             AttributeOperation::AddValue},
    Template{"luck", "minecraft:luck", "minecraft:effect.luck", 1.0, AttributeOperation::AddValue},
    Template{"unluck", "minecraft:luck", "minecraft:effect.unluck", -1.0, AttributeOperation::AddValue}};

/// parity: `MobEffect.addAttributeModifiers`.
void applyModifiers(ServerEntity& mob, const Identifier& effect, std::int32_t amplifier) {
    for (const Template& modifier : kTemplates) {
        if (effect.path() != modifier.effect) {
            continue;
        }
        mob.attributeModifiers[std::string{modifier.attribute}].insert_or_assign(
            std::string{modifier.id}, AttributeModifier{.id = std::string{modifier.id},
                                                        .amount = modifier.amount * static_cast<double>(amplifier + 1),
                                                        .operation = modifier.operation});
    }
}

/// parity: `MobEffect.removeAttributeModifiers`.
void removeModifiers(ServerEntity& mob, const Identifier& effect) {
    for (const Template& modifier : kTemplates) {
        if (effect.path() != modifier.effect) {
            continue;
        }
        if (const auto found = mob.attributeModifiers.find(modifier.attribute); found != mob.attributeModifiers.end()) {
            found->second.erase(std::string{modifier.id});
        }
    }
}

[[nodiscard]] bool shorterThan(const world::ActiveEffect& one, const world::ActiveEffect& other) {
    // parity: `isShorterDurationThan`, where an infinite effect outlasts everything.
    return !one.infinite() && (one.remainingTicks < other.remainingTicks || other.infinite());
}

/// parity: `MobEffectInstance.update` on the hidden dose at `index` of the chain.
void updateHidden(std::vector<world::ActiveEffect>& chain, std::size_t index, const world::ActiveEffect& takeOver) {
    if (index >= chain.size()) {
        chain.push_back(takeOver);
        return;
    }
    const world::ActiveEffect before = chain[index];
    if (takeOver.amplifier > before.amplifier) {
        if (shorterThan(takeOver, before)) {
            chain.insert(chain.begin() + static_cast<std::ptrdiff_t>(index) + 1, before);
        }
        chain[index].amplifier = takeOver.amplifier;
        chain[index].remainingTicks = takeOver.remainingTicks;
        chain[index].ambient = takeOver.ambient;
    } else if (shorterThan(before, takeOver)) {
        if (takeOver.amplifier != before.amplifier) {
            updateHidden(chain, index + 1, takeOver);
            return;
        }
        chain[index].remainingTicks = takeOver.remainingTicks;
        chain[index].ambient = takeOver.ambient;
    } else if (!takeOver.ambient && before.ambient) {
        chain[index].ambient = false;
    }
    chain[index].visible = takeOver.visible;
    chain[index].showIcon = takeOver.showIcon;
}

void encodeDetails(NbtCompound& entry, const world::ActiveEffect& active, std::span<const world::ActiveEffect> hidden) {
    entry.putByte("amplifier", static_cast<std::int8_t>(active.amplifier));
    entry.putInt("duration", active.remainingTicks);
    entry.putBoolean("ambient", active.ambient);
    entry.putBoolean("show_particles", active.visible);
    entry.putBoolean("show_icon", active.showIcon);
    if (!hidden.empty()) {
        NbtCompound next;
        encodeDetails(next, hidden.front(), hidden.subspan(1));
        entry.set("hidden_effect", NbtTag::ofCompound(std::move(next)));
    }
}

[[nodiscard]] world::ActiveEffect decodeDetails(const Identifier& id, const NbtCompound& entry) {
    return world::ActiveEffect{.effect = id,
                               .amplifier = entry.getByte("amplifier"),
                               .remainingTicks = entry.getInt("duration", 1),
                               .ambient = entry.getBoolean("ambient"),
                               .visible = entry.getBoolean("show_particles", true),
                               .showIcon = entry.getBoolean("show_icon", true)};
}

} // namespace

bool canBeAffected(const ServerEntity& mob, const Identifier& effect) {
    const std::string_view path = mob.type().path();
    // parity: `EnderDragon.addEffect` and `WitherBoss.addEffect` refuse everything.
    if (path == "ender_dragon" || path == "wither") {
        return false;
    }
    if (path == "silverfish") {
        return effect.path() != "infested";
    }
    if (path == "slime") {
        return effect.path() != "oozing";
    }
    // parity: `#minecraft:ignores_poison_and_regen`, which is `#minecraft:undead`.
    if (witherBoss::isFriend(path) && (effect == effects::regeneration() || effect == effects::poison())) {
        return false;
    }
    if ((path == "spider" || path == "cave_spider" || path == "nautilus" || path == "zombie_nautilus") &&
        effect == effects::poison()) {
        return false;
    }
    if (path == "wither_skeleton" && effect == effects::wither()) {
        return false;
    }
    return !(path == "parched" && effect.path() == "weakness");
}

bool invertsHealAndHarm(const ServerEntity& mob) { return witherBoss::isFriend(mob.type().path()); }

Instant instantOf(const ServerEntity& mob, const Identifier& effect, std::int32_t amplifier) {
    const bool healing = effect == effects::instantHealth();
    if (!healing && effect != effects::instantDamage()) {
        return {};
    }
    // parity: `HealOrHarmMobEffect`.
    const std::int32_t shift = amplifier & 31;
    const auto heal = static_cast<float>(std::max(4 << shift, 0));
    const auto harm = static_cast<float>(6 << shift);
    return healing != invertsHealAndHarm(mob) ? Instant{.heal = heal} : Instant{.damage = harm};
}

bool add(ServerEntity& mob, const world::ActiveEffect& effect) {
    if (!canBeAffected(mob, effect.effect)) {
        return false;
    }
    const auto found = mob.effects.find(effect.effect);
    if (found == mob.effects.end()) {
        mob.effects.emplace(effect.effect, effect);
        applyModifiers(mob, effect.effect, effect.amplifier);
        return true;
    }
    world::ActiveEffect& current = found->second;
    std::vector<world::ActiveEffect>& hidden = mob.hiddenEffects[effect.effect];
    bool changed = false;
    if (effect.amplifier > current.amplifier) {
        // parity: the weaker dose outlasting the stronger one is kept aside, to come back later.
        if (shorterThan(effect, current)) {
            hidden.insert(hidden.begin(), current);
        }
        current.amplifier = effect.amplifier;
        current.remainingTicks = effect.remainingTicks;
        changed = true;
    } else if (shorterThan(current, effect)) {
        if (effect.amplifier == current.amplifier) {
            current.remainingTicks = effect.remainingTicks;
            changed = true;
        } else {
            updateHidden(hidden, 0, effect);
        }
    }
    if (hidden.empty()) {
        mob.hiddenEffects.erase(effect.effect);
    }
    if (changed) {
        applyModifiers(mob, current.effect, current.amplifier);
    }
    if ((!effect.ambient && current.ambient) || changed) {
        current.ambient = effect.ambient;
        changed = true;
    }
    if (effect.visible != current.visible || effect.showIcon != current.showIcon) {
        current.visible = effect.visible;
        current.showIcon = effect.showIcon;
        changed = true;
    }
    return changed;
}

bool has(const ServerEntity& mob, const Identifier& effect) { return mob.effects.contains(effect); }

bool remove(ServerEntity& mob, const Identifier& effect) {
    removeModifiers(mob, effect);
    mob.hiddenEffects.erase(effect);
    return mob.effects.erase(effect) > 0;
}

void clear(ServerEntity& mob) {
    for (const auto& [id, active] : mob.effects) {
        removeModifiers(mob, id);
    }
    mob.effects.clear();
    mob.hiddenEffects.clear();
}

std::vector<TickEffect> tick(ServerEntity& mob, std::vector<Identifier>& expired) {
    std::vector<TickEffect> owed;
    for (auto it = mob.effects.begin(); it != mob.effects.end();) {
        world::ActiveEffect& active = it->second;
        const std::int32_t ticks = active.infinite() ? mob.tickCount : active.remainingTicks;
        if (active.effect == effects::regeneration()) {
            if (onInterval(ticks, kRegenerationInterval, active.amplifier)) {
                owed.push_back({.heal = 1.0F});
            }
        } else if (active.effect == effects::poison()) {
            // parity: poison never kills.
            if (onInterval(ticks, kPoisonInterval, active.amplifier) && mob.health > 1.0) {
                owed.push_back({.damage = 1.0F, .cause = "magic"});
            }
        } else if (active.effect == effects::wither()) {
            if (onInterval(ticks, kWitherInterval, active.amplifier)) {
                owed.push_back({.damage = 1.0F, .cause = "wither"});
            }
        }
        if (!active.infinite()) {
            active.remainingTicks--;
        }
        // parity: `tickDownDuration` counts hidden doses down too.
        if (const auto chain = mob.hiddenEffects.find(active.effect); chain != mob.hiddenEffects.end()) {
            for (world::ActiveEffect& hidden : chain->second) {
                if (!hidden.infinite() && hidden.remainingTicks > 0) {
                    hidden.remainingTicks--;
                }
            }
            if (active.remainingTicks == 0 && !chain->second.empty()) {
                active = chain->second.front();
                chain->second.erase(chain->second.begin());
                if (chain->second.empty()) {
                    mob.hiddenEffects.erase(chain);
                }
                applyModifiers(mob, active.effect, active.amplifier);
            }
        }
        if (!active.infinite() && active.remainingTicks <= 0) {
            expired.push_back(active.effect);
            removeModifiers(mob, active.effect);
            it = mob.effects.erase(it);
        } else {
            ++it;
        }
    }
    return owed;
}

Swirl swirlOf(const std::map<Identifier, world::ActiveEffect>& carried) {
    Swirl swirl;
    swirl.allAmbient = true;
    for (const auto& [id, active] : carried) {
        swirl.allAmbient = swirl.allAmbient && active.ambient;
        if (!active.visible) {
            continue;
        }
        if (const std::optional<std::string_view> own = effects::ownParticleOf(id); own.has_value()) {
            swirl.particles.push_back({.particle = Identifier::vanilla(std::string{*own})});
            continue;
        }
        // parity: `MobEffect`'s default particle, the colour at full or at the ambient alpha.
        const std::uint32_t alpha = active.ambient ? static_cast<std::uint32_t>(kAmbientAlpha) : 255U;
        const auto rgb = static_cast<std::uint32_t>(effects::colorOf(id).value_or(0)) & 0xFFFFFFU;
        swirl.particles.push_back({.particle = Identifier::vanilla("entity_effect"),
                                   .argb = static_cast<std::int32_t>((alpha << 24U) | rgb)});
    }
    return swirl;
}

NbtTag encode(const ServerEntity& mob) {
    wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
    for (const auto& [id, active] : mob.effects) {
        NbtCompound entry;
        entry.putString("id", id.toString());
        const auto chain = mob.hiddenEffects.find(id);
        encodeDetails(entry, active,
                      chain == mob.hiddenEffects.end() ? std::span<const world::ActiveEffect>{}
                                                       : std::span<const world::ActiveEffect>{chain->second});
        static_cast<void>(list.push(NbtTag::ofCompound(std::move(entry))));
    }
    return NbtTag::ofList(std::move(list));
}

void decode(ServerEntity& mob, const NbtCompound& tag) {
    mob.effects.clear();
    const wire::nbt::NbtList* list = tag.getList("active_effects");
    if (list == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < list->size(); ++index) {
        const NbtCompound* entry = (*list)[index].get<NbtCompound>();
        if (entry == nullptr) {
            continue;
        }
        const auto id = Identifier::parse(entry->getString("id"));
        if (!id.has_value()) {
            continue;
        }
        applyModifiers(mob, *id, entry->getByte("amplifier"));
        mob.effects.insert_or_assign(*id, decodeDetails(*id, *entry));
        std::vector<world::ActiveEffect> chain;
        for (const NbtCompound* hidden = entry->getCompound("hidden_effect"); hidden != nullptr;
             hidden = hidden->getCompound("hidden_effect")) {
            chain.push_back(decodeDetails(*id, *hidden));
        }
        if (!chain.empty()) {
            mob.hiddenEffects.insert_or_assign(*id, std::move(chain));
        }
    }
}

} // namespace basalt::server::runtime::entity::mobStatusEffects
