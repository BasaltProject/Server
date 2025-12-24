#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// What a hit does to something that is not a player.
///
/// parity: `LivingEntity.hurtServer` for the half that a mob has: the half second of invulnerability
/// after a hit, the bigger-hit-only rule inside it, armour off the mob's own attribute, and the push
/// the hit gives.
namespace entityCombat {

/// parity: `LivingEntity.invulnerableDuration`, ten ticks.
inline constexpr std::int32_t kHurtCooldownTicks = 20;

/// parity: `LivingEntity.dealDefaultKnockback`, which every hit that lands is worth.
inline constexpr double kDefaultKnockback = 0.4;

/// What a hit left behind: whether it landed, and whether it killed.
struct Result {
    bool landed = false;
    bool died = false;
    float damage = 0.0F;
};

/// What a plugin gets a say in when something is hurt. Callbacks rather than the plugin bus itself:
/// this has no business knowing what a plugin is, and unset hooks mean nobody objects.
struct Hooks {
    /// Answers the damage to deal, or nothing to stop the hit entirely.
    std::function<std::optional<float>(const ServerEntity& entity, float amount, std::string_view cause,
                                       const std::optional<std::string>& attackerName)>
        damaged;
    std::function<void(const ServerEntity& entity, const std::optional<std::string>& attackerName)> died;
};

/// Hurts `entity` for `amount`, pushed away from the source, answering what happened.
///
/// parity: `LivingEntity.hurtServer`. The cooldown is the part worth naming: inside it only a bigger
/// hit lands, and only by the difference, which is what stops a fast weapon from being strictly
/// better than a strong one.
Result hurt(ServerEntity& entity, float amount, std::optional<double> sourceX = std::nullopt,
            std::optional<double> sourceZ = std::nullopt, const Hooks& hooks = {}, std::string_view cause = "attack",
            const std::optional<std::string>& attackerName = std::nullopt);

/// parity: `LivingEntity.knockback`. The coordinates point at whoever swung, and the push is taken
/// off the speed, so the entity ends up going the other way.
void knockback(ServerEntity& entity, double power, double towardsX, double towardsZ);

/// What this kind of mob hits for. parity: `Attributes.ATTACK_DAMAGE`, zero for anything harmless.
[[nodiscard]] float attackDamageOf(const ServerEntity& entity);

/// The sound a mob makes when hit, and when it dies.
///
/// parity: `getHurtSound` and `getDeathSound`, which nearly every mob answers with its own name.
[[nodiscard]] std::string hurtSound(const wire::common::Identifier& type);

[[nodiscard]] std::string deathSound(const wire::common::Identifier& type);

/// Counts the invulnerability window down, once per tick.
void tick(ServerEntity& entity);

/// parity: `Entity.getDisplayName`, which is the name somebody gave it or the name of its kind.
[[nodiscard]] wire::text::Component displayName(const ServerEntity& entity);

} // namespace entityCombat

/// Who dealt a hit and where they stood, which is what decides the way it pushes.
struct DamageSource {
    std::int32_t entityId = 0;
    double x = 0.0;
    double z = 0.0;
    /// Travels with the hit because only where it comes from knows it: below there is just an id.
    std::optional<wire::text::Component> name;
};

} // namespace basalt::server::runtime::entity
