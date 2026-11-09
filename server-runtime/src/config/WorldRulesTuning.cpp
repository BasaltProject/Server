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

[[nodiscard]] float real(const TomlTable& table, std::string_view key, float fallback) {
    return static_cast<float>(ruleErrors::real(table, key, static_cast<double>(fallback), 0.0, 1000.0));
}

} // namespace

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
