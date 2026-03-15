#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/player/Combat.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::entity::entityCombat {
namespace {

namespace reg = server::registry;

/// parity: the half of the invulnerability window where only a bigger hit gets through.
constexpr std::int32_t kCooldownHalfTicks = kHurtCooldownTicks / 2;

[[nodiscard]] float attributeOf(const wire::common::Identifier& type, std::string_view name) {
    const std::optional<double> base = reg::entityAttributes::baseValue(type, name);
    return base.has_value() ? static_cast<float>(*base) : 0.0F;
}

/// parity: a mob's own `armor` and `armor_toughness` attributes, which most leave at zero.
[[nodiscard]] float afterArmour(const ServerEntity& entity, float amount) {
    const float armour = attributeOf(entity.type(), reg::entityAttributes::kArmor);
    if (armour <= 0.0F) {
        return amount;
    }
    return player::combat::afterArmour(amount, armour,
                                       attributeOf(entity.type(), reg::entityAttributes::kArmorToughness));
}

} // namespace

void knockback(ServerEntity& entity, double power, double towardsX, double towardsZ) {
    const std::array<double, 3> moved = player::combat::knockback(
        power, towardsX, towardsZ, entity.velocityX, entity.velocityY, entity.velocityZ, entity.onGround,
        static_cast<double>(attributeOf(entity.type(), reg::entityAttributes::kKnockbackResistance)));
    entity.velocityX = moved[0];
    entity.velocityY = moved[1];
    entity.velocityZ = moved[2];
}

Result hurt(ServerEntity& entity, float amount, std::optional<double> sourceX, std::optional<double> sourceZ,
            const Hooks& hooks, std::string_view cause, const std::optional<std::string>& attackerName,
            const bool byPlayer) {
    if (entity.removed || entity.health <= 0.0) {
        return {};
    }
    // parity: `EnderDragon.hurtServer` is a hit on the body, and only a player or an explosion
    // (`always_hurts_ender_dragons`) takes health off. Anything else still counts as having hit.
    if (entity.dragon != nullptr) {
        const enderDragon::Part part = entity.dragon->hitPart.value_or(enderDragon::Part::Body);
        entity.dragon->hitPart.reset();
        const std::optional<float> onBody =
            enderDragon::damageFor(*entity.dragon, part, amount, cause == "projectile" || cause == "wind_charge");
        if (!onBody.has_value()) {
            return {};
        }
        if (!byPlayer && cause != "explosion") {
            return {.landed = true};
        }
        amount = *onBody;
    }
    // parity: `Warden.isInvulnerableTo`, untouchable while it is rising or burrowing.
    if (wardenBrain::isDiggingOrEmerging(entity) && cause != "out_of_world" && cause != "generic_kill") {
        return {};
    }
    if (entity.wither != nullptr) {
        // A mob's melee names its type; a projectile or blast owned by one sets the flag instead.
        const std::string_view attacker = attackerName.has_value() ? std::string_view{*attackerName} : "";
        const bool friendly = std::exchange(entity.wither->nextHitFromFriend, false) ||
                              (cause == "mob_attack" && attacker.starts_with("minecraft:") &&
                               witherBoss::isFriend(attacker.substr(attacker.find(':') + 1)));
        if (witherBoss::refuses(entity, *entity.wither,
                                {.fromWitherFriend = friendly,
                                 .arrowOrWindCharge = cause == "projectile" || cause == "wind_charge",
                                 .bypassesInvulnerability = cause == "out_of_world" || cause == "generic_kill",
                                 .witherImmuneType = cause == "drown"})) {
            return {};
        }
    }
    // Every hit a mob takes comes through here, so this is the one place that has to ask. A plugin
    // may stop it or change how much it is worth; what it cannot do is see a hit that was never
    // going to land, which is why this is below the checks above.
    if (hooks.damaged) {
        const std::optional<float> allowed = hooks.damaged(entity, amount, cause, attackerName);
        if (!allowed.has_value() || *allowed <= 0.0F) {
            return {};
        }
        amount = *allowed;
    }
    float incoming = amount;
    const bool cooling = entity.hurtCooldown > kCooldownHalfTicks;
    if (cooling) {
        if (amount <= entity.lastHurt) {
            return {};
        }
        incoming = amount - entity.lastHurt;
        entity.lastHurt = amount;
    } else {
        entity.lastHurt = amount;
        entity.hurtCooldown = kHurtCooldownTicks;
    }
    const float reduced = afterArmour(entity, incoming);
    if (reduced <= 0.0F) {
        return {};
    }
    const double before = entity.health;
    entity.lastDamageCause = std::string{cause};
    // parity: `CopperGolem.actuallyHurt`, which drops whatever pose it was working in.
    if (entity.copperGolem != nullptr) {
        entity.copperGolem->pose = copperGolems::Pose::Idle;
    }
    entity.health -= static_cast<double>(reduced);
    bool died = entity.health <= 0.0;
    if (died) {
        entity.health = 0.0;
    }
    if (entity.dragon != nullptr) {
        // parity: `EnderDragon.handleKillingBlow`: a flying dragon is left on one health to die.
        if (died && enderDragon::refusesDeath(*entity.dragon)) {
            entity.health = 1.0;
            died = false;
        }
        enderDragon::afterHurt(*entity.dragon, static_cast<float>(before), static_cast<float>(entity.health));
    }
    // parity: the 0.4 every hit is worth, taken before the attacker's own extra knockback.
    // parity: `EnderDragon.knockback`, which a sitting dragon ignores.
    const bool planted = entity.dragon != nullptr && enderDragon::isSitting(entity.dragon->phase);
    if (!cooling && !planted) {
        knockback(entity, kDefaultKnockback, sourceX.value_or(entity.x) - entity.x,
                  sourceZ.value_or(entity.z) - entity.z);
    }
    if (died && hooks.died) {
        hooks.died(entity, attackerName);
    }
    return {.landed = true, .died = died, .damage = reduced};
}

float attackDamageOf(const ServerEntity& entity) {
    return static_cast<float>(entity.attributeWithModifiers(attribute::kAttackDamage));
}

std::string hurtSound(const wire::common::Identifier& type) { return "entity." + type.path() + ".hurt"; }

std::string deathSound(const wire::common::Identifier& type) { return "entity." + type.path() + ".death"; }

void rememberPlayer(ServerEntity& entity, const wire::common::Uuid& player) {
    entity.lastHurtByPlayer = player;
    entity.lastHurtByPlayerTime = kPlayerHitMemoryTicks;
}

void tick(ServerEntity& entity) {
    if (entity.hurtCooldown > 0) {
        entity.hurtCooldown--;
    }
    if (entity.lastHurtByPlayerTime > 0) {
        entity.lastHurtByPlayerTime--;
    } else {
        entity.lastHurtByPlayer.reset();
    }
    if (entity.attackCooldown > 0) {
        entity.attackCooldown--;
    }
}

wire::text::Component displayName(const ServerEntity& entity) {
    if (entity.customName.has_value()) {
        return wire::text::Component::text(*entity.customName);
    }
    return wire::text::Component::translatable("entity.minecraft." + entity.type().path());
}

} // namespace basalt::server::runtime::entity::entityCombat
