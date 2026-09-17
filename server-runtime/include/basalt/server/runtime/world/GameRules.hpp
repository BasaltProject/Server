#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// One game rule: its id, its default, and the bounds an integer rule accepts.
///
/// 26.2 renamed most of them (`doDaylightCycle` is `advance_time`, `doMobSpawning` is `spawn_mobs`)
/// and the client resolves each id's label itself, so these are the new names verbatim.
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

/// parity: `GameRules`, with the rules something already reads named so a caller asks for one by
/// name instead of by string.
namespace gameRules {

inline constexpr GameRule kAdvanceTime = GameRule::boolean("advance_time", true);
inline constexpr GameRule kAdvanceWeather = GameRule::boolean("advance_weather", true);
inline constexpr GameRule kKeepInventory = GameRule::boolean("keep_inventory", false);
inline constexpr GameRule kMaxBlockModifications = GameRule::integer("max_block_modifications", 32768, 1);
inline constexpr GameRule kMaxCommandSequenceLength = GameRule::integer("max_command_sequence_length", 65536, 0);

/// parity: `isCommandBlockEnabled`, which is what stops a command block running its command. The
/// cycle around it carries on either way: only the command is skipped.
inline constexpr GameRule kCommandBlocksWork = GameRule::boolean("command_blocks_work", true);
inline constexpr GameRule kMaxCommandForks = GameRule::integer("max_command_forks", 65536, 0);
inline constexpr GameRule kSendCommandFeedback = GameRule::boolean("send_command_feedback", true);
inline constexpr GameRule kLogAdminCommands = GameRule::boolean("log_admin_commands", true);
inline constexpr GameRule kShowDeathMessages = GameRule::boolean("show_death_messages", true);
inline constexpr GameRule kRandomTickSpeed = GameRule::integer("random_tick_speed", 3, 0);

/// Named because the vine spread asks for it by name. parity: `GameRules.SPREAD_VINES`.
inline constexpr GameRule kSpreadVines = GameRule::boolean("spread_vines", true);
inline constexpr GameRule kMobGriefing = GameRule::boolean("mob_griefing", true);

/// Named because a nether portal asks it before spawning anything.
inline constexpr GameRule kSpawnMonsters = GameRule::boolean("spawn_monsters", true);
inline constexpr GameRule kImmediateRespawn = GameRule::boolean("immediate_respawn", false);

/// parity: the three rules a Nether portal reads before it takes anyone anywhere.
inline constexpr GameRule kAllowEnteringNetherUsingPortals =
    GameRule::boolean("allow_entering_nether_using_portals", true);
inline constexpr GameRule kPlayersNetherPortalCreativeDelay =
    GameRule::integer("players_nether_portal_creative_delay", 0, 0);
inline constexpr GameRule kPlayersNetherPortalDefaultDelay =
    GameRule::integer("players_nether_portal_default_delay", 80, 0);

/// parity: with this on, a player crafts only the recipes they have been taught.
inline constexpr GameRule kLimitedCrafting = GameRule::boolean("limited_crafting", false);

/// parity: `show_advancement_messages`, the rule vanilla renamed from `announceAdvancements`.
inline constexpr GameRule kShowAdvancementMessages = GameRule::boolean("show_advancement_messages", true);

/// parity: `spawn_mobs`, which vanilla renamed from `doMobSpawning`.
inline constexpr GameRule kSpawnMobs = GameRule::boolean("spawn_mobs", true);

/// parity: the custom phantom spawner's independent switch.
inline constexpr GameRule kSpawnPhantoms = GameRule::boolean("spawn_phantoms", true);

/// parity: the custom pillager patrol spawner's independent switch.
inline constexpr GameRule kSpawnPatrols = GameRule::boolean("spawn_patrols", true);

/// parity: the periodic trader spawner's independent switch.
inline constexpr GameRule kSpawnWanderingTraders = GameRule::boolean("spawn_wandering_traders", true);

/// parity: what makes two water sources next to each other fill the block between them.
inline constexpr GameRule kWaterSourceConversion = GameRule::boolean("water_source_conversion", true);

/// parity: the share of the players who have to be in bed for the night to end.
inline constexpr GameRule kPlayersSleepingPercentage = GameRule::integer("players_sleeping_percentage", 100, 0);

/// parity: what a fall costs, and what turning it off costs nothing.
inline constexpr GameRule kFallDamage = GameRule::boolean("fall_damage", true);

/// parity: `TntBlock.prime` answers false with this off, and the caller then leaves the block where it
/// is: nothing lights and nothing is removed.
inline constexpr GameRule kTntExplodes = GameRule::boolean("tnt_explodes", true);

/// parity: `Block.popResource` and `Block.popExperience`, which are what every block break ends in.
/// With this off a block still breaks and still goes: what it drops is what stops.
inline constexpr GameRule kBlockDrops = GameRule::boolean("block_drops", true);

/// parity: `Entity.spawnAtLocation` answers nothing with this off, so a mob still dies and an item
/// frame still comes off the wall: what they leave behind is what stops.
inline constexpr GameRule kEntityDrops = GameRule::boolean("entity_drops", true);

/// parity: what a mob leaves when it dies, the dragon's experience included.
inline constexpr GameRule kMobDrops = GameRule::boolean("mob_drops", true);

/// parity: off in vanilla, which is why lava has no infinite pools.
inline constexpr GameRule kLavaSourceConversion = GameRule::boolean("lava_source_conversion", false);

/// Every rule, by id, which is the order `/gamerule` lists them in.
[[nodiscard]] std::span<const GameRule> all();

/// The rule with that id, with or without the `minecraft:` a command may carry.
[[nodiscard]] const GameRule* byId(std::string_view id);

} // namespace gameRules

/// The values one world runs with. Everything not set explicitly answers with the vanilla default,
/// so a fresh world stores nothing and a `level.nbt` from an older build keeps working.
///
/// Only the rules something already reads change behaviour; the rest are stored and reported so an
/// operator can set them before the system that honours them exists, which is what vanilla's own
/// data does across versions.
class GameRuleValues {
public:
    [[nodiscard]] bool get(const GameRule& rule) const;

    [[nodiscard]] std::int32_t number(const GameRule& rule) const;

    /// The value as `/gamerule` prints it: `true`/`false` for a boolean, digits for an integer.
    [[nodiscard]] std::string serialized(const GameRule& rule) const;

    void set(const GameRule& rule, std::string value);

    void writeTo(wire::nbt::NbtCompound& root) const;

    [[nodiscard]] static GameRuleValues readFrom(const wire::nbt::NbtCompound& root);

    /// By the values set, so world metadata read back from disk equals what wrote it.
    friend bool operator==(const GameRuleValues&, const GameRuleValues&) = default;

private:
    std::map<std::string, std::string, std::less<>> values_;
};

} // namespace basalt::server::runtime::world
