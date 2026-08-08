#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <string>

namespace basalt::server::runtime::entity::mobOverrides {
namespace {

void setBase(ServerEntity& entity, std::string_view id, const std::optional<double>& value) {
    if (value.has_value() && !entity.attributeValues.contains(id)) {
        entity.attributeValues.emplace(std::string{id}, *value);
    }
}

} // namespace

void apply(ServerEntity& entity, const config::MobSettings& settings) {
    const auto at = settings.overrides.find(entity.type().path());
    if (at == settings.overrides.end()) {
        return;
    }
    const config::MobOverride& tweak = *at->second;
    entity.tuning = at->second;

    const double oldMaxHealth = entity.attributeWithModifiers(attribute::kMaxHealth);
    setBase(entity, attribute::kMaxHealth, tweak.maxHealth);
    setBase(entity, attribute::kAttackDamage, tweak.attackDamage);
    setBase(entity, attribute::kMovementSpeed, tweak.movementSpeed);
    setBase(entity, "minecraft:follow_range", tweak.followRange);
    setBase(entity, attribute::kArmor, tweak.armor);
    setBase(entity, "minecraft:armor_toughness", tweak.armorToughness);
    setBase(entity, "minecraft:knockback_resistance", tweak.knockbackResistance);
    setBase(entity, "minecraft:attack_knockback", tweak.attackKnockback);
    setBase(entity, "minecraft:flying_speed", tweak.flyingSpeed);
    setBase(entity, "minecraft:jump_strength", tweak.jumpStrength);
    setBase(entity, attribute::kScale, tweak.scale);
    setBase(entity, "minecraft:safe_fall_distance", tweak.safeFallDistance);
    setBase(entity, "minecraft:fall_damage_multiplier", tweak.fallDamageMultiplier);
    setBase(entity, "minecraft:explosion_knockback_resistance", tweak.explosionKnockbackResistance);
    setBase(entity, "minecraft:oxygen_bonus", tweak.oxygenBonus);
    if (tweak.maxHealth.has_value()) {
        const double newMaxHealth = entity.attributeWithModifiers(attribute::kMaxHealth);
        entity.health = entity.health >= oldMaxHealth ? newMaxHealth : std::min(entity.health, newMaxHealth);
    }

    if (tweak.burnInDaylight.has_value() && entity.burnsInDay < 0) {
        entity.burnsInDay = *tweak.burnInDaylight ? 1 : 0;
    }
    if (tweak.noAi.has_value()) {
        entity.noAi = *tweak.noAi;
    }
    if (tweak.persistent.has_value() && *tweak.persistent) {
        entity.persistent = true;
    }
    if (tweak.canPickUpLoot.has_value()) {
        entity.canPickUpLoot = *tweak.canPickUpLoot;
    }
    if (tweak.noGravity.has_value()) {
        entity.noGravity = *tweak.noGravity;
    }
    if (tweak.collidable.has_value()) {
        entity.collidable = *tweak.collidable;
    }
    if (tweak.canSwim.has_value() && !entity.navigationFloats.has_value()) {
        entity.navigationFloats = tweak.canSwim;
    }
    if (tweak.opensDoors.has_value() && !entity.navigationOpensDoors.has_value()) {
        entity.navigationOpensDoors = tweak.opensDoors;
    }
    if (tweak.passesDoors.has_value() && !entity.navigationPassesDoors.has_value()) {
        entity.navigationPassesDoors = tweak.passesDoors;
    }
    if (tweak.maxNoDamageTicks.has_value()) {
        entity.maxNoDamageTicks = *tweak.maxNoDamageTicks;
    }
    if (tweak.maxAirSupply.has_value() && !entity.maxAirOverride.has_value()) {
        entity.maxAirOverride = tweak.maxAirSupply;
        entity.airSupply = std::min(entity.airSupply, *tweak.maxAirSupply);
    }
}

bool griefing(const ServerEntity& entity, bool worldRule) {
    return entity.tuning != nullptr && entity.tuning->mobGriefing.has_value() ? *entity.tuning->mobGriefing : worldRule;
}

} // namespace basalt::server::runtime::entity::mobOverrides
