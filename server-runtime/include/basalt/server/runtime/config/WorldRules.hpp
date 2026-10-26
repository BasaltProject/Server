#pragma once

#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

struct GrowthPercents {
    std::int32_t crops = 100;
    std::int32_t stems = 100;
    std::int32_t saplings = 100;
    std::int32_t cactus = 100;
    std::int32_t sugarCane = 100;
    std::int32_t bamboo = 100;
    std::int32_t vines = 100;
    std::int32_t kelp = 100;
    std::int32_t caveVines = 100;
    std::int32_t twistingVines = 100;
    std::int32_t weepingVines = 100;
    std::int32_t netherWart = 100;
    std::int32_t sweetBerries = 100;
    std::int32_t cocoa = 100;
    std::int32_t mushrooms = 100;
    std::int32_t chorusFlower = 100;

    [[nodiscard]] std::int32_t forBlock(std::string_view path) const noexcept;

    friend bool operator==(const GrowthPercents&, const GrowthPercents&) = default;
};

struct BorderRules {
    double size = 5.9999968E7;
    double centerX = 0.0;
    double centerZ = 0.0;
    double damagePerBlock = 0.2;
    double damageBuffer = 5.0;
    std::int32_t warningDistance = 5;
    std::int32_t warningSeconds = 15;

    friend bool operator==(const BorderRules&, const BorderRules&) = default;
};

struct EnvironmentRules {
    std::int32_t thunderChance = 100000;
    std::int32_t portalSearchRadius = 128;
    std::int32_t portalCreateRadius = 16;
    bool voidDamage = true;
    bool disableThunder = false;
    std::int32_t fishingMinWaitTicks = 100;
    std::int32_t fishingMaxWaitTicks = 600;
    std::int32_t netherCeilingVoidDamageHeight = 0;

    friend bool operator==(const EnvironmentRules&, const EnvironmentRules&) = default;
};

struct HungerRules {
    float sprintExhaustion = 0.1F;
    float swimExhaustion = 0.01F;
    float jumpExhaustion = 0.05F;
    float sprintJumpExhaustion = 0.2F;
    float combatExhaustion = 0.1F;
    float regenExhaustion = 6.0F;

    friend bool operator==(const HungerRules&, const HungerRules&) = default;
};

struct WorldTuning {
    std::int32_t fireTickDelay = 30;
    std::int32_t cactusMaxHeight = 3;
    std::int32_t sugarCaneMaxHeight = 3;
    std::int32_t bambooMaxHeight = 16;
    double dragonDeathSoundRadius = 0.0;
    double witherSpawnSoundRadius = 0.0;
    std::int32_t keepSpawnLoadedRange = 1;
    std::int32_t unloadMarginChunks = 4;
    bool keepSprintOnAttack = false;
    bool playerCollisions = true;
    HungerRules hunger;

    friend bool operator==(const WorldTuning&, const WorldTuning&) = default;
};

struct AntiXrayRules {
    bool enabled = false;
    std::int32_t engineMode = 1;
    std::vector<std::string> hiddenBlocks;
    std::vector<std::string> replacementBlocks;
    std::int32_t maxBlockHeight = 64;
    std::int32_t updateRadius = 2;
    bool lavaObscures = false;
    bool usePermission = false;

    [[nodiscard]] static AntiXrayRules vanilla();

    friend bool operator==(const AntiXrayRules&, const AntiXrayRules&) = default;
};

struct WorldRules {
    std::int32_t simulationDistance = 10;
    bool generateStructures = true;
    std::int32_t maxWorldSize = 29999984;
    std::map<std::string, std::string> gameRules;
    GrowthPercents growth;
    std::int32_t hopperTransferTicks = 8;
    std::int32_t hopperAmount = 1;
    std::int32_t hopperCheckTicks = 1;
    WorldTuning gameplay;
    EnvironmentRules environment;
    BorderRules border;
    AntiXrayRules antiXray = AntiXrayRules::vanilla();

    [[nodiscard]] static WorldRules from(const wire::common::toml::TomlTable* table, const WorldRules& inherited);

    friend bool operator==(const WorldRules&, const WorldRules&) = default;
};

/// Shared with the gameplay half of the template, which lives in its own file.
[[nodiscard]] std::string renderRulesTemplate(std::string_view text, bool forDefaults);

[[nodiscard]] std::string_view gameplayRulesTemplate() noexcept;

void readWorldTuning(const wire::common::toml::TomlTable& table, WorldTuning& rules);

/// The commented sections of the rules, as `[defaults.x]` for worlds.toml or commented `[x]` for a world's own file.
[[nodiscard]] std::string worldRulesTemplate(bool forDefaults);

} // namespace basalt::server::runtime::config
