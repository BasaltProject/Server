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
        friend bool operator==(const Blocks&, const Blocks&) = default;
    };

    struct Mechanics {
        std::int32_t drowningTicksPerDamage = 20;
        float drowningDamage = 2.0F;
        float healthRegenAmount = 1.0F;
        float poisonDegenerationAmount = 1.0F;
        float witherDegenerationAmount = 1.0F;
        float hungerExhaustionAmount = 0.005F;
        friend bool operator==(const Mechanics&, const Mechanics&) = default;
    };

    struct Player {
        float criticalDamageMultiplier = 1.5F;
        bool totemOfUndyingWorksInInventory = false;
        friend bool operator==(const Player&, const Player&) = default;
    };

    struct Items {
        float enderPearlDamage = 5.0F;
        friend bool operator==(const Items&, const Items&) = default;
    };

    Blocks blocks;
    Mechanics mechanics;
    Player player;
    Items items;

    [[nodiscard]] static GameplayRules from(const wire::common::toml::TomlTable* gameplay,
                                            const GameplayRules& inherited);

    friend bool operator==(const GameplayRules&, const GameplayRules&) = default;
};

} // namespace basalt::server::runtime::config
