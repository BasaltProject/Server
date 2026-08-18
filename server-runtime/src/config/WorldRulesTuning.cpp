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
# Chunks beyond a player's view distance that stay loaded before the unloading sweep takes them.
unload-margin = 4

[combat]
# NOT VANILLA. Hitting something with a knockback weapon does not stop the attacker from sprinting.
keep-sprint-on-attack = false

[collisions]
# Whether players push each other. Vanilla is true; false is NOT VANILLA.
enable-player-collisions = true
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

[[nodiscard]] float real(const TomlTable& table, std::string_view key, float fallback) {
    return static_cast<float>(std::clamp(numberOf(table, key).value_or(fallback), 0.0, 1000.0));
}

} // namespace

std::string_view gameplayRulesTemplate() noexcept { return kGameplayTemplate; }

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
            std::max(numberOf(*sounds, "dragon-death-radius").value_or(rules.dragonDeathSoundRadius), 0.0);
        rules.witherSpawnSoundRadius =
            std::max(numberOf(*sounds, "wither-spawn-radius").value_or(rules.witherSpawnSoundRadius), 0.0);
    }
    if (const TomlTable* chunks = table.table("chunks"); chunks != nullptr) {
        rules.keepSpawnLoadedRange = whole(*chunks, "keep-spawn-loaded-range", rules.keepSpawnLoadedRange, 0, 32);
        rules.unloadMarginChunks = whole(*chunks, "unload-margin", rules.unloadMarginChunks, 0, 64);
    }
    if (const TomlTable* combat = table.table("combat"); combat != nullptr) {
        rules.keepSprintOnAttack = combat->boolean("keep-sprint-on-attack").value_or(rules.keepSprintOnAttack);
    }
    if (const TomlTable* collisions = table.table("collisions"); collisions != nullptr) {
        rules.playerCollisions = collisions->boolean("enable-player-collisions").value_or(rules.playerCollisions);
    }
}

} // namespace basalt::server::runtime::config
