package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.ConditionResult
import org.basaltmc.common.placeholder.PlaceholderCondition
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import kotlin.concurrent.AtomicReference

internal const val SERVER_LIST_DEFAULT_TOML = """# How the server looks in the multiplayer list.
#
# All of this is decided before anyone logs in, so the only things known about
# whoever is looking are the three the ping carries: the protocol version of
# their client, the address they typed to reach you, and where they connected
# from. Every condition below is one of those three.
#
# A profile is one complete look. The one that wins is the highest priority
# among the profiles that either are switched on with `active` or have a [when]
# that matches. If none does, the one marked `default` is used. Anything a
# profile leaves out it takes from the default profile, so a profile that only
# changes the text is three lines long.
#
# Text is MiniMessage. Writing the MOTD as a list gives you the two lines the
# client shows; a single string with \n does the same thing.
#
# Anything %like_this% is a placeholder, filled in before the text is parsed and
# usable in every file that takes text. A placeholder this server cannot fill in
# is left visible and explained once in the log, so a wrong name shows up rather
# than silently emptying the line. The four that matter here:
#   %basalt_server_online%       players connected right now
#   %basalt_server_max_players%  the slot count shown in the list
#   %basalt_server_version%      the Minecraft version this server implements
#   %basalt_player_name%         who is pinging, if this server knows them; see
#                                [greeting] at the bottom, off by default.
#
# `when` is a list of conditions over those same placeholders, all of which have
# to hold. Operators: = != < <= > >= matches contains, and a condition on its own
# is true when it reads `true`. A condition this situation cannot answer, such as
# a player's coordinates before anyone has logged in, is reported once in the log
# and the profile is skipped: it never counts as a silent no.

[profiles.default]
default = true
motd = [
    "              <bold><gradient:gray:white>BASALT SERVER</gradient></bold> <gray>[1.8-26.2]</gray>",
    "              <white>Welcome to your new server</white>",
]

[profiles.legacy]
# Gradients are RGB text, and RGB text arrived in 1.16: an older client renders
# the profile above as flat white and loses the effect entirely. Same layout,
# built out of the sixteen colours those clients do have.
motd = [
    "              <bold><gray>BASALT SERVER</gray></bold> <dark_gray>[1.8-26.2]</dark_gray>",
    "              <white>Welcome to your new server</white>",
]

# 735 is 1.16, the first version with RGB text.
when = ["%basalt_player_protocol% < 735"]

[profiles.maintenance]
# Not conditional: switch it on, and it is what everyone sees until you switch
# it off. That is what `active` is for, as opposed to [when].
active = false
priority = 100
motd = [
    "              <bold><gray>BASALT SERVER</gray></bold> <dark_gray>[1.8-26.2]</dark_gray>",
    "           <red>The server is under maintenance</red>",
]
# Replaces the player count with ??? rather than showing 0 of 80.
hide-player-count = true
# Purely how the list draws it: the client shows a version it does not recognise
# in red, and this stops nobody from joining. Rejecting people is a job for the
# whitelist, not for the server list.
version-name = "Maintenance"
version-protocol = -1

# Other keys a profile takes:
#   priority = 0              higher wins between profiles that both apply
#   max-players = 80          what the list shows, not how many can actually join
#   online-players = 0        likewise, for the left-hand number
#   hover = ["a", "b"]        the lines shown when hovering the player count
#   icon = "server-icon.png"  64x64 PNG in config/, replacing the built-in one
#
# And what a `when` tends to look like:
#   when = ["%basalt_player_host% matches beta.*"]      per domain
#   when = ["%basalt_player_ip% matches 192.168.*"]     per network
#   when = ["%basalt_player_protocol% = 776"]           one exact version
#   when = ["%basalt_player_known%"]                    somebody seen before
#   when = ["%basalt_server_online% >= 20", "%basalt_player_protocol% >= 735"]

[greeting]
# Remembering the address a player last connected from is what lets <player> and
# `known-player` work: the ping happens before login, so the address is the only
# thing there is to recognise someone by.
#
# It is off by default because it is personal data that vanilla never stores, and
# because it is a poor identifier anyway: everyone behind one router shares an
# address, and dynamic addresses change hands. Fine for saying hello, never for
# deciding what someone may do, which is why permissions cannot be driven by it.
#
# The address is one column in players.db, overwritten on each login, and turning
# this off stops it being written but does not erase what is already there.
remember-ip = false
"""

/**
 * What the server knows about whoever is pinging. All of it comes from the handshake or the socket.
 *
 * [knownPlayer] is resolved lazily and only once, because working it out means a database read and
 * a ping is a request anyone on the internet can send as fast as they like. A file that never asks
 * who is pinging never pays for the answer, which is why nothing has to scan the profiles up front
 * to decide whether the lookup is worth doing.
 */
public class PingRequest(
    public val protocolVersion: Int,
    /** The address the client was pointed at, which is the handshake's virtual host. */
    public val host: String,
    public val address: String?,
    lookupKnownPlayer: () -> String? = { null },
) {
    public val knownPlayer: String? by lazy(lookupKnownPlayer)

    public constructor(
        protocolVersion: Int,
        host: String,
        address: String?,
        knownPlayer: String?,
    ) : this(protocolVersion, host, address, { knownPlayer })
}

/**
 * One complete look for the server list.
 *
 * Every field except [name] is nullable because a profile is a set of overrides on the default one:
 * a maintenance profile that only changes the text should not have to restate the icon, the version
 * and the player count to avoid silently resetting them.
 */
public data class ServerListProfile(
    public val name: String,
    public val priority: Int = 0,
    public val isDefault: Boolean = false,
    public val isActive: Boolean = false,
    public val conditions: List<PlaceholderCondition> = emptyList(),
    public val motd: List<String>? = null,
    public val versionName: String? = null,
    public val versionProtocol: Int? = null,
    public val hidePlayerCount: Boolean? = null,
    public val maxPlayers: Int? = null,
    public val onlinePlayers: Int? = null,
    public val hover: List<String>? = null,
    public val icon: String? = null,
) {
    /** This profile's own values, with [fallback]'s used wherever it declared nothing. */
    public fun inheriting(fallback: ServerListProfile): ServerListProfile =
        if (fallback === this) {
            this
        } else {
            copy(
                motd = motd ?: fallback.motd,
                versionName = versionName ?: fallback.versionName,
                versionProtocol = versionProtocol ?: fallback.versionProtocol,
                hidePlayerCount = hidePlayerCount ?: fallback.hidePlayerCount,
                maxPlayers = maxPlayers ?: fallback.maxPlayers,
                onlinePlayers = onlinePlayers ?: fallback.onlinePlayers,
                hover = hover ?: fallback.hover,
                icon = icon ?: fallback.icon,
            )
        }

    internal companion object {
        fun from(
            name: String,
            table: TomlValue.Table,
        ): ServerListProfile =
            ServerListProfile(
                name = name,
                priority = table.intOrNull("priority")?.toInt() ?: 0,
                isDefault = table.boolOrNull("default") ?: false,
                isActive = table.boolOrNull("active") ?: false,
                conditions = table.stringListOrNull("when").orEmpty().map(::PlaceholderCondition),
                motd = table.linesOrNull("motd"),
                versionName = table.stringOrNull("version-name"),
                versionProtocol = table.intOrNull("version-protocol")?.toInt(),
                hidePlayerCount = table.boolOrNull("hide-player-count"),
                maxPlayers = table.intOrNull("max-players")?.toInt(),
                onlinePlayers = table.intOrNull("online-players")?.toInt(),
                hover = table.stringListOrNull("hover"),
                icon = table.stringOrNull("icon"),
            )

        /** A list of lines, or a single string split on its newlines: both spell the same MOTD. */
        private fun TomlValue.Table.linesOrNull(key: String): List<String>? =
            stringListOrNull(key) ?: stringOrNull(key)?.split("\n")
    }
}

/**
 * `config/server_list.toml`: the profiles, and which one a given ping gets.
 *
 * Selection is deliberately not first-match: profiles are written in whatever order reads well, and
 * ordering rules that depend on that are the reason permission files are hard to reason about. The
 * rule is highest priority wins, ties broken by declaration order, and a tie at the same priority
 * is a sign the file wanted a priority rather than a coin toss.
 *
 * A profile with no `when` and `active = false` is not a mistake and is not reported as one: it is
 * how a profile that the operator switches on by hand, such as the maintenance one, is written.
 */
public class ServerListConfig(
    public val profiles: List<ServerListProfile>,
    public val defaultProfile: ServerListProfile,
    public val rememberIp: Boolean,
) {
    // Conditions are evaluated per ping, so a broken one would otherwise complain per ping. Held as
    // an immutable set swapped by compare-and-set because pings arrive on connection threads, and a
    // plain HashSet mutated from several of them is a data race, not just a duplicated warning.
    private val reported = AtomicReference<Set<String>>(emptySet())

    /**
     * The profile this ping gets.
     *
     * A profile applies when it is switched on or when every one of its conditions holds. A
     * condition that cannot be decided at all is not a no: the profile is skipped, and the reason is
     * logged once, because "your condition is nonsense here" and "your condition said no" are
     * different things and only one of them is the operator's mistake.
     */
    public fun select(
        registry: PlaceholderRegistry,
        context: PlaceholderContext = PlaceholderContext.EMPTY,
        logger: Logger = Logger("ServerList"),
    ): ServerListProfile {
        val winner =
            profiles
                .filter { it.isActive || applies(it, registry, context, logger) }
                .maxByOrNull { it.priority }
        return (winner ?: defaultProfile).inheriting(defaultProfile)
    }

    private fun applies(
        profile: ServerListProfile,
        registry: PlaceholderRegistry,
        context: PlaceholderContext,
        logger: Logger,
    ): Boolean {
        if (profile.conditions.isEmpty()) return false
        for (condition in profile.conditions) {
            when (val result = condition.evaluate(registry, context)) {
                ConditionResult.True -> continue
                ConditionResult.False -> return false
                is ConditionResult.Unresolvable -> {
                    reportOnce(logger) {
                        "Condition '${condition.raw}' of server list profile '${profile.name}' " +
                            "was skipped: ${result.reason}."
                    }
                    return false
                }
            }
        }
        return true
    }

    private fun reportOnce(
        logger: Logger,
        message: () -> String,
    ) {
        val text = message()
        while (true) {
            val current = reported.value
            if (text in current) return
            if (reported.compareAndSet(current, current + text)) {
                logger.warn { text }
                return
            }
        }
    }

    public companion object {
        public const val FILE_NAME: String = "server_list.toml"

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            legacyMotd: String? = null,
            logger: Logger = Logger("Config"),
        ): ServerListConfig {
            val root = directory.load(FILE_NAME, defaultContents(legacyMotd, logger))
            val path = directory.pathOf(FILE_NAME)
            val declared =
                root.tableOrNull("profiles")
                    ?: throw TomlFormatException("$path declares no [profiles] table.")

            val profiles =
                declared.keys().mapNotNull { name ->
                    declared.tableOrNull(name)?.let { ServerListProfile.from(name, it) }
                }
            if (profiles.isEmpty()) throw TomlFormatException("$path declares no profiles.")

            val defaults = profiles.filter { it.isDefault }
            if (defaults.size != 1) {
                throw TomlFormatException("$path must mark exactly one profile as `default`, found ${defaults.size}.")
            }
            val active = profiles.filter { it.isActive }.sortedByDescending { it.priority }
            if (active.isNotEmpty()) {
                logger.info { "Server list profile '${active.first().name}' is switched on in $path." }
            }
            return ServerListConfig(
                profiles = profiles,
                defaultProfile = defaults.single(),
                rememberIp = root.tableOrNull("greeting")?.boolOrNull("remember-ip") ?: false,
            )
        }
    }
}

/**
 * The file to generate on first run: the shipped one, or the shipped one with a vanilla
 * `server.properties` MOTD in place of both greetings.
 *
 * Both, and not just the modern one, because the point of the import is that somebody arriving from
 * vanilla sees the message they already had. Leaving Basalt's own text on the older-client profile
 * would mean the same server greeted two people differently for no reason they chose.
 */
internal fun defaultContents(
    legacyMotd: String?,
    logger: Logger,
): String {
    if (legacyMotd.isNullOrBlank()) return SERVER_LIST_DEFAULT_TOML
    if ('§' in legacyMotd) {
        logger.warn {
            "The MOTD imported from server.properties uses section-sign colour codes, which are " +
                "not translated: rewrite it in MiniMessage in ${ServerListConfig.FILE_NAME}."
        }
    }
    val lines =
        legacyMotd
            .replace("\\n", "\n")
            .split("\n")
            .joinToString(",\n", prefix = "[\n", postfix = ",\n]") { "    \"${escapeToml(it)}\"" }
    return SERVER_LIST_DEFAULT_TOML
        .replace(MODERN_MOTD_BLOCK, "motd = $lines")
        .replace(LEGACY_MOTD_BLOCK, "motd = $lines")
}

private fun escapeToml(text: String): String = text.replace("\\", "\\\\").replace("\"", "\\\"")

private const val MODERN_MOTD_BLOCK = """motd = [
    "              <bold><gradient:gray:white>BASALT SERVER</gradient></bold> <gray>[1.8-26.2]</gray>",
    "              <white>Welcome to your new server</white>",
]"""

private const val LEGACY_MOTD_BLOCK = """motd = [
    "              <bold><gray>BASALT SERVER</gray></bold> <dark_gray>[1.8-26.2]</dark_gray>",
    "              <white>Welcome to your new server</white>",
]"""
