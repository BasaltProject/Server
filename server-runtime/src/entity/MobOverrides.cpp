#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/world/loot/LootContext.hpp>

#include <algorithm>
#include <string>

namespace basalt::server::runtime::entity::mobOverrides {
namespace {

void setBase(ServerEntity& entity, std::string_view id, const std::optional<double>& value) {
    if (value.has_value()) {
        entity.attributeValues.insert_or_assign(std::string{id}, *value);
    }
}

[[nodiscard]] double unitFrom(const ServerEntity& entity, std::uint64_t salt) {
    std::uint64_t mixed = (entity.uuid().high() ^ (entity.uuid().low() * 0x9E3779B97F4A7C15ULL)) + salt;
    mixed ^= mixed >> 33;
    mixed *= 0xFF51AFD7ED558CCDULL;
    mixed ^= mixed >> 33;
    return static_cast<double>(mixed >> 11) / static_cast<double>(1ULL << 53);
}

[[nodiscard]] std::optional<double> pickBetween(const ServerEntity& entity, std::uint64_t salt,
                                                const std::optional<double>& low, const std::optional<double>& high) {
    if (low.has_value() && high.has_value()) {
        const double from = std::min(*low, *high);
        return from + (std::max(*low, *high) - from) * unitFrom(entity, salt);
    }
    return low.has_value() ? low : high;
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
    setBase(entity, attribute::kMaxHealth,
            tweak.maxHealth.has_value() ? tweak.maxHealth
                                        : pickBetween(entity, 1, tweak.maxHealthMin, tweak.maxHealthMax));
    setBase(entity, "minecraft:tempt_range", tweak.temptRange);
    setBase(entity, "minecraft:gravity", tweak.gravity);
    setBase(entity, "minecraft:step_height", tweak.stepHeight);
    setBase(entity, attribute::kAttackDamage, tweak.attackDamage);
    setBase(entity, attribute::kMovementSpeed,
            tweak.movementSpeed.has_value() ? tweak.movementSpeed
                                            : pickBetween(entity, 2, tweak.movementSpeedMin, tweak.movementSpeedMax));
    setBase(entity, "minecraft:follow_range", tweak.followRange);
    setBase(entity, attribute::kArmor, tweak.armor);
    setBase(entity, "minecraft:armor_toughness", tweak.armorToughness);
    setBase(entity, "minecraft:knockback_resistance", tweak.knockbackResistance);
    setBase(entity, "minecraft:attack_knockback", tweak.attackKnockback);
    setBase(entity, "minecraft:flying_speed", tweak.flyingSpeed);
    setBase(entity, "minecraft:jump_strength",
            tweak.jumpStrength.has_value() ? tweak.jumpStrength
                                           : pickBetween(entity, 3, tweak.jumpStrengthMin, tweak.jumpStrengthMax));
    setBase(entity, attribute::kScale, tweak.scale);
    setBase(entity, "minecraft:safe_fall_distance", tweak.safeFallDistance);
    setBase(entity, "minecraft:fall_damage_multiplier", tweak.fallDamageMultiplier);
    setBase(entity, "minecraft:explosion_knockback_resistance", tweak.explosionKnockbackResistance);
    setBase(entity, "minecraft:oxygen_bonus", tweak.oxygenBonus);
    if (entity.attributeValues.contains(attribute::kMaxHealth)) {
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

void customDrops(const ServerEntity& dead, std::vector<server::world::loot::ItemStack>& loot,
                 random::RandomSource& random) {
    if (dead.tuning == nullptr || (dead.tuning->drops.empty() && dead.tuning->dropsReplace != true)) {
        return;
    }
    if (dead.tuning->dropsReplace == true) {
        loot.clear();
    }
    for (const config::MobDrop& drop : dead.tuning->drops) {
        if (drop.chance < 1.0 && random.nextDouble() >= drop.chance) {
            continue;
        }
        const std::int32_t count = drop.maxCount > drop.minCount
                                       ? drop.minCount + random.nextInt(drop.maxCount - drop.minCount + 1)
                                       : drop.minCount;
        const auto item = wire::common::Identifier::parse(drop.item);
        if (count > 0 && item.has_value()) {
            loot.push_back(server::world::loot::ItemStack{.item = *item, .count = count});
        }
    }
}

} // namespace basalt::server::runtime::entity::mobOverrides
