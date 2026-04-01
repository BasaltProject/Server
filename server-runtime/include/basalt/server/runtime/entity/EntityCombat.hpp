#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
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

/// A clock that ticks once per hit on a raider and once per raid bar refresh, so the raid can tell
/// which came last.
[[nodiscard]] std::int64_t nextRaidBarStamp();

/// Hurts `entity` for `amount`, pushed away from the source, answering what happened.
///
/// parity: `LivingEntity.hurtServer`. The cooldown is the part worth naming: inside it only a bigger
/// hit lands, and only by the difference, which is what stops a fast weapon from being strictly
/// better than a strong one.
///
/// `byPlayer` is whether a player is behind the hit, which is all an ender dragon listens to
/// besides explosions.
Result hurt(ServerEntity& entity, float amount, std::optional<double> sourceX = std::nullopt,
            std::optional<double> sourceZ = std::nullopt, const Hooks& hooks = {}, std::string_view cause = "attack",
            const std::optional<std::string>& attackerName = std::nullopt, bool byPlayer = false);

/// Names who is behind the next `hurt` of `target` and what lands it, for its death roll.
void blame(ServerEntity& target, std::optional<std::int32_t> causingId, std::int32_t directId,
           const wire::common::Identifier& directType);

/// parity: `LivingEntity.knockback`. The coordinates point at whoever swung, and the push is taken
/// off the speed, so the entity ends up going the other way.
void knockback(ServerEntity& entity, double power, double towardsX, double towardsZ);

/// What this kind of mob hits for. parity: `Attributes.ATTACK_DAMAGE`, zero for anything harmless.
[[nodiscard]] float attackDamageOf(const ServerEntity& entity);

/// The sound a mob makes when hit, and when it dies.
///
/// parity: `getHurtSound` and `getDeathSound`, which nearly every mob answers with its own name.
/// The same for this mob, which a copper golem answers by how weathered it is.
[[nodiscard]] std::string hurtSound(const ServerEntity& entity);
[[nodiscard]] std::string deathSound(const ServerEntity& entity);

[[nodiscard]] std::string hurtSound(const wire::common::Identifier& type);

[[nodiscard]] std::string deathSound(const wire::common::Identifier& type);

/// parity: `LivingEntity.resolvePlayerResponsibleForDamage`, which keeps a player's hit for five
/// seconds.
inline constexpr std::int32_t kPlayerHitMemoryTicks = 100;

/// Remembers that a player hurt it, which is what a kill needs to drop experience.
void rememberPlayer(ServerEntity& entity, const wire::common::Uuid& player);

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
    /// Who shot it, when the hit is a projectile's. parity: `DamageSource.getEntity`, which is the
    /// owner rather than the arrow.
    std::optional<std::int32_t> ownerId;
    /// An arrow with a pierce level, which no shield stops. parity: `LivingEntity.applyItemBlocking`.
    bool piercing = false;
};

} // namespace basalt::server::runtime::entity
