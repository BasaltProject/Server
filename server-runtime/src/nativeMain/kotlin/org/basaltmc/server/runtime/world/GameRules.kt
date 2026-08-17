package org.basaltmc.server.runtime.world

import org.basaltmc.wire.nbt.NbtCompound

/**
 * One game rule: its id, its default, and the bounds an integer rule accepts.
 *
 * 26.2 renamed most of them (`doDaylightCycle` is `advance_time`, `doMobSpawning` is `spawn_mobs`)
 * and the client resolves each id's label itself, so these are the new names verbatim.
 */
public sealed class GameRule<T>(
    public val id: String,
    public val default: T,
) {
    public class Bool(
        id: String,
        default: Boolean,
    ) : GameRule<Boolean>(id, default)

    public class Integer(
        id: String,
        default: Int,
        public val min: Int = kotlin.Int.MIN_VALUE,
        public val max: Int = kotlin.Int.MAX_VALUE,
    ) : GameRule<Int>(id, default)
}

/** parity: `GameRules`, in its own registration order, which is what `/gamerule` lists. */
public object GameRuleRegistry {
    public val ADVANCE_TIME: GameRule.Bool = GameRule.Bool("advance_time", true)
    public val ADVANCE_WEATHER: GameRule.Bool = GameRule.Bool("advance_weather", true)
    public val KEEP_INVENTORY: GameRule.Bool = GameRule.Bool("keep_inventory", false)
    public val MAX_BLOCK_MODIFICATIONS: GameRule.Integer = GameRule.Integer("max_block_modifications", 32768, min = 1)
    public val MAX_COMMAND_SEQUENCE_LENGTH: GameRule.Integer =
        GameRule.Integer("max_command_sequence_length", 65536, min = 0)
    public val MAX_COMMAND_FORKS: GameRule.Integer = GameRule.Integer("max_command_forks", 65536, min = 0)
    public val SEND_COMMAND_FEEDBACK: GameRule.Bool = GameRule.Bool("send_command_feedback", true)
    public val LOG_ADMIN_COMMANDS: GameRule.Bool = GameRule.Bool("log_admin_commands", true)
    public val SHOW_DEATH_MESSAGES: GameRule.Bool = GameRule.Bool("show_death_messages", true)
    public val RANDOM_TICK_SPEED: GameRule.Integer = GameRule.Integer("random_tick_speed", 3, min = 0)

    /** Named because the vine spread asks for it by name. parity: `GameRules.RULE_SPREAD_VINES`. */
    public val SPREAD_VINES: GameRule.Bool = GameRule.Bool("spread_vines", true)
    public val IMMEDIATE_RESPAWN: GameRule.Bool = GameRule.Bool("immediate_respawn", false)

    /** parity: the three rules a Nether portal reads before it takes anyone anywhere. */
    public val ALLOW_ENTERING_NETHER_USING_PORTALS: GameRule.Bool =
        GameRule.Bool("allow_entering_nether_using_portals", true)
    public val PLAYERS_NETHER_PORTAL_CREATIVE_DELAY: GameRule.Integer =
        GameRule.Integer("players_nether_portal_creative_delay", 0, min = 0)
    public val PLAYERS_NETHER_PORTAL_DEFAULT_DELAY: GameRule.Integer =
        GameRule.Integer("players_nether_portal_default_delay", 80, min = 0)

    /** parity: with this on, a player crafts only the recipes they have been taught. */
    public val LIMITED_CRAFTING: GameRule.Bool = GameRule.Bool("limited_crafting", false)

    /** parity: `show_advancement_messages`, the rule vanilla renamed from `announceAdvancements`. */
    public val SHOW_ADVANCEMENT_MESSAGES: GameRule.Bool = GameRule.Bool("show_advancement_messages", true)

    /** parity: what makes two water sources next to each other fill the block between them. */
    public val WATER_SOURCE_CONVERSION: GameRule.Bool = GameRule.Bool("water_source_conversion", true)

    /** parity: the share of the players who have to be in bed for the night to end. */
    public val PLAYERS_SLEEPING_PERCENTAGE: GameRule.Integer =
        GameRule.Integer(
            "players_sleeping_percentage",
            100,
            min = 0,
        )

    /** parity: what a fall costs, and what turning it off costs nothing. */
    public val FALL_DAMAGE: GameRule.Bool = GameRule.Bool("fall_damage", true)

    /** parity: off in vanilla, which is why lava has no infinite pools. */
    public val LAVA_SOURCE_CONVERSION: GameRule.Bool = GameRule.Bool("lava_source_conversion", false)

    public val ALL: List<GameRule<*>> =
        listOf(
            ADVANCE_TIME,
            ADVANCE_WEATHER,
            ALLOW_ENTERING_NETHER_USING_PORTALS,
            GameRule.Bool("block_drops", true),
            GameRule.Bool("block_explosion_drop_decay", true),
            GameRule.Bool("command_blocks_work", true),
            GameRule.Bool("command_block_output", true),
            GameRule.Bool("drowning_damage", true),
            GameRule.Bool("elytra_movement_check", true),
            GameRule.Bool("ender_pearls_vanish_on_death", true),
            GameRule.Bool("entity_drops", true),
            GameRule.Bool("fire_damage", true),
            GameRule.Integer("fire_spread_radius_around_player", 128, min = -1),
            GameRule.Bool("forgive_dead_players", true),
            GameRule.Bool("freeze_damage", true),
            GameRule.Bool("global_sound_events", true),
            IMMEDIATE_RESPAWN,
            KEEP_INVENTORY,
            LAVA_SOURCE_CONVERSION,
            LIMITED_CRAFTING,
            GameRule.Bool("locator_bar", true),
            LOG_ADMIN_COMMANDS,
            MAX_BLOCK_MODIFICATIONS,
            MAX_COMMAND_FORKS,
            MAX_COMMAND_SEQUENCE_LENGTH,
            GameRule.Integer("max_entity_cramming", 24, min = 0),
            GameRule.Integer("max_minecart_speed", 8, min = 1, max = 1000),
            GameRule.Integer("max_snow_accumulation_height", 1, min = 0, max = 8),
            GameRule.Bool("mob_drops", true),
            GameRule.Bool("mob_explosion_drop_decay", true),
            GameRule.Bool("mob_griefing", true),
            GameRule.Bool("natural_health_regeneration", true),
            GameRule.Bool("player_movement_check", true),
            PLAYERS_NETHER_PORTAL_CREATIVE_DELAY,
            PLAYERS_NETHER_PORTAL_DEFAULT_DELAY,
            GameRule.Bool("projectiles_can_break_blocks", true),
            GameRule.Bool("pvp", true),
            GameRule.Bool("raids", true),
            RANDOM_TICK_SPEED,
            GameRule.Bool("reduced_debug_info", false),
            GameRule.Integer("respawn_radius", 10, min = 0),
            SEND_COMMAND_FEEDBACK,
            SHOW_ADVANCEMENT_MESSAGES,
            SHOW_DEATH_MESSAGES,
            GameRule.Bool("spawner_blocks_work", true),
            GameRule.Bool("spawn_mobs", true),
            GameRule.Bool("spawn_monsters", true),
            GameRule.Bool("spawn_patrols", true),
            GameRule.Bool("spawn_phantoms", true),
            GameRule.Bool("spawn_wandering_traders", true),
            GameRule.Bool("spawn_wardens", true),
            GameRule.Bool("spectators_generate_chunks", true),
            SPREAD_VINES,
            GameRule.Bool("tnt_explodes", true),
            GameRule.Bool("tnt_explosion_drop_decay", false),
            GameRule.Bool("universal_anger", false),
            WATER_SOURCE_CONVERSION,
            FALL_DAMAGE,
            PLAYERS_SLEEPING_PERCENTAGE,
        ).sortedBy { it.id }

    public fun byId(id: String): GameRule<*>? {
        val bare = id.removePrefix("minecraft:")
        return ALL.firstOrNull { it.id == bare }
    }
}

/**
 * The values one world runs with. Everything not set explicitly answers with the vanilla default,
 * so a fresh world stores nothing and a `level.nbt` from an older build keeps working.
 *
 * Only the rules something already reads change behaviour; the rest are stored and reported so an
 * operator can set them before the system that honours them exists, which is what vanilla's own
 * data does across versions.
 */
public class GameRuleValues {
    private val values = mutableMapOf<String, String>()

    public operator fun get(rule: GameRule.Bool): Boolean = values[rule.id]?.toBooleanStrictOrNull() ?: rule.default

    public operator fun get(rule: GameRule.Integer): Int = values[rule.id]?.toIntOrNull() ?: rule.default

    /** The value as `/gamerule` prints it: `true`/`false` for a boolean, digits for an integer. */
    public fun serialized(rule: GameRule<*>): String =
        values[rule.id] ?: when (rule) {
            is GameRule.Bool -> rule.default.toString()
            is GameRule.Integer -> rule.default.toString()
        }

    public fun set(
        rule: GameRule<*>,
        value: String,
    ) {
        values[rule.id] = value
    }

    /** By the values set, so a [WorldMetadata] read back from disk equals the one that wrote it. */
    override fun equals(other: Any?): Boolean = other is GameRuleValues && values == other.values

    override fun hashCode(): Int = values.hashCode()

    override fun toString(): String = "GameRuleValues($values)"

    public fun writeTo(root: NbtCompound) {
        if (values.isEmpty()) return
        val rules = NbtCompound()
        for ((id, value) in values) rules.putString(id, value)
        root["GameRules"] = rules
    }

    public companion object {
        public fun readFrom(root: NbtCompound): GameRuleValues {
            val values = GameRuleValues()
            val rules = root["GameRules"] as? NbtCompound ?: return values
            for (id in rules.keys) {
                val rule = GameRuleRegistry.byId(id) ?: continue
                values.values[rule.id] = rules.getString(id)
            }
            return values
        }
    }
}
