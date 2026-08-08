#include <basalt/server/runtime/config/GameplayRules.hpp>

#include <algorithm>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;

[[nodiscard]] std::int32_t whole(const TomlTable* table, std::string_view key, std::int32_t fallback,
                                 std::int64_t least) {
    if (table == nullptr) {
        return fallback;
    }
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(table->integer(key).value_or(fallback), least, 1 << 20));
}

[[nodiscard]] float decimal(const TomlTable* table, std::string_view key, float fallback, double least) {
    if (table == nullptr) {
        return fallback;
    }
    return static_cast<float>(std::max(table->real(key).value_or(fallback), least));
}

[[nodiscard]] bool flag(const TomlTable* table, std::string_view key, bool fallback) {
    return table == nullptr ? fallback : table->boolean(key).value_or(fallback);
}

} // namespace

GameplayRules GameplayRules::from(const TomlTable* gameplay, const GameplayRules& inherited) {
    if (gameplay == nullptr) {
        return inherited;
    }
    GameplayRules rules = inherited;
    if (const TomlTable* table = gameplay->table("blocks"); table != nullptr) {
        Blocks& blocks = rules.blocks;
        blocks.waterInfiniteRequiredSources =
            whole(table, "water-infinite-required-sources", blocks.waterInfiniteRequiredSources, 1);
        blocks.lavaInfiniteRequiredSources =
            whole(table, "lava-infinite-required-sources", blocks.lavaInfiniteRequiredSources, -1);
        blocks.lavaSpeedNether = whole(table, "lava-speed-nether", blocks.lavaSpeedNether, 1);
        blocks.lavaSpeedNotNether = whole(table, "lava-speed-not-nether", blocks.lavaSpeedNotNether, 1);
        blocks.spongeAbsorptionArea = whole(table, "sponge-absorption-area", blocks.spongeAbsorptionArea, 1);
        blocks.spongeAbsorptionRadius = whole(table, "sponge-absorption-radius", blocks.spongeAbsorptionRadius, 1);
        blocks.pistonBlockPushLimit = whole(table, "piston-block-push-limit", blocks.pistonBlockPushLimit, 1);
        blocks.poweredRailActivationRange =
            whole(table, "powered-rail-activation-range", blocks.poweredRailActivationRange, 1);
        blocks.magmaBlockDamageWhenSneaking =
            flag(table, "magma-block-damage-when-sneaking", blocks.magmaBlockDamageWhenSneaking);
        blocks.bedExplode = flag(table, "bed-explode", blocks.bedExplode);
        blocks.bedExplosionPower = decimal(table, "bed-explosion-power", blocks.bedExplosionPower, 0.0);
        blocks.bedExplosionFire = flag(table, "bed-explosion-fire", blocks.bedExplosionFire);
        blocks.campfireLitWhenPlaced = flag(table, "campfire-lit-when-placed", blocks.campfireLitWhenPlaced);
    }
    if (const TomlTable* table = gameplay->table("mechanics"); table != nullptr) {
        Mechanics& mechanics = rules.mechanics;
        mechanics.drowningTicksPerDamage =
            whole(table, "drowning-ticks-per-damage", mechanics.drowningTicksPerDamage, 1);
        mechanics.drowningDamage = decimal(table, "drowning-damage", mechanics.drowningDamage, 0.0);
        mechanics.healthRegenAmount = decimal(table, "health-regen-amount", mechanics.healthRegenAmount, 0.0);
        mechanics.poisonDegenerationAmount =
            decimal(table, "poison-degeneration-amount", mechanics.poisonDegenerationAmount, 0.0);
        mechanics.witherDegenerationAmount =
            decimal(table, "wither-degeneration-amount", mechanics.witherDegenerationAmount, 0.0);
        mechanics.hungerExhaustionAmount =
            decimal(table, "hunger-exhaustion-amount", mechanics.hungerExhaustionAmount, 0.0);
    }
    if (const TomlTable* table = gameplay->table("player"); table != nullptr) {
        Player& player = rules.player;
        player.criticalDamageMultiplier =
            decimal(table, "critical-damage-multiplier", player.criticalDamageMultiplier, 0.0);
        player.totemOfUndyingWorksInInventory =
            flag(table, "totem-of-undying-works-in-inventory", player.totemOfUndyingWorksInInventory);
    }
    if (const TomlTable* table = gameplay->table("items"); table != nullptr) {
        rules.items.enderPearlDamage = decimal(table, "ender-pearl-damage", rules.items.enderPearlDamage, 0.0);
    }
    return rules;
}

} // namespace basalt::server::runtime::config
