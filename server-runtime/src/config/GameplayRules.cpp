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
        rules.blocks.cactusBreaksFromSolidNeighbors =
            flag(*table, "cactus-breaks-from-solid-neighbors", rules.blocks.cactusBreaksFromSolidNeighbors);
        rules.blocks.coralDieOutsideWater = flag(*table, "coral-die-outside-water", rules.blocks.coralDieOutsideWater);
        rules.blocks.chestOpenWithSolidBlockOnTop =
            flag(*table, "chest-open-with-solid-block-on-top", rules.blocks.chestOpenWithSolidBlockOnTop);
        rules.blocks.spongeAbsorbsLava = flag(*table, "sponge-absorbs-lava", rules.blocks.spongeAbsorbsLava);
        rules.blocks.spongeAbsorbsWaterFromMud =
            flag(*table, "sponge-absorbs-water-from-mud", rules.blocks.spongeAbsorbsWaterFromMud);
        rules.blocks.noteBlockIgnoreAbove = flag(*table, "note-block-ignore-above", rules.blocks.noteBlockIgnoreAbove);
        rules.blocks.beaconEffectRangeLevel1 =
            whole(*table, "beacon-effect-range-level-1", rules.blocks.beaconEffectRangeLevel1, 0);
        rules.blocks.beaconEffectRangeLevel2 =
            whole(*table, "beacon-effect-range-level-2", rules.blocks.beaconEffectRangeLevel2, 0);
        rules.blocks.beaconEffectRangeLevel3 =
            whole(*table, "beacon-effect-range-level-3", rules.blocks.beaconEffectRangeLevel3, 0);
        rules.blocks.beaconEffectRangeLevel4 =
            whole(*table, "beacon-effect-range-level-4", rules.blocks.beaconEffectRangeLevel4, 0);
        rules.blocks.beaconAllowEffectsWithTintedGlass =
            flag(*table, "beacon-allow-effects-with-tinted-glass", rules.blocks.beaconAllowEffectsWithTintedGlass);
        rules.blocks.respawnAnchorExplode = flag(*table, "respawn-anchor-explode", rules.blocks.respawnAnchorExplode);
        rules.blocks.respawnAnchorExplosionPower =
            decimal(*table, "respawn-anchor-explosion-power", rules.blocks.respawnAnchorExplosionPower, 0);
        rules.blocks.respawnAnchorExplosionFire =
            flag(*table, "respawn-anchor-explosion-fire", rules.blocks.respawnAnchorExplosionFire);
        rules.blocks.endCrystalBaselessExplode =
            flag(*table, "end-crystal-baseless-explode", rules.blocks.endCrystalBaselessExplode);
        rules.blocks.endCrystalBaselessExplosionPower =
            decimal(*table, "end-crystal-baseless-explosion-power", rules.blocks.endCrystalBaselessExplosionPower, 0);
        rules.blocks.endCrystalBaselessExplosionFire =
            flag(*table, "end-crystal-baseless-explosion-fire", rules.blocks.endCrystalBaselessExplosionFire);
        rules.blocks.endCrystalBaseExplode =
            flag(*table, "end-crystal-base-explode", rules.blocks.endCrystalBaseExplode);
        rules.blocks.endCrystalBaseExplosionPower =
            decimal(*table, "end-crystal-base-explosion-power", rules.blocks.endCrystalBaseExplosionPower, 0);
        rules.blocks.endCrystalBaseExplosionFire =
            flag(*table, "end-crystal-base-explosion-fire", rules.blocks.endCrystalBaseExplosionFire);
        rules.blocks.endCrystalPlaceAnywhere =
            flag(*table, "end-crystal-place-anywhere", rules.blocks.endCrystalPlaceAnywhere);
        rules.blocks.dragonEggTeleport = flag(*table, "dragon-egg-teleport", rules.blocks.dragonEggTeleport);
        rules.blocks.stonecutterDamage = decimal(*table, "stonecutter-damage", rules.blocks.stonecutterDamage, 0);
        rules.blocks.spawnerDeactivateByRedstone =
            flag(*table, "spawner-deactivate-by-redstone", rules.blocks.spawnerDeactivateByRedstone);
        rules.blocks.lightningRodRange = whole(*table, "lightning-rod-range", rules.blocks.lightningRodRange, 1);
        rules.blocks.farmlandDisableTrampling =
            flag(*table, "farmland-disable-trampling", rules.blocks.farmlandDisableTrampling);
        rules.blocks.farmlandOnlyPlayersTrample =
            flag(*table, "farmland-only-players-trample", rules.blocks.farmlandOnlyPlayersTrample);
        rules.blocks.farmlandFeatherFallDistanceAffectsTrampling =
            flag(*table, "farmland-feather-fall-distance-affects-trampling",
                 rules.blocks.farmlandFeatherFallDistanceAffectsTrampling);
        rules.blocks.farmlandTrampleHeight =
            decimal(*table, "farmland-trample-height", rules.blocks.farmlandTrampleHeight, -1);
        rules.blocks.kelpMaxGrowthAge = whole(*table, "kelp-max-growth-age", rules.blocks.kelpMaxGrowthAge, 0);
        rules.blocks.caveVinesMaxGrowthAge =
            whole(*table, "cave-vines-max-growth-age", rules.blocks.caveVinesMaxGrowthAge, 0);
        rules.blocks.twistingVinesMaxGrowthAge =
            whole(*table, "twisting-vines-max-growth-age", rules.blocks.twistingVinesMaxGrowthAge, 0);
        rules.blocks.weepingVinesMaxGrowthAge =
            whole(*table, "weeping-vines-max-growth-age", rules.blocks.weepingVinesMaxGrowthAge, 0);
        rules.blocks.bigDripleafTiltDelayUnstable =
            whole(*table, "big-dripleaf-tilt-delay-unstable", rules.blocks.bigDripleafTiltDelayUnstable, 1);
        rules.blocks.bigDripleafTiltDelayPartial =
            whole(*table, "big-dripleaf-tilt-delay-partial", rules.blocks.bigDripleafTiltDelayPartial, 1);
        rules.blocks.bigDripleafTiltDelayFull =
            whole(*table, "big-dripleaf-tilt-delay-full", rules.blocks.bigDripleafTiltDelayFull, 1);
        rules.blocks.magmaBlockReverseBubbleColumnFlow =
            flag(*table, "magma-block-reverse-bubble-column-flow", rules.blocks.magmaBlockReverseBubbleColumnFlow);
        rules.blocks.soulSandReverseBubbleColumnFlow =
            flag(*table, "soul-sand-reverse-bubble-column-flow", rules.blocks.soulSandReverseBubbleColumnFlow);
        rules.blocks.anvilCumulativeCost = flag(*table, "anvil-cumulative-cost", rules.blocks.anvilCumulativeCost);
        rules.blocks.beehiveMaxBeesInside =
            whole(*table, "beehive-max-bees-inside", rules.blocks.beehiveMaxBeesInside, 1);
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
        rules.mechanics.elytraDamagePerSecond =
            whole(*table, "elytra-damage-per-second", rules.mechanics.elytraDamagePerSecond, 0);
        rules.mechanics.elytraDamagePerFireworkBoost =
            whole(*table, "elytra-damage-per-firework-boost", rules.mechanics.elytraDamagePerFireworkBoost, 0);
        rules.mechanics.milkCuresBadOmen = flag(*table, "milk-cures-bad-omen", rules.mechanics.milkCuresBadOmen);
        rules.mechanics.milkClearsBeneficialEffects =
            flag(*table, "milk-clears-beneficial-effects", rules.mechanics.milkClearsBeneficialEffects);
        rules.mechanics.raidCooldownSeconds =
            whole(*table, "raid-cooldown-seconds", rules.mechanics.raidCooldownSeconds, 0);
        rules.mechanics.animalBreedingCooldownSeconds =
            whole(*table, "animal-breeding-cooldown-seconds", rules.mechanics.animalBreedingCooldownSeconds, 0);
        rules.mechanics.rainStopsAfterSleep =
            flag(*table, "rain-stops-after-sleep", rules.mechanics.rainStopsAfterSleep);
        rules.mechanics.thunderStopsAfterSleep =
            flag(*table, "thunder-stops-after-sleep", rules.mechanics.thunderStopsAfterSleep);
        rules.mechanics.daylightDaytimeTicks =
            whole(*table, "daylight-daytime-ticks", rules.mechanics.daylightDaytimeTicks, 1);
        rules.mechanics.daylightNighttimeTicks =
            whole(*table, "daylight-nighttime-ticks", rules.mechanics.daylightNighttimeTicks, 1);
    }
    if (const TomlTable* table = gameplay->table("player"); table != nullptr) {
        rules.player.criticalDamageMultiplier =
            decimal(*table, "critical-damage-multiplier", rules.player.criticalDamageMultiplier, 0);
        rules.player.totemOfUndyingWorksInInventory =
            flag(*table, "totem-of-undying-works-in-inventory", rules.player.totemOfUndyingWorksInInventory);
        rules.player.expPickupDelayTicks = whole(*table, "exp-pickup-delay-ticks", rules.player.expPickupDelayTicks, 0);
        rules.player.canSkipNight = flag(*table, "can-skip-night", rules.player.canSkipNight);
        rules.player.sleepIgnoreNearbyMobs =
            flag(*table, "sleep-ignore-nearby-mobs", rules.player.sleepIgnoreNearbyMobs);
        rules.player.creativeOnePunch = flag(*table, "creative-one-punch", rules.player.creativeOnePunch);
    }
    if (const TomlTable* table = gameplay->table("items"); table != nullptr) {
        rules.items.enderPearlDamage = decimal(*table, "ender-pearl-damage", rules.items.enderPearlDamage, 0);
        rules.items.enderPearlCooldown = whole(*table, "ender-pearl-cooldown", rules.items.enderPearlCooldown, 0);
        rules.items.enderPearlCreativeCooldown =
            whole(*table, "ender-pearl-creative-cooldown", rules.items.enderPearlCreativeCooldown, 0);
        rules.items.enderPearlEndermiteSpawnChance =
            decimal(*table, "ender-pearl-endermite-spawn-chance", rules.items.enderPearlEndermiteSpawnChance, 0);
        rules.items.snowballDamage = whole(*table, "snowball-damage", rules.items.snowballDamage, -1);
        rules.items.projectileOffsetBow = decimal(*table, "projectile-offset-bow", rules.items.projectileOffsetBow, 0);
        rules.items.projectileOffsetCrossbow =
            decimal(*table, "projectile-offset-crossbow", rules.items.projectileOffsetCrossbow, 0);
        rules.items.projectileOffsetEgg = decimal(*table, "projectile-offset-egg", rules.items.projectileOffsetEgg, 0);
        rules.items.projectileOffsetEnderPearl =
            decimal(*table, "projectile-offset-ender-pearl", rules.items.projectileOffsetEnderPearl, 0);
        rules.items.projectileOffsetThrowablePotion =
            decimal(*table, "projectile-offset-throwable-potion", rules.items.projectileOffsetThrowablePotion, 0);
        rules.items.projectileOffsetTrident =
            decimal(*table, "projectile-offset-trident", rules.items.projectileOffsetTrident, 0);
        rules.items.projectileOffsetSnowball =
            decimal(*table, "projectile-offset-snowball", rules.items.projectileOffsetSnowball, 0);
        rules.items.projectileOffsetWindCharge =
            decimal(*table, "projectile-offset-wind-charge", rules.items.projectileOffsetWindCharge, 0);
        rules.items.minecartMaxSpeed = decimal(*table, "minecart-max-speed", rules.items.minecartMaxSpeed, 0);
        rules.items.minecartPlaceAnywhere = flag(*table, "minecart-place-anywhere", rules.items.minecartPlaceAnywhere);
        rules.items.poweredRailBoostModifier =
            decimal(*table, "powered-rail-boost-modifier", rules.items.poweredRailBoostModifier, 0);
    }
    return rules;
}

} // namespace basalt::server::runtime::config
