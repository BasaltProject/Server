#include "RuleErrors.hpp"

#include <basalt/server/runtime/config/WorldRules.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;
using wire::common::toml::TomlValue;

constexpr std::string_view kGameplayTemplate = R"([fire]
# Ticks a fire waits before it spreads or burns out; each tick adds a random 0 to 9 on top, as vanilla does. Vanilla
# is 30. Anything else is NOT VANILLA.
tick-delay = 30

[hunger]
# Exhaustion each action adds, vanilla's values. Anything else is NOT VANILLA.
sprint-exhaustion = 0.1
swim-exhaustion = 0.01
jump-exhaustion = 0.05
sprint-jump-exhaustion = 0.2
combat-exhaustion = 0.1
# Spent every time natural regeneration heals with a food bar of 18 or more.
regen-exhaustion = 6.0

[growth-height]
# How tall a plant may grow before it stops. Anything but vanilla's value is NOT VANILLA.
cactus = 3
sugar-cane = 3
bamboo = 16

[sounds]
# How far from the event, in blocks, players hear it. 0 is vanilla: the whole server hears it.
dragon-death-radius = 0.0
wither-spawn-radius = 0.0

[chunks]
# Chunks around the world spawn that stay loaded without a player. Vanilla keeps 1.
keep-spawn-loaded-range = 1
# Chunks the server snapshots in one go while it saves; each batch is a short turn on the tick thread, so a lower
# number smooths the tick and makes a save take longer.
autosave-batch-size = 8
# Chunks beyond a player's view distance that stay loaded before the unloading sweep takes them.
unload-margin = 4

[combat]
# NOT VANILLA. Hitting something with a knockback weapon does not stop the attacker from sprinting.
keep-sprint-on-attack = false

[block-updates]
# NOT VANILLA. Stops those blocks from reshaping when a neighbour changes (a note block keeps its instrument).
disable-noteblock-updates = false
disable-tripwire-updates = false
disable-chorus-plant-updates = false
disable-mushroom-block-updates = false

[explosions]
# NOT VANILLA. The most primed TNT that may go off in one tick; the rest wait for the next tick. -1 is no limit.
max-tnt-per-tick = -1

[maps]
# Markers a map may track before an item frame refuses to show it. Vanilla is 256.
item-frame-cursor-limit = 256
# Ticks between the checks that keep a map in an item frame up to date. Vanilla is 10.
item-frame-cursor-update-interval = 10

[end]
# NOT VANILLA. Leaving the End for the first time takes the player straight home, without the credits.
disable-credits = false

[leash]
# How far a leashed mob may get from its holder, in blocks, before the lead snaps. Vanilla is 12.
max-distance = 12.0

[command-blocks]
# Permission level the commands of command blocks and command block minecarts run at, 0 to 4. Vanilla is 2.
permission-level = 2

[collisions]
# Whether players push each other. Vanilla is true; false is NOT VANILLA.
enable-player-collisions = true
)";

[[nodiscard]] std::int32_t whole(const TomlTable& table, std::string_view key, std::int32_t fallback,
                                 std::int64_t least, std::int64_t most) {
    return ruleErrors::whole(table, key, fallback, least, most);
}

[[nodiscard]] float real(const TomlTable& table, std::string_view key, float fallback) {
    return static_cast<float>(ruleErrors::real(table, key, static_cast<double>(fallback), 0.0, 1000.0));
}

} // namespace

std::string_view tuningRulesTemplate() noexcept { return kGameplayTemplate; }

void readTuningRules(const TomlTable& table, TuningRules& rules) {
    if (const TomlTable* fire = table.table("fire"); fire != nullptr) {
        rules.fireTickDelay = whole(*fire, "tick-delay", rules.fireTickDelay, 1, 1 << 20);
    }
    if (const TomlTable* hunger = table.table("hunger"); hunger != nullptr) {
        HungerRules& h = rules.hunger;
        h.sprintExhaustion = real(*hunger, "sprint-exhaustion", h.sprintExhaustion);
        h.swimExhaustion = real(*hunger, "swim-exhaustion", h.swimExhaustion);
        h.jumpExhaustion = real(*hunger, "jump-exhaustion", h.jumpExhaustion);
        h.sprintJumpExhaustion = real(*hunger, "sprint-jump-exhaustion", h.sprintJumpExhaustion);
        h.combatExhaustion = real(*hunger, "combat-exhaustion", h.combatExhaustion);
        h.regenExhaustion = real(*hunger, "regen-exhaustion", h.regenExhaustion);
    }
    if (const TomlTable* height = table.table("growth-height"); height != nullptr) {
        rules.cactusMaxHeight = whole(*height, "cactus", rules.cactusMaxHeight, 1, 64);
        rules.sugarCaneMaxHeight = whole(*height, "sugar-cane", rules.sugarCaneMaxHeight, 1, 64);
        rules.bambooMaxHeight = whole(*height, "bamboo", rules.bambooMaxHeight, 1, 128);
    }
    if (const TomlTable* sounds = table.table("sounds"); sounds != nullptr) {
        rules.dragonDeathSoundRadius =
            ruleErrors::real(*sounds, "dragon-death-radius", rules.dragonDeathSoundRadius, 0.0, 1.0E7);
        rules.witherSpawnSoundRadius =
            ruleErrors::real(*sounds, "wither-spawn-radius", rules.witherSpawnSoundRadius, 0.0, 1.0E7);
    }
    if (const TomlTable* chunks = table.table("chunks"); chunks != nullptr) {
        rules.keepSpawnLoadedRange = whole(*chunks, "keep-spawn-loaded-range", rules.keepSpawnLoadedRange, 0, 32);
        rules.autosaveChunksPerBatch = whole(*chunks, "autosave-batch-size", rules.autosaveChunksPerBatch, 1, 1024);
        rules.unloadMarginChunks = whole(*chunks, "unload-margin", rules.unloadMarginChunks, 0, 64);
    }
    if (const TomlTable* combat = table.table("combat"); combat != nullptr) {
        rules.keepSprintOnAttack = ruleErrors::flag(combat, "keep-sprint-on-attack", rules.keepSprintOnAttack);
    }
    if (const TomlTable* updates = table.table("block-updates"); updates != nullptr) {
        rules.disableNoteBlockUpdates =
            ruleErrors::flag(updates, "disable-noteblock-updates", rules.disableNoteBlockUpdates);
        rules.disableTripwireUpdates =
            ruleErrors::flag(updates, "disable-tripwire-updates", rules.disableTripwireUpdates);
        rules.disableChorusPlantUpdates =
            ruleErrors::flag(updates, "disable-chorus-plant-updates", rules.disableChorusPlantUpdates);
        rules.disableMushroomBlockUpdates =
            ruleErrors::flag(updates, "disable-mushroom-block-updates", rules.disableMushroomBlockUpdates);
    }
    if (const TomlTable* explosions = table.table("explosions"); explosions != nullptr) {
        rules.maxTntPerTick = whole(*explosions, "max-tnt-per-tick", rules.maxTntPerTick, -1, 1 << 20);
    }
    if (const TomlTable* maps = table.table("maps"); maps != nullptr) {
        rules.mapMarkerLimit = whole(*maps, "item-frame-cursor-limit", rules.mapMarkerLimit, 0, 1 << 20);
        rules.mapFrameInterval = whole(*maps, "item-frame-cursor-update-interval", rules.mapFrameInterval, 1, 1 << 20);
    }
    if (const TomlTable* end = table.table("end"); end != nullptr) {
        rules.disableEndCredits = ruleErrors::flag(end, "disable-credits", rules.disableEndCredits);
    }
    if (const TomlTable* leash = table.table("leash"); leash != nullptr) {
        rules.leashSnapDistance = ruleErrors::real(*leash, "max-distance", rules.leashSnapDistance, 2.0, 256.0);
    }
    if (const TomlTable* blocks = table.table("command-blocks"); blocks != nullptr) {
        rules.commandBlockPermissionLevel = whole(*blocks, "permission-level", rules.commandBlockPermissionLevel, 0, 4);
    }
    if (const TomlTable* collisions = table.table("collisions"); collisions != nullptr) {
        rules.playerCollisions = ruleErrors::flag(collisions, "enable-player-collisions", rules.playerCollisions);
    }
}

} // namespace basalt::server::runtime::config
