#include <basalt/server/runtime/config/WorldRules.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;
using wire::common::toml::TomlValue;

constexpr std::string_view kTemplate = R"(# Rules that change how this world plays. Every value shown is vanilla's.
# Anything that differs from vanilla is marked NOT VANILLA. Read when a world loads, so editing needs a restart.

[gamerules]
# Initial game rules of a world that is being created. A world that already has its level data keeps
# its own rules, and /gamerule still changes them afterwards. Names are the vanilla 26.3 ones, so
# pvp, spawn_monsters, advance_time and advance_weather live here too.
# keep_inventory = true
# pvp = false
# random_tick_speed = 6

[growth]
# How often plants try to grow, as a percent of vanilla's chance. 100 is vanilla, 0 stops the plant,
# 200 makes it grow twice as often. Anything but 100 is NOT VANILLA.
# Wheat, carrots, potatoes, beetroots, torchflower and pitcher plants.
crops = 100
# Pumpkin and melon stems.
stems = 100
saplings = 100
cactus = 100
sugar-cane = 100
bamboo = 100
vines = 100
kelp = 100
cave-vines = 100
twisting-vines = 100
weeping-vines = 100
nether-wart = 100
sweet-berries = 100
cocoa = 100
mushrooms = 100
chorus-flower = 100

[hopper]
# Ticks a hopper rests after moving an item. Vanilla is 8; anything else is NOT VANILLA.
transfer-ticks = 8
# Items moved each time a hopper acts, from the same stack. Vanilla is 1; anything else is NOT VANILLA.
amount = 1
# A hopper only looks for work on ticks that are a multiple of this. Vanilla is 1, every tick.
check-ticks = 1

[environment]
# One in this many chances per chunk and tick, while it thunders, that lightning strikes there.
thunder-chance = 100000
# How far, in blocks of the destination dimension times 8 for the Nether, a portal looks for an existing
# portal to come out of (vanilla: 128 around the destination in the Overworld, 16 in the Nether).
portal-search-radius = 128
# How far from the wanted spot a new portal may be built when none is found.
portal-create-radius = 16
# Whether falling out of the world hurts. Turn it off for a sky world that should never kill.
void-damage = true
# NOT VANILLA. Rain can still fall but it never thunders, so there is no lightning and no skeleton traps.
disable-thunder = false
# NOT VANILLA. Players at or above this height in the Nether take void damage, which keeps them off the roof.
# 0 turns it off, as vanilla has it.
nether-ceiling-void-damage-height = 0
# Ticks a bobber waits for a fish to come, drawn between the two (before Lure shortens it). Vanilla is 100 to 600.
fishing-min-wait-ticks = 100
fishing-max-wait-ticks = 600

[border]
# The world border a world STARTS with. A world that already has a saved border keeps it,
# and /worldborder changes it afterwards. Size is the full width in blocks.
size = 59999968.0
center-x = 0.0
center-z = 0.0
damage-per-block = 0.2
damage-buffer = 5.0
warning-distance = 5
warning-time = 15

[anti-xray]
# NOT VANILLA. Hides ores from clients that cheat by reading the chunk data they are sent. Blocks that
# touch nothing see-through are replaced in the chunk packets; whoever mines next to them is sent the
# real blocks. Players on Bedrock are not covered yet.
enabled = false
# 1: the hidden blocks are shown as the plain rock of the dimension (stone, deepslate, netherrack or end stone).
# 2: the hidden blocks and the replacement blocks are all shown as random hidden blocks, fake ores included.
# 3: as 2, but a whole layer of a section gets the same fake block, which is much cheaper.
engine-mode = 1
# Blocks that are hidden. In modes 2 and 3 they also make up the fake ores, so leave out blocks with
# an inventory.
hidden-blocks = ["copper_ore", "deepslate_copper_ore", "gold_ore", "deepslate_gold_ore", "iron_ore", "deepslate_iron_ore", "coal_ore", "deepslate_coal_ore", "lapis_ore", "deepslate_lapis_ore", "mossy_cobblestone", "obsidian", "diamond_ore", "deepslate_diamond_ore", "redstone_ore", "deepslate_redstone_ore", "clay", "emerald_ore", "deepslate_emerald_ore"]
# Modes 2 and 3 only: ordinary blocks that are swapped for fake ores as well.
replacement-blocks = ["stone", "oak_planks", "deepslate"]
# Only blocks below this height are touched, rounded down to a multiple of 16.
max-block-height = 64
# When a block is broken, the hidden blocks within this many blocks of it (0 to 2) are sent for real.
update-radius = 2
# Treat lava as solid, so ores behind a lava lake stay hidden.
lava-obscures = false
# Let players with the permission basalt.antixray.bypass see everything.
use-permission = false
)";

[[nodiscard]] std::optional<double> numberOf(const TomlTable& table, std::string_view key) {
    const TomlValue* value = table.find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    if (const auto* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    return std::nullopt;
}

[[nodiscard]] std::int32_t whole(const TomlTable& table, std::string_view key, std::int32_t fallback,
                                 std::int64_t least, std::int64_t most) {
    return static_cast<std::int32_t>(std::clamp(table.integer(key).value_or(fallback), least, most));
}

[[nodiscard]] double real(const TomlTable& table, std::string_view key, double fallback, double least, double most) {
    return std::clamp(numberOf(table, key).value_or(fallback), least, most);
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
    environment.voidDamage = table.boolean("void-damage").value_or(environment.voidDamage);
    environment.netherCeilingVoidDamageHeight =
        whole(table, "nether-ceiling-void-damage-height", environment.netherCeilingVoidDamageHeight, 0, 4096);
    environment.disableThunder = table.boolean("disable-thunder").value_or(environment.disableThunder);
    environment.fishingMinWaitTicks = whole(table, "fishing-min-wait-ticks", environment.fishingMinWaitTicks, 1, 72000);
    environment.fishingMaxWaitTicks =
        std::max(whole(table, "fishing-max-wait-ticks", environment.fishingMaxWaitTicks, 1, 72000),
                 environment.fishingMinWaitTicks);
}

void readAntiXray(const TomlTable& table, AntiXrayRules& rules) {
    rules.enabled = table.boolean("enabled").value_or(rules.enabled);
    rules.engineMode = whole(table, "engine-mode", rules.engineMode, 1, 3);
    if (table.find("hidden-blocks") != nullptr) {
        rules.hiddenBlocks = table.stringList("hidden-blocks");
    }
    if (table.find("replacement-blocks") != nullptr) {
        rules.replacementBlocks = table.stringList("replacement-blocks");
    }
    rules.maxBlockHeight = whole(table, "max-block-height", rules.maxBlockHeight, 0, 4096);
    rules.updateRadius = whole(table, "update-radius", rules.updateRadius, 0, 2);
    rules.lavaObscures = table.boolean("lava-obscures").value_or(rules.lavaObscures);
    rules.usePermission = table.boolean("use-permission").value_or(rules.usePermission);
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
    rules.simulationDistance = whole(*table, "simulation-distance", inherited.simulationDistance, 2, 32);
    rules.generateStructures = table->boolean("generate-structures").value_or(inherited.generateStructures);
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
    readWorldTuning(*table, rules.gameplay);
    if (const TomlTable* section = table->table("anti-xray"); section != nullptr) {
        readAntiXray(*section, rules.antiXray);
    }
    return rules;
}

std::string renderRulesTemplate(const std::string_view text, const bool forDefaults) {
    std::string out = "\n";
    std::int32_t depth = 0;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        const std::string_view line = text.substr(start, end - start);
        start = end + 1;
        const bool plain = line.empty() || line.front() == '#';
        if (depth == 0 && !plain && line.front() == '[') {
            out += forDefaults ? "[defaults." + std::string{line.substr(1)} : "# " + std::string{line};
        } else if (plain || forDefaults) {
            out += line;
        } else {
            out += "# " + std::string{line};
        }
        out += '\n';
        if (!plain) {
            for (const char symbol : line) {
                depth += symbol == '[' ? 1 : symbol == ']' ? -1 : 0;
            }
        }
    }
    return out;
}

std::string worldRulesTemplate(const bool forDefaults) {
    return renderRulesTemplate(kTemplate, forDefaults) + renderRulesTemplate(gameplayRulesTemplate(), forDefaults);
}

} // namespace basalt::server::runtime::config
