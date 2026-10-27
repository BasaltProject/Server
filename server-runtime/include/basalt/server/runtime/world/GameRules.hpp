#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

struct GameRule {
    enum class Kind : std::uint8_t { Boolean, Integer };

    std::string_view id;
    Kind kind = Kind::Boolean;
    bool defaultFlag = false;
    std::int32_t defaultNumber = 0;
    std::int32_t min = std::numeric_limits<std::int32_t>::min();
    std::int32_t max = std::numeric_limits<std::int32_t>::max();

    [[nodiscard]] static constexpr GameRule boolean(std::string_view id, bool defaultFlag) {
        return GameRule{.id = id, .kind = Kind::Boolean, .defaultFlag = defaultFlag};
    }

    [[nodiscard]] static constexpr GameRule integer(std::string_view id, std::int32_t defaultNumber,
                                                    std::int32_t min = std::numeric_limits<std::int32_t>::min(),
                                                    std::int32_t max = std::numeric_limits<std::int32_t>::max()) {
        return GameRule{.id = id, .kind = Kind::Integer, .defaultNumber = defaultNumber, .min = min, .max = max};
    }
};

namespace gameRules {

inline constexpr GameRule kAdvanceTime = GameRule::boolean("advance_time", true);
inline constexpr GameRule kAdvanceWeather = GameRule::boolean("advance_weather", true);
inline constexpr GameRule kKeepInventory = GameRule::boolean("keep_inventory", false);
inline constexpr GameRule kNaturalHealthRegeneration = GameRule::boolean("natural_health_regeneration", true);
inline constexpr GameRule kMaxBlockModifications = GameRule::integer("max_block_modifications", 32768, 1);
inline constexpr GameRule kMaxCommandSequenceLength = GameRule::integer("max_command_sequence_length", 65536, 0);

/// parity: `isCommandBlockEnabled` only skips the command.
inline constexpr GameRule kCommandBlocksWork = GameRule::boolean("command_blocks_work", true);
inline constexpr GameRule kRaids = GameRule::boolean("raids", true);
inline constexpr GameRule kMaxCommandForks = GameRule::integer("max_command_forks", 65536, 0);
inline constexpr GameRule kSendCommandFeedback = GameRule::boolean("send_command_feedback", true);
inline constexpr GameRule kLogAdminCommands = GameRule::boolean("log_admin_commands", true);
inline constexpr GameRule kShowDeathMessages = GameRule::boolean("show_death_messages", true);
inline constexpr GameRule kRandomTickSpeed = GameRule::integer("random_tick_speed", 3, 0);

/// parity: `max_snow_accumulation_height`.
inline constexpr GameRule kMaxSnowAccumulationHeight = GameRule::integer("max_snow_accumulation_height", 1, 0, 8);

inline constexpr GameRule kSpreadVines = GameRule::boolean("spread_vines", true);
inline constexpr GameRule kMobGriefing = GameRule::boolean("mob_griefing", true);

inline constexpr GameRule kUniversalAnger = GameRule::boolean("universal_anger", false);

inline constexpr GameRule kForgiveDeadPlayers = GameRule::boolean("forgive_dead_players", true);

inline constexpr GameRule kSpawnMonsters = GameRule::boolean("spawn_monsters", true);
inline constexpr GameRule kImmediateRespawn = GameRule::boolean("immediate_respawn", false);

/// parity: the three rules a Nether portal reads before it takes anyone anywhere.
inline constexpr GameRule kAllowEnteringNetherUsingPortals =
    GameRule::boolean("allow_entering_nether_using_portals", true);
inline constexpr GameRule kPlayersNetherPortalCreativeDelay =
    GameRule::integer("players_nether_portal_creative_delay", 0, 0);
inline constexpr GameRule kPlayersNetherPortalDefaultDelay =
    GameRule::integer("players_nether_portal_default_delay", 80, 0);

inline constexpr GameRule kLimitedCrafting = GameRule::boolean("limited_crafting", false);

/// parity: `show_advancement_messages`.
inline constexpr GameRule kShowAdvancementMessages = GameRule::boolean("show_advancement_messages", true);

/// parity: `spawn_mobs`, which vanilla renamed from `doMobSpawning`.
inline constexpr GameRule kSpawnMobs = GameRule::boolean("spawn_mobs", true);

inline constexpr GameRule kSpawnPhantoms = GameRule::boolean("spawn_phantoms", true);

inline constexpr GameRule kSpawnPatrols = GameRule::boolean("spawn_patrols", true);

inline constexpr GameRule kSpawnWardens = GameRule::boolean("spawn_wardens", true);

inline constexpr GameRule kSpawnerBlocksWork = GameRule::boolean("spawner_blocks_work", true);

inline constexpr GameRule kSpawnWanderingTraders = GameRule::boolean("spawn_wandering_traders", true);

inline constexpr GameRule kWaterSourceConversion = GameRule::boolean("water_source_conversion", true);

inline constexpr GameRule kPlayersSleepingPercentage = GameRule::integer("players_sleeping_percentage", 100, 0);

inline constexpr GameRule kFallDamage = GameRule::boolean("fall_damage", true);

inline constexpr GameRule kFreezeDamage = GameRule::boolean("freeze_damage", true);

/// parity: `TntBlock.prime` answers false with this off, leaving the block where it is.
inline constexpr GameRule kTntExplodes = GameRule::boolean("tnt_explodes", true);

/// parity: `ServerLevel.canSpreadFireAround`.
inline constexpr GameRule kFireSpreadRadiusAroundPlayer =
    GameRule::integer("fire_spread_radius_around_player", 128, -1);

/// parity: `Block.popResource`/`popExperience`.
inline constexpr GameRule kBlockDrops = GameRule::boolean("block_drops", true);

/// parity: `Entity.spawnAtLocation` answers nothing with this off.
inline constexpr GameRule kEntityDrops = GameRule::boolean("entity_drops", true);

inline constexpr GameRule kMobDrops = GameRule::boolean("mob_drops", true);

inline constexpr GameRule kMaxEntityCramming = GameRule::integer("max_entity_cramming", 24, 0);

inline constexpr GameRule kMaxMinecartSpeed = GameRule::integer("max_minecart_speed", 8, 1, 1000);

inline constexpr GameRule kPvp = GameRule::boolean("pvp", true);

/// parity: off in vanilla, which is why lava has no infinite pools.
inline constexpr GameRule kLavaSourceConversion = GameRule::boolean("lava_source_conversion", false);

/// Every rule by id, in the order `/gamerule` lists them.
[[nodiscard]] std::span<const GameRule> all();

[[nodiscard]] const GameRule* byId(std::string_view id);

} // namespace gameRules

/// Unset rules answer the vanilla default so an older `level.nbt` keeps working; unread rules are still stored.
class GameRuleValues {
public:
    [[nodiscard]] bool get(const GameRule& rule) const;

    [[nodiscard]] std::int32_t number(const GameRule& rule) const;

    [[nodiscard]] std::string serialized(const GameRule& rule) const;

    void set(const GameRule& rule, std::string value);

    void writeTo(wire::nbt::NbtCompound& root) const;

    [[nodiscard]] static GameRuleValues readFrom(const wire::nbt::NbtCompound& root);

    friend bool operator==(const GameRuleValues&, const GameRuleValues&) = default;

private:
    std::map<std::string, std::string, std::less<>> values_;
};

} // namespace basalt::server::runtime::world
