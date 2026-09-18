#pragma once

#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// Status effects on a mob: who takes which, how a second dose merges with the first, and what a
/// tick of each one does.
///
/// parity: `LivingEntity.addEffect`, `canBeAffected`, `tickEffects` and `MobEffectInstance.update`.
namespace mobStatusEffects {

/// parity: `LivingEntity.canBeAffected` and the overrides that refuse one effect or all of them.
[[nodiscard]] bool canBeAffected(const ServerEntity& mob, const wire::common::Identifier& effect);

/// Whether a heal hurts it and a harm heals it. parity: `isInvertedHealAndHarm`, the undead.
[[nodiscard]] bool invertsHealAndHarm(const ServerEntity& mob);

/// What an instant effect does on the spot: health back, or a hit of `damage`.
struct Instant {
    float heal = 0.0F;
    float damage = 0.0F;
};

/// parity: `InstantenousMobEffect.applyInstantenousEffect` at full strength.
[[nodiscard]] Instant instantOf(const ServerEntity& mob, const wire::common::Identifier& effect,
                                std::int32_t amplifier);

/// Adds or merges an effect. Answers whether anything changed. Instant effects are the caller's.
///
/// parity: `addEffect` over `MobEffectInstance.update`.
bool add(ServerEntity& mob, const world::ActiveEffect& effect);

[[nodiscard]] bool has(const ServerEntity& mob, const wire::common::Identifier& effect);

bool remove(ServerEntity& mob, const wire::common::Identifier& effect);

/// One point of health or one hit a tick owes the mob.
struct TickEffect {
    float heal = 0.0F;
    float damage = 0.0F;
    /// The damage cause: `magic` or `wither`.
    std::string_view cause;
};

/// Counts every effect down and answers what their ticks do, in the order the effects run.
/// `expired` gets the effects that ran out.
[[nodiscard]] std::vector<TickEffect> tick(ServerEntity& mob, std::vector<wire::common::Identifier>& expired);

/// parity: `active_effects`, the saved form.
[[nodiscard]] wire::nbt::NbtTag encode(const ServerEntity& mob);

void decode(ServerEntity& mob, const wire::nbt::NbtCompound& tag);

} // namespace mobStatusEffects

} // namespace basalt::server::runtime::entity
