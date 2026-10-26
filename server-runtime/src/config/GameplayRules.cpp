#include <basalt/server/runtime/config/GameplayRules.hpp>

#include <algorithm>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;

[[nodiscard]] std::int32_t whole(const TomlTable& table, std::string_view key, std::int32_t fallback,
                                 std::int64_t least) {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(table.integer(key).value_or(fallback), least, 1 << 20));
}

[[nodiscard]] float decimal(const TomlTable& table, std::string_view key, float fallback, double least) {
    return static_cast<float>(std::max(table.real(key).value_or(fallback), least));
}

[[nodiscard]] bool flag(const TomlTable& table, std::string_view key, bool fallback) {
    return table.boolean(key).value_or(fallback);
}

} // namespace

GameplayRules GameplayRules::from(const TomlTable* gameplay, const GameplayRules& inherited) {
    if (gameplay == nullptr) {
        return inherited;
    }
    GameplayRules rules = inherited;
    if (const TomlTable* table = gameplay->table("blocks"); table != nullptr) {
        rules.blocks.waterInfiniteRequiredSources =
            whole(*table, "water-infinite-required-sources", rules.blocks.waterInfiniteRequiredSources, 1);
        rules.blocks.lavaInfiniteRequiredSources =
            whole(*table, "lava-infinite-required-sources", rules.blocks.lavaInfiniteRequiredSources, -1);
        rules.blocks.lavaSpeedNether = whole(*table, "lava-speed-nether", rules.blocks.lavaSpeedNether, 1);
        rules.blocks.lavaSpeedNotNether = whole(*table, "lava-speed-not-nether", rules.blocks.lavaSpeedNotNether, 1);
        rules.blocks.spongeAbsorptionArea =
            whole(*table, "sponge-absorption-area", rules.blocks.spongeAbsorptionArea, 1);
        rules.blocks.spongeAbsorptionRadius =
            whole(*table, "sponge-absorption-radius", rules.blocks.spongeAbsorptionRadius, 1);
        rules.blocks.pistonBlockPushLimit =
            whole(*table, "piston-block-push-limit", rules.blocks.pistonBlockPushLimit, 1);
        rules.blocks.poweredRailActivationRange =
            whole(*table, "powered-rail-activation-range", rules.blocks.poweredRailActivationRange, 1);
        rules.blocks.magmaBlockDamageWhenSneaking =
            flag(*table, "magma-block-damage-when-sneaking", rules.blocks.magmaBlockDamageWhenSneaking);
        rules.blocks.bedExplode = flag(*table, "bed-explode", rules.blocks.bedExplode);
        rules.blocks.bedExplosionPower = decimal(*table, "bed-explosion-power", rules.blocks.bedExplosionPower, 0);
        rules.blocks.bedExplosionFire = flag(*table, "bed-explosion-fire", rules.blocks.bedExplosionFire);
        rules.blocks.campfireLitWhenPlaced =
            flag(*table, "campfire-lit-when-placed", rules.blocks.campfireLitWhenPlaced);
    }
    if (const TomlTable* table = gameplay->table("mechanics"); table != nullptr) {
        rules.mechanics.drowningTicksPerDamage =
            whole(*table, "drowning-ticks-per-damage", rules.mechanics.drowningTicksPerDamage, 1);
        rules.mechanics.drowningDamage = decimal(*table, "drowning-damage", rules.mechanics.drowningDamage, 0);
        rules.mechanics.healthRegenAmount =
            decimal(*table, "health-regen-amount", rules.mechanics.healthRegenAmount, 0);
        rules.mechanics.poisonDegenerationAmount =
            decimal(*table, "poison-degeneration-amount", rules.mechanics.poisonDegenerationAmount, 0);
        rules.mechanics.witherDegenerationAmount =
            decimal(*table, "wither-degeneration-amount", rules.mechanics.witherDegenerationAmount, 0);
        rules.mechanics.hungerExhaustionAmount =
            decimal(*table, "hunger-exhaustion-amount", rules.mechanics.hungerExhaustionAmount, 0);
    }
    if (const TomlTable* table = gameplay->table("player"); table != nullptr) {
        rules.player.criticalDamageMultiplier =
            decimal(*table, "critical-damage-multiplier", rules.player.criticalDamageMultiplier, 0);
        rules.player.totemOfUndyingWorksInInventory =
            flag(*table, "totem-of-undying-works-in-inventory", rules.player.totemOfUndyingWorksInInventory);
    }
    if (const TomlTable* table = gameplay->table("items"); table != nullptr) {
        rules.items.enderPearlDamage = decimal(*table, "ender-pearl-damage", rules.items.enderPearlDamage, 0);
    }
    return rules;
}

} // namespace basalt::server::runtime::config
