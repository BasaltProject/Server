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

/// parity: `LivingEntity.hurtServer`.
namespace entityCombat {

/// parity: `LivingEntity.invulnerableDuration`, ten ticks.
inline constexpr std::int32_t kHurtCooldownTicks = 20;

/// parity: `LivingEntity.dealDefaultKnockback`, which every hit that lands is worth.
inline constexpr double kDefaultKnockback = 0.4;

struct Result {
    bool landed = false;
    bool died = false;
    float damage = 0.0F;
};

struct Hooks {
    std::function<std::optional<float>(const ServerEntity& entity, float amount, std::string_view cause,
                                       const std::optional<std::string>& attackerName,
                                       const std::optional<std::int32_t>& attackerId)>
        damaged;
    std::function<void(const ServerEntity& entity, const std::optional<std::string>& attackerName,
                       const std::optional<std::int32_t>& killerId)>
        died;
};

[[nodiscard]] std::int64_t nextRaidBarStamp();

/// parity: `LivingEntity.hurtServer`.
Result hurt(ServerEntity& entity, float amount, std::optional<double> sourceX = std::nullopt,
            std::optional<double> sourceZ = std::nullopt, const Hooks& hooks = {}, std::string_view cause = "attack",
            const std::optional<std::string>& attackerName = std::nullopt, bool byPlayer = false,
            float armorFractionAdd = 0.0F);

void blame(ServerEntity& target, std::optional<std::int32_t> causingId, std::int32_t directId,
           const wire::common::Identifier& directType);

/// parity: `LivingEntity.knockback`.
void knockback(ServerEntity& entity, double power, double towardsX, double towardsZ);

/// parity: `Attributes.ATTACK_DAMAGE`, zero for anything harmless.
[[nodiscard]] float attackDamageOf(const ServerEntity& entity);

/// parity: `getHurtSound` and `getDeathSound`.
[[nodiscard]] std::string hurtSound(const ServerEntity& entity);
[[nodiscard]] std::string deathSound(const ServerEntity& entity);

[[nodiscard]] std::string hurtSound(const wire::common::Identifier& type);

[[nodiscard]] std::string deathSound(const wire::common::Identifier& type);

/// parity: `LivingEntity.resolvePlayerResponsibleForDamage`.
inline constexpr std::int32_t kPlayerHitMemoryTicks = 100;

void rememberPlayer(ServerEntity& entity, const wire::common::Uuid& player);

void tick(ServerEntity& entity);

/// parity: `Entity.getDisplayName`.
[[nodiscard]] wire::text::Component displayName(const ServerEntity& entity);

} // namespace entityCombat

struct DamageSource {
    std::int32_t entityId = 0;
    double x = 0.0;
    double z = 0.0;
    /// Travels with the hit because only where it comes from knows it: below there is just an id.
    std::optional<wire::text::Component> name;
    /// parity: `DamageSource.getEntity`.
    std::optional<std::int32_t> ownerId;
    /// parity: `LivingEntity.applyItemBlocking`.
    bool piercing = false;
    /// parity: breach.
    float armorFractionAdd = 0.0F;
};

} // namespace basalt::server::runtime::entity
