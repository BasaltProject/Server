#include "RuleErrors.hpp"

#include <basalt/server/runtime/config/WorldRules.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;
using wire::common::toml::TomlValue;

[[nodiscard]] std::int32_t whole(const TomlTable& table, std::string_view key, std::int32_t fallback,
                                 std::int64_t least, std::int64_t most) {
    return ruleErrors::whole(table, key, fallback, least, most);
}

[[nodiscard]] double real(const TomlTable& table, std::string_view key, double fallback, double least, double most) {
    return ruleErrors::real(table, key, fallback, least, most);
}

void readGrowth(const TomlTable& table, GrowthPercents& growth) {
    constexpr std::int64_t kMost = 100000;
    growth.crops = whole(table, "crops", growth.crops, 0, kMost);
    growth.stems = whole(table, "stems", growth.stems, 0, kMost);
    growth.saplings = whole(table, "saplings", growth.saplings, 0, kMost);
    growth.cactus = whole(table, "cactus", growth.cactus, 0, kMost);
    growth.sugarCane = whole(table, "sugar-cane", growth.sugarCane, 0, kMost);
    growth.bamboo = whole(table, "bamboo", growth.bamboo, 0, kMost);
    growth.vines = whole(table, "vines", growth.vines, 0, kMost);
    growth.kelp = whole(table, "kelp", growth.kelp, 0, kMost);
    growth.caveVines = whole(table, "cave-vines", growth.caveVines, 0, kMost);
    growth.twistingVines = whole(table, "twisting-vines", growth.twistingVines, 0, kMost);
    growth.weepingVines = whole(table, "weeping-vines", growth.weepingVines, 0, kMost);
    growth.netherWart = whole(table, "nether-wart", growth.netherWart, 0, kMost);
    growth.sweetBerries = whole(table, "sweet-berries", growth.sweetBerries, 0, kMost);
    growth.cocoa = whole(table, "cocoa", growth.cocoa, 0, kMost);
    growth.mushrooms = whole(table, "mushrooms", growth.mushrooms, 0, kMost);
    growth.chorusFlower = whole(table, "chorus-flower", growth.chorusFlower, 0, kMost);
}

void readBorder(const TomlTable& table, BorderRules& border) {
    border.size = real(table, "size", border.size, 1.0, 5.9999968E7);
    border.centerX = real(table, "center-x", border.centerX, -2.9999984E7, 2.9999984E7);
    border.centerZ = real(table, "center-z", border.centerZ, -2.9999984E7, 2.9999984E7);
    border.damagePerBlock = real(table, "damage-per-block", border.damagePerBlock, 0.0, 1.0E6);
    border.damageBuffer = real(table, "damage-buffer", border.damageBuffer, 0.0, 1.0E6);
    border.warningDistance = whole(table, "warning-distance", border.warningDistance, 0, 59999968);
    border.warningSeconds = whole(table, "warning-time", border.warningSeconds, 0, 86400);
}

void readEnvironment(const TomlTable& table, EnvironmentRules& environment) {
    environment.thunderChance = whole(table, "thunder-chance", environment.thunderChance, 1, 1 << 30);
    environment.portalSearchRadius = whole(table, "portal-search-radius", environment.portalSearchRadius, 1, 512);
    environment.portalCreateRadius = whole(table, "portal-create-radius", environment.portalCreateRadius, 1, 128);
    environment.voidDamage = ruleErrors::flag(table, "void-damage", environment.voidDamage);
    environment.netherCeilingVoidDamageHeight =
        whole(table, "nether-ceiling-void-damage-height", environment.netherCeilingVoidDamageHeight, 0, 4096);
    environment.disableIceAndSnow = ruleErrors::flag(table, "disable-ice-and-snow", environment.disableIceAndSnow);
    environment.disableExplosionKnockback =
        ruleErrors::flag(table, "disable-explosion-knockback", environment.disableExplosionKnockback);
    environment.disableThunder = ruleErrors::flag(table, "disable-thunder", environment.disableThunder);
    environment.fishingMinWaitTicks = whole(table, "fishing-min-wait-ticks", environment.fishingMinWaitTicks, 1, 72000);
    environment.fishingMaxWaitTicks =
        std::max(whole(table, "fishing-max-wait-ticks", environment.fishingMaxWaitTicks, 1, 72000),
                 environment.fishingMinWaitTicks);
}

void readAntiXray(const TomlTable& table, AntiXrayRules& rules) {
    rules.enabled = ruleErrors::flag(table, "enabled", rules.enabled);
    rules.engineMode = whole(table, "engine-mode", rules.engineMode, 1, 3);
    if (table.find("hidden-blocks") != nullptr) {
        rules.hiddenBlocks = table.stringList("hidden-blocks");
    }
    if (table.find("replacement-blocks") != nullptr) {
        rules.replacementBlocks = table.stringList("replacement-blocks");
    }
    rules.maxBlockHeight = whole(table, "max-block-height", rules.maxBlockHeight, 0, 4096);
    rules.updateRadius = whole(table, "update-radius", rules.updateRadius, 0, 2);
    rules.lavaObscures = ruleErrors::flag(table, "lava-obscures", rules.lavaObscures);
    rules.usePermission = ruleErrors::flag(table, "use-permission", rules.usePermission);
}

void readGameRules(const TomlTable& table, std::map<std::string, std::string>& rules) {
    for (const std::string& name : table.keys()) {
        const TomlValue* value = table.find(name);
        if (value == nullptr) {
            continue;
        }
        if (const auto* flag = value->get<bool>(); flag != nullptr) {
            rules.insert_or_assign(name, *flag ? "true" : "false");
        } else if (const auto* number = value->get<std::int64_t>(); number != nullptr) {
            rules.insert_or_assign(name, std::to_string(*number));
        } else {
            ruleErrors::report("gamerules." + name, "must be true, false or a whole number");
        }
    }
}

} // namespace

std::int32_t GrowthPercents::forBlock(const std::string_view path) const noexcept {
    if (path == "wheat" || path == "carrots" || path == "potatoes" || path == "beetroots" ||
        path == "torchflower_crop" || path == "pitcher_crop") {
        return crops;
    }
    if (path == "pumpkin_stem" || path == "melon_stem") {
        return stems;
    }
    if (path == "cactus") {
        return cactus;
    }
    if (path == "sugar_cane") {
        return sugarCane;
    }
    if (path == "bamboo" || path == "bamboo_sapling") {
        return bamboo;
    }
    if (path == "vine") {
        return vines;
    }
    if (path == "kelp") {
        return kelp;
    }
    if (path == "cave_vines") {
        return caveVines;
    }
    if (path == "twisting_vines") {
        return twistingVines;
    }
    if (path == "weeping_vines") {
        return weepingVines;
    }
    if (path == "nether_wart") {
        return netherWart;
    }
    if (path == "sweet_berry_bush") {
        return sweetBerries;
    }
    if (path == "cocoa") {
        return cocoa;
    }
    if (path == "brown_mushroom" || path == "red_mushroom") {
        return mushrooms;
    }
    if (path == "chorus_flower") {
        return chorusFlower;
    }
    return 100;
}

AntiXrayRules AntiXrayRules::vanilla() {
    AntiXrayRules rules;
    rules.hiddenBlocks = {"copper_ore",
                          "deepslate_copper_ore",
                          "gold_ore",
                          "deepslate_gold_ore",
                          "iron_ore",
                          "deepslate_iron_ore",
                          "coal_ore",
                          "deepslate_coal_ore",
                          "lapis_ore",
                          "deepslate_lapis_ore",
                          "mossy_cobblestone",
                          "obsidian",
                          "diamond_ore",
                          "deepslate_diamond_ore",
                          "redstone_ore",
                          "deepslate_redstone_ore",
                          "clay",
                          "emerald_ore",
                          "deepslate_emerald_ore"};
    rules.replacementBlocks = {"stone", "oak_planks", "deepslate"};
    return rules;
}

WorldRules WorldRules::from(const TomlTable* table, const WorldRules& inherited) {
    if (table == nullptr) {
        return inherited;
    }
    WorldRules rules = inherited;
    rules.generateStructures = ruleErrors::flag(table, "generate-structures", inherited.generateStructures);
    rules.maxWorldSize = whole(*table, "max-world-size", inherited.maxWorldSize, 1, 29999984);
    if (const TomlTable* section = table->table("gamerules"); section != nullptr) {
        readGameRules(*section, rules.gameRules);
    }
    if (const TomlTable* section = table->table("growth"); section != nullptr) {
        readGrowth(*section, rules.growth);
    }
    if (const TomlTable* section = table->table("hopper"); section != nullptr) {
        rules.hopperTransferTicks = whole(*section, "transfer-ticks", rules.hopperTransferTicks, 1, 1 << 20);
        rules.hopperAmount = whole(*section, "amount", rules.hopperAmount, 1, 64);
        rules.hopperCheckTicks = whole(*section, "check-ticks", rules.hopperCheckTicks, 1, 1 << 20);
    }
    if (const TomlTable* section = table->table("environment"); section != nullptr) {
        readEnvironment(*section, rules.environment);
    }
    if (const TomlTable* section = table->table("border"); section != nullptr) {
        readBorder(*section, rules.border);
    }
    readTuningRules(*table, rules.tuning);
    if (const TomlTable* section = table->table("anti-xray"); section != nullptr) {
        readAntiXray(*section, rules.antiXray);
    }
    return rules;
}

} // namespace basalt::server::runtime::config
