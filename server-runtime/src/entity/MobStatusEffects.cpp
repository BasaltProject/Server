#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/wire/nbt/NbtType.hpp>

#include <algorithm>

namespace basalt::server::runtime::entity::mobStatusEffects {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
namespace effects = world::mobEffects;

// parity: each effect's own `shouldApplyEffectTickThisTick`, halved per level.
constexpr std::int32_t kRegenerationInterval = 50;
constexpr std::int32_t kPoisonInterval = 25;
constexpr std::int32_t kWitherInterval = 40;

[[nodiscard]] bool onInterval(std::int32_t ticks, std::int32_t base, std::int32_t amplifier) {
    const std::int32_t interval = amplifier >= 31 ? 0 : base >> amplifier;
    return interval <= 0 || ticks % interval == 0;
}

[[nodiscard]] bool shorterThan(const world::ActiveEffect& one, const world::ActiveEffect& other) {
    // parity: `isShorterDurationThan`, where an infinite effect outlasts everything.
    return !one.infinite() && (one.remainingTicks < other.remainingTicks || other.infinite());
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
    // parity: `HealOrHarmMobEffect`: four hearts' worth doubling per level to heal, six to harm.
    const auto heal = static_cast<float>(std::max(4 << amplifier, 0));
    const auto harm = static_cast<float>(6 << amplifier);
    return healing != invertsHealAndHarm(mob) ? Instant{.heal = heal} : Instant{.damage = harm};
}

bool add(ServerEntity& mob, const world::ActiveEffect& effect) {
    if (!canBeAffected(mob, effect.effect)) {
        return false;
    }
    const auto found = mob.effects.find(effect.effect);
    if (found == mob.effects.end()) {
        mob.effects.emplace(effect.effect, effect);
        return true;
    }
    world::ActiveEffect& current = found->second;
    bool changed = false;
    if (effect.amplifier > current.amplifier) {
        current.amplifier = effect.amplifier;
        current.remainingTicks = effect.remainingTicks;
        changed = true;
    } else if (shorterThan(current, effect) && effect.amplifier == current.amplifier) {
        current.remainingTicks = effect.remainingTicks;
        changed = true;
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

bool remove(ServerEntity& mob, const Identifier& effect) { return mob.effects.erase(effect) > 0; }

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
        if (!active.infinite() && active.remainingTicks <= 0) {
            expired.push_back(active.effect);
            it = mob.effects.erase(it);
        } else {
            ++it;
        }
    }
    return owed;
}

NbtTag encode(const ServerEntity& mob) {
    wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
    for (const auto& [id, active] : mob.effects) {
        NbtCompound entry;
        entry.putString("id", id.toString());
        entry.putByte("amplifier", static_cast<std::int8_t>(active.amplifier));
        entry.putInt("duration", active.remainingTicks);
        entry.putBoolean("ambient", active.ambient);
        entry.putBoolean("show_particles", active.visible);
        entry.putBoolean("show_icon", active.showIcon);
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
        mob.effects.insert_or_assign(*id, world::ActiveEffect{.effect = *id,
                                                              .amplifier = entry->getByte("amplifier"),
                                                              .remainingTicks = entry->getInt("duration", 1),
                                                              .ambient = entry->getBoolean("ambient"),
                                                              .visible = entry->getBoolean("show_particles", true),
                                                              .showIcon = entry->getBoolean("show_icon", true)});
    }
}

} // namespace basalt::server::runtime::entity::mobStatusEffects
