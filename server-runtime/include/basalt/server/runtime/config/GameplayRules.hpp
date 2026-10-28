#pragma once

#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>

namespace basalt::server::runtime::config {

struct GameplayRules {
    struct Blocks {
        std::int32_t waterInfiniteRequiredSources = 2;
        std::int32_t lavaInfiniteRequiredSources = -1;
        std::int32_t lavaSpeedNether = 10;
        std::int32_t lavaSpeedNotNether = 30;
        std::int32_t spongeAbsorptionArea = 65;
        std::int32_t spongeAbsorptionRadius = 6;
        std::int32_t pistonBlockPushLimit = 12;
        std::int32_t poweredRailActivationRange = 8;
        bool magmaBlockDamageWhenSneaking = false;
        bool bedExplode = true;
        float bedExplosionPower = 5.0F;
        bool bedExplosionFire = true;
        bool campfireLitWhenPlaced = true;
        bool cactusBreaksFromSolidNeighbors = true;
        bool coralDieOutsideWater = true;
        bool chestOpenWithSolidBlockOnTop = false;
        bool spongeAbsorbsLava = false;
        bool spongeAbsorbsWaterFromMud = false;
        bool noteBlockIgnoreAbove = false;
        std::int32_t beaconEffectRangeLevel1 = 20;
        std::int32_t beaconEffectRangeLevel2 = 30;
        std::int32_t beaconEffectRangeLevel3 = 40;
        std::int32_t beaconEffectRangeLevel4 = 50;
        bool beaconAllowEffectsWithTintedGlass = false;
        bool respawnAnchorExplode = true;
        float respawnAnchorExplosionPower = 5.0F;
        bool respawnAnchorExplosionFire = true;
        bool endCrystalBaselessExplode = true;
        float endCrystalBaselessExplosionPower = 6.0F;
        bool endCrystalBaselessExplosionFire = false;
        bool endCrystalBaseExplode = true;
        float endCrystalBaseExplosionPower = 6.0F;
        bool endCrystalBaseExplosionFire = false;
        bool endCrystalPlaceAnywhere = false;
        bool dragonEggTeleport = true;
        float stonecutterDamage = 0.0F;
        bool spawnerDeactivateByRedstone = false;
        std::int32_t lightningRodRange = 128;
        bool farmlandDisableTrampling = false;
        bool farmlandOnlyPlayersTrample = false;
        bool farmlandFeatherFallDistanceAffectsTrampling = false;
        float farmlandTrampleHeight = -1.0F;
        std::int32_t kelpMaxGrowthAge = 25;
        std::int32_t caveVinesMaxGrowthAge = 25;
        std::int32_t twistingVinesMaxGrowthAge = 25;
        std::int32_t weepingVinesMaxGrowthAge = 25;
        std::int32_t bigDripleafTiltDelayUnstable = 10;
        std::int32_t bigDripleafTiltDelayPartial = 10;
        std::int32_t bigDripleafTiltDelayFull = 100;
        bool magmaBlockReverseBubbleColumnFlow = false;
        bool soulSandReverseBubbleColumnFlow = false;
        bool anvilCumulativeCost = true;
        std::int32_t beehiveMaxBeesInside = 3;
        bool cactusAffectedByBonemeal = false;
        bool sugarCaneAffectedByBonemeal = false;
        bool netherWartAffectedByBonemeal = false;
        std::int32_t farmlandMobGriefingOverride = 0;
        std::int32_t fireballsMobGriefingOverride = 0;
        bool observerDisableClock = false;
        std::int32_t endCrystalCrammingAmount = 0;
        bool bedExplodeOnVillagerSleep = false;
        bool slabBreakIndividualSlabsWhenSneaking = false;
        bool silkTouchSpawnerEnabled = false;
        std::int32_t silkTouchMinimalLevel = 1;
        bool hoeReplantCrops = false;
        bool hoeReplantNetherWarts = false;
        bool signAllowColors = false;
        bool anvilAllowColors = false;
        bool anvilUseMiniMessage = false;
        float turtleEggHatchChance = 0.002F;
        std::int32_t turtleEggMobGriefingOverride = 0;
        friend bool operator==(const Blocks&, const Blocks&) = default;
    };

    struct Mechanics {
        std::int32_t drowningTicksPerDamage = 20;
        float drowningDamage = 2.0F;
        float healthRegenAmount = 1.0F;
        float poisonDegenerationAmount = 1.0F;
        float witherDegenerationAmount = 1.0F;
        float hungerExhaustionAmount = 0.005F;
        std::int32_t elytraDamagePerSecond = 1;
        std::int32_t elytraDamagePerFireworkBoost = 0;
        bool milkCuresBadOmen = true;
        bool milkClearsBeneficialEffects = true;
        std::int32_t raidCooldownSeconds = 0;
        std::int32_t animalBreedingCooldownSeconds = 0;
        bool rainStopsAfterSleep = true;
        bool thunderStopsAfterSleep = true;
        std::int32_t daylightDaytimeTicks = 12000;
        std::int32_t daylightNighttimeTicks = 12000;
        std::int32_t mobLastHurtByPlayerTime = 100;
        bool entitiesCanUsePortals = true;
        bool boatEjectPlayersOnLand = false;
        friend bool operator==(const Mechanics&, const Mechanics&) = default;
    };

    struct Player {
        float criticalDamageMultiplier = 1.5F;
        bool totemOfUndyingWorksInInventory = false;
        std::int32_t expPickupDelayTicks = 2;
        bool canSkipNight = true;
        bool sleepIgnoreNearbyMobs = false;
        bool creativeOnePunch = false;
        friend bool operator==(const Player&, const Player&) = default;
    };

    struct Items {
        float enderPearlDamage = 5.0F;
        std::int32_t enderPearlCooldown = 20;
        std::int32_t enderPearlCreativeCooldown = 20;
        float enderPearlEndermiteSpawnChance = 0.05F;
        std::int32_t snowballDamage = -1;
        float projectileOffsetBow = 1.0F;
        float projectileOffsetCrossbow = 1.0F;
        float projectileOffsetEgg = 1.0F;
        float projectileOffsetEnderPearl = 1.0F;
        float projectileOffsetThrowablePotion = 1.0F;
        float projectileOffsetTrident = 1.0F;
        float projectileOffsetSnowball = 1.0F;
        float projectileOffsetWindCharge = 1.0F;
        float minecartMaxSpeed = 0.4F;
        bool minecartPlaceAnywhere = false;
        float poweredRailBoostModifier = 0.06F;
        float tridentLoyaltyVoidReturnHeight = 0.0F;
        bool shulkerBoxDropContentsWhenDestroyed = true;
        friend bool operator==(const Items&, const Items&) = default;
    };

    Blocks blocks;
    Mechanics mechanics;
    Player player;
    Items items;

    /// A tri-state override: 0 follows the game rule, 1 forces it on, 2 forces it off.
    [[nodiscard]] static bool resolve(std::int32_t forced, bool rule) noexcept {
        return forced == 0 ? rule : forced == 1;
    }

    [[nodiscard]] static GameplayRules from(const wire::common::toml::TomlTable* gameplay,
                                            const GameplayRules& inherited);

    friend bool operator==(const GameplayRules&, const GameplayRules&) = default;
};

} // namespace basalt::server::runtime::config
