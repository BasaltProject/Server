package org.basaltmc.server.placeholder

import org.basaltmc.common.placeholder.PlaceholderCatalog
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderProvider
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.server.config.GroupsConfig
import org.basaltmc.server.config.PingRequest
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.storage.StoredStats
import kotlin.math.roundToInt

/**
 * `%basalt_player_*%`, answered from whoever the context is about.
 *
 * The same names serve both situations, which is the point: `%basalt_player_name%` in a chat format
 * means the player talking, and in the server list it means whoever this server recognised at that
 * address. A name that only one of the two can answer says so in the other rather than resolving to
 * something invented, and whoever used it gets told which one it was.
 *
 * Written as tables of functions rather than one long `when` so that listing the names and
 * answering them come from the same place. Two lists kept in step by hand drift, and the one that
 * drifts is always the one nobody runs.
 */
public class PlayerPlaceholders(
    private val groups: GroupsConfig,
    /** Looks a world up by name, so `dimension` can answer without the session carrying a copy of it. */
    private val worlds: () -> Map<String, org.basaltmc.server.world.LoadedWorld> = { emptyMap() },
) : PlaceholderProvider,
    PlaceholderCatalog {
    /** Answerable from a pending connection as well as from a player: what a handshake already carries. */
    private val eitherWay: Map<String, (PlayerSession?, PingRequest?) -> Any?> =
        mapOf(
            "protocol" to { session, ping -> ping?.protocolVersion ?: session?.protocolVersion },
            "host" to { session, ping -> ping?.host ?: session?.connectedHost },
            "ip" to { session, ping -> ping?.address ?: session?.address },
            "name" to { session, ping -> session?.name ?: ping?.knownPlayer },
            "displayname" to { session, ping -> session?.name ?: ping?.knownPlayer },
            "list_name" to { session, ping -> session?.name ?: ping?.knownPlayer },
            "known" to { session, ping -> if (session != null) true else ping?.let { it.knownPlayer != null } },
        )

    private val ofPlayer: Map<String, (PlayerSession) -> PlaceholderValue> =
        mapOf(
            "uuid" to { it -> PlaceholderValue.of(it.uuid.toString()) },
            "online" to { _ -> PlaceholderValue.of(true) },
            "world" to { it -> PlaceholderValue.of(it.worldName) },
            "dimension" to { it ->
                PlaceholderValue.of(worlds()[it.worldName]?.declaration?.dimension)
            },
            "x" to { it -> PlaceholderValue.of(it.x) },
            "y" to { it -> PlaceholderValue.of(it.y) },
            "z" to { it -> PlaceholderValue.of(it.z) },
            "yaw" to { it -> PlaceholderValue.of(it.yaw) },
            "pitch" to { it -> PlaceholderValue.of(it.pitch) },
            "direction" to { it -> PlaceholderValue.of(direction(it.yaw)) },
            "direction_xz" to { it -> PlaceholderValue.of(directionXZ(it.yaw)) },
            "gamemode" to { it -> PlaceholderValue.of(it.gamemode.name.lowercase()) },
            "group" to { it -> PlaceholderValue.of(groups[it.group].name) },
            "is_op" to { it -> PlaceholderValue.of(groups[it.group].isOperator) },
            "health" to { it -> PlaceholderValue.of(it.health) },
            "health_rounded" to { it -> PlaceholderValue.of(it.health.roundToInt()) },
            "max_health" to { _ -> PlaceholderValue.of(PlayerSession.MAX_HEALTH) },
            "max_health_rounded" to { _ -> PlaceholderValue.of(PlayerSession.MAX_HEALTH.roundToInt()) },
            "food_level" to { it -> PlaceholderValue.of(it.food.foodLevel) },
            "saturation" to { it -> PlaceholderValue.of(it.food.saturationLevel) },
            "level" to { it -> PlaceholderValue.of(it.experience.level) },
            "exp" to { it -> PlaceholderValue.of(it.experience.progress) },
            "current_exp" to { it ->
                PlaceholderValue.of((it.experience.progress * it.experience.xpNeededForNextLevel()).toInt())
            },
            "exp_to_level" to { it -> PlaceholderValue.of(it.experience.xpNeededForNextLevel()) },
            "total_exp" to { it -> PlaceholderValue.of(it.experience.total) },
            "is_flying" to { it -> PlaceholderValue.of(it.flying) },
            "allow_flight" to { it -> PlaceholderValue.of(it.mayFly) },
            "is_sprinting" to { it -> PlaceholderValue.of(it.sprinting) },
            "is_dead" to { it -> PlaceholderValue.of(it.dead) },
        )

    override fun names(): Collection<String> =
        (eitherWay.keys + ofPlayer.keys + NOT_IMPLEMENTED.keys + "$PERMISSION_PREFIX<node>").sorted()

    override fun resolve(
        params: String,
        context: PlaceholderContext,
    ): PlaceholderValue {
        val session = context[PlaceholderSlots.PLAYER]
        val ping = context[PlaceholderSlots.PING]

        eitherWay[params]?.let { answer ->
            val value = answer(session, ping) ?: return unanswered(ping)
            return PlaceholderValue.Text(value.toString())
        }
        NOT_IMPLEMENTED[params]?.let { return notImplemented(it) }
        if (params.startsWith(STAT_PREFIX) && params.length > STAT_PREFIX.length) {
            val key = params.removePrefix(STAT_PREFIX)
            // Written without the namespaces for readability: `stat_mined_stone` rather than the
            // full `minecraft:mined/minecraft:stone` a file would otherwise have to spell out.
            return session?.let { PlaceholderValue.of(it.stats[expandStatKey(key)]) } ?: needsPlayer(ping)
        }
        if (params.startsWith(PERMISSION_PREFIX) && params.length > PERMISSION_PREFIX.length) {
            val node = params.removePrefix(PERMISSION_PREFIX)
            return session?.let { PlaceholderValue.of(groups.resolve(it.group, node)) } ?: needsPlayer(ping)
        }
        val answer = ofPlayer[params] ?: return PlaceholderValue.Unknown
        return session?.let(answer) ?: needsPlayer(ping)
    }

    private fun needsPlayer(ping: PingRequest?): PlaceholderValue =
        PlaceholderValue.Unavailable(
            if (ping == null) NO_SUBJECT else "there is no player yet: this is a server list ping, before login",
        )

    /**
     * A name a ping *could* have answered and did not, which is not the same as having no subject.
     *
     * `%basalt_player_name%` in the server list is a fair question: it is asking who is looking. It
     * simply has no answer for somebody this server has never seen, and saying "this text is not
     * about any player" there was plainly wrong, since it is about the person pinging.
     */
    private fun unanswered(ping: PingRequest?): PlaceholderValue =
        PlaceholderValue.Unavailable(
            when {
                ping == null -> NO_SUBJECT
                else -> NOT_RECOGNISED
            },
        )

    private companion object {
        const val NO_SUBJECT = "this text is not about any player"
        const val STAT_PREFIX = "stat_"

        /**
         * `mined_stone` to `minecraft:mined/minecraft:stone`, and `deaths` to
         * `minecraft:custom/minecraft:deaths`.
         *
         * A key already carrying a namespace is left alone, so anything vanilla can name can still
         * be named in full.
         */
        fun expandStatKey(key: String): String {
            if (':' in key) return key
            val category = StoredStats.MINED.substringAfter(':')
            return if (key.startsWith("${category}_")) {
                StoredStats.mined("minecraft:${key.removePrefix("${category}_")}")
            } else {
                StoredStats.custom(key)
            }
        }

        const val NOT_RECOGNISED =
            "nobody has connected from that address before, so there is no name to greet. " +
                "Put the greeting in a profile with when = [\"%basalt_player_known%\"]"
        const val PERMISSION_PREFIX = "has_permission_"

        /** parity: the compass directions vanilla's F3 screen shows, from the same yaw ranges. */
        fun direction(yaw: Float): String = COMPASS[(((yaw % 360 + 360) % 360) / 22.5f + 0.5f).toInt() % 16]

        fun directionXZ(yaw: Float): String = COMPASS[((((yaw % 360 + 360) % 360) / 90f + 0.5f).toInt() % 4) * 4]

        val COMPASS =
            listOf("S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW", "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE")

        /**
         * Placeholders whose meaning is settled but whose answer needs a system this build has not
         * written yet. Registered so that using one is reported as "not yet" and not as a typo.
         */
        val NOT_IMPLEMENTED =
            buildMap {
                val inventory = "there is no inventory system yet"
                for (
                name in listOf(
                    "item_in_hand",
                    "item_in_hand_name",
                    "item_in_hand_data",
                    "item_in_hand_durability",
                    "item_in_offhand",
                    "item_in_offhand_name",
                    "item_in_offhand_data",
                    "item_in_offhand_durability",
                    "has_empty_slot",
                    "empty_slots",
                    "can_pickup_items",
                )
                ) {
                    put(name, inventory)
                }
                for (piece in listOf("helmet", "chestplate", "leggings", "boots")) {
                    for (suffix in listOf("name", "data", "durability")) {
                        put("armor_${piece}_$suffix", inventory)
                    }
                }
                for (name in listOf("has_health_boost", "health_boost", "absorption", "last_damage")) {
                    put(name, "there are no potion effects or damage tracking yet")
                }
                for (name in listOf("bed_x", "bed_y", "bed_z", "bed_world", "is_sleeping", "sleep_ticks")) {
                    put(name, "beds and respawn points are not implemented yet")
                }
                for (name in listOf("compass_world", "compass_x", "compass_y", "compass_z")) {
                    put(name, "there is no compass target yet")
                }
                for (name in listOf("biome", "biome_capitalized", "block_underneath", "light_level")) {
                    put(name, "the world lookup a live player needs for this is not wired up yet")
                }
                for (
                name in listOf(
                    "locale",
                    "locale_display_name",
                    "locale_short",
                    "locale_country",
                    "locale_display_country",
                )
                ) {
                    put(name, "the client's language is received but not kept on the session yet")
                }
                for (name in listOf("ping", "colored_ping")) {
                    put(name, "round-trip time is not measured yet")
                }
                for (
                name in listOf(
                    "first_join_date",
                    "first_played",
                    "first_join",
                    "first_played_formatted",
                    "last_played",
                    "last_join",
                    "last_played_formatted",
                    "last_join_date",
                    "has_played_before",
                    "minutes_lived",
                    "seconds_lived",
                    "ticks_lived",
                )
                ) {
                    put(name, "the stored row is not kept on the live session yet")
                }
                for (
                name in listOf(
                    "is_whitelisted",
                    "is_banned",
                    "is_sneaking",
                    "is_inside_vehicle",
                    "custom_name",
                    "fly_speed",
                    "walk_speed",
                    "health_scale",
                    "max_air",
                    "remaining_air",
                    "max_no_damage_ticks",
                    "no_damage_ticks",
                    "thunder_duration",
                    "weather_duration",
                    "time",
                    "time_offset",
                    "world_type",
                    "world_time_12",
                    "world_time_24",
                )
                ) {
                    put(name, "not implemented yet")
                }
            }
    }
}
