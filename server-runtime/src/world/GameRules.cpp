#include <basalt/server/runtime/world/GameRules.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

constexpr std::array kAll = {
    gameRules::kAdvanceTime,
    gameRules::kAdvanceWeather,
    gameRules::kAllowEnteringNetherUsingPortals,
    gameRules::kBlockDrops,
    GameRule::boolean("block_explosion_drop_decay", true),
    GameRule::boolean("command_block_output", true),
    gameRules::kCommandBlocksWork,
    GameRule::boolean("drowning_damage", true),
    GameRule::boolean("elytra_movement_check", true),
    GameRule::boolean("ender_pearls_vanish_on_death", true),
    gameRules::kEntityDrops,
    gameRules::kFallDamage,
    GameRule::boolean("fire_damage", true),
    GameRule::integer("fire_spread_radius_around_player", 128, -1),
    GameRule::boolean("forgive_dead_players", true),
    gameRules::kFreezeDamage,
    GameRule::boolean("global_sound_events", true),
    gameRules::kImmediateRespawn,
    gameRules::kKeepInventory,
    gameRules::kLavaSourceConversion,
    gameRules::kLimitedCrafting,
    GameRule::boolean("locator_bar", true),
    gameRules::kLogAdminCommands,
    gameRules::kMaxBlockModifications,
    gameRules::kMaxCommandForks,
    gameRules::kMaxCommandSequenceLength,
    gameRules::kMaxEntityCramming,
    gameRules::kMaxMinecartSpeed,
    GameRule::integer("max_snow_accumulation_height", 1, 0, 8),
    GameRule::boolean("mob_drops", true),
    GameRule::boolean("mob_explosion_drop_decay", true),
    GameRule::boolean("mob_griefing", true),
    GameRule::boolean("natural_health_regeneration", true),
    GameRule::boolean("player_movement_check", true),
    gameRules::kPlayersNetherPortalCreativeDelay,
    gameRules::kPlayersNetherPortalDefaultDelay,
    gameRules::kPlayersSleepingPercentage,
    GameRule::boolean("projectiles_can_break_blocks", true),
    gameRules::kPvp,
    GameRule::boolean("raids", true),
    gameRules::kRandomTickSpeed,
    GameRule::boolean("reduced_debug_info", false),
    GameRule::integer("respawn_radius", 10, 0),
    gameRules::kSendCommandFeedback,
    gameRules::kShowAdvancementMessages,
    gameRules::kShowDeathMessages,
    gameRules::kSpawnMobs,
    gameRules::kSpawnMonsters,
    gameRules::kSpawnPatrols,
    gameRules::kSpawnPhantoms,
    gameRules::kSpawnWanderingTraders,
    GameRule::boolean("spawn_wardens", true),
    gameRules::kSpawnerBlocksWork,
    GameRule::boolean("spectators_generate_chunks", true),
    gameRules::kSpreadVines,
    gameRules::kTntExplodes,
    GameRule::boolean("tnt_explosion_drop_decay", false),
    GameRule::boolean("universal_anger", false),
    gameRules::kWaterSourceConversion,
};

} // namespace

namespace gameRules {

std::span<const GameRule> all() { return kAll; }

const GameRule* byId(std::string_view id) {
    constexpr std::string_view kVanilla = "minecraft:";
    const std::string_view bare = id.starts_with(kVanilla) ? id.substr(kVanilla.size()) : id;
    const auto found = std::ranges::find(kAll, bare, &GameRule::id);
    return found == kAll.end() ? nullptr : &*found;
}

} // namespace gameRules

bool GameRuleValues::get(const GameRule& rule) const {
    const auto found = values_.find(rule.id);
    if (found == values_.end()) {
        return rule.defaultFlag;
    }
    // parity: `toBooleanStrictOrNull`.
    if (found->second == "true") {
        return true;
    }
    if (found->second == "false") {
        return false;
    }
    return rule.defaultFlag;
}

std::int32_t GameRuleValues::number(const GameRule& rule) const {
    const auto found = values_.find(rule.id);
    if (found == values_.end()) {
        return rule.defaultNumber;
    }
    std::int32_t parsed = 0;
    const char* first = found->second.data();
    const char* last = first + found->second.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last) {
        return rule.defaultNumber;
    }
    return parsed;
}

std::string GameRuleValues::serialized(const GameRule& rule) const {
    const auto found = values_.find(rule.id);
    if (found != values_.end()) {
        return found->second;
    }
    if (rule.kind == GameRule::Kind::Boolean) {
        return rule.defaultFlag ? "true" : "false";
    }
    return std::to_string(rule.defaultNumber);
}

void GameRuleValues::set(const GameRule& rule, std::string value) { values_[std::string{rule.id}] = std::move(value); }

void GameRuleValues::writeTo(wire::nbt::NbtCompound& root) const {
    if (values_.empty()) {
        return;
    }
    wire::nbt::NbtCompound rules;
    for (const auto& [id, value] : values_) {
        rules.putString(id, value);
    }
    root.set("GameRules", wire::nbt::NbtTag::ofCompound(std::move(rules)));
}

GameRuleValues GameRuleValues::readFrom(const wire::nbt::NbtCompound& root) {
    GameRuleValues values;
    const wire::nbt::NbtCompound* rules = root.getCompound("GameRules");
    if (rules == nullptr) {
        return values;
    }
    for (const std::string& id : rules->names()) {
        if (const GameRule* rule = gameRules::byId(id); rule != nullptr) {
            values.values_[std::string{rule->id}] = std::string{rules->getString(id)};
        }
    }
    return values;
}

} // namespace basalt::server::runtime::world
