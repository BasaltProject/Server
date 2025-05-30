package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

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
# Placeholders, replaced before the text is parsed:
#   <online>   players connected right now
#   <max>      the slot count shown in the list
#   <version>  the Minecraft version this server implements
#   <player>   the name of whoever is pinging, if this server knows them; see
#              [greeting] at the bottom, which is off by default.

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

[profiles.legacy.when]
# 735 is 1.16, the first version with RGB text.
protocol-below = 735

[profiles.maintenance]
# Not conditional: switch it on, and it is what everyone sees until you switch
# it off. That is what `active` is for, as opposed to [when].
active = false
priority = 100
motd = [
    "              <bold><gray>BASALT SERVER</gray></bold> <red>[maintenance]</red>",
    "              <white>Back shortly</white>",
]
# Replaces the player count with ??? rather than showing 0 of 20.
hide-player-count = true
# Purely how the list draws it: the client shows a version it does not recognise
# in red, and this stops nobody from joining. Rejecting people is a job for the
# whitelist, not for the server list.
version-name = "Maintenance"
version-protocol = -1

# Other keys a profile takes:
#   priority = 0              higher wins between profiles that both apply
#   max-players = 20          what the list shows, not how many can actually join
#   online-players = 0        likewise, for the left-hand number
#   hover = ["a", "b"]        the lines shown when hovering the player count
#   icon = "server-icon.png"  64x64 PNG in config/, replacing the built-in one
#
# And the conditions a [when] takes, all of which have to match at once:
#   host = "mc.example.com"   the address they connected to, `*` allowed
#   ip = "192.168.*"          where they connected from
#   protocol = 776            exactly this protocol version
#   protocol-below = 735      older than this
#   protocol-above = 776      newer than this
#   known-player = true       this server has seen that address before

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

/** What the server knows about whoever is pinging. All of it comes from the handshake or the socket. */
public data class PingRequest(
    public val protocolVersion: Int,
    /** The address the client was pointed at, which is the handshake's virtual host. */
    public val host: String,
    public val address: String?,
    /** The name last seen at [address], or null when nobody is remembered or the greeting is off. */
    public val knownPlayer: String? = null,
)

/**
 * When a profile applies. Every declared condition has to hold, and a condition nobody declared is
 * not a condition: an empty [PingCondition] never matches anything, since a profile that applies
 * unconditionally is what `default` and `active` are for.
 */
public data class PingCondition(
    public val host: String? = null,
    public val ip: String? = null,
    public val protocol: Int? = null,
    public val protocolBelow: Int? = null,
    public val protocolAbove: Int? = null,
    public val knownPlayer: Boolean? = null,
) {
    public val isEmpty: Boolean
        get() =
            host == null &&
                ip == null &&
                protocol == null &&
                protocolBelow == null &&
                protocolAbove == null &&
                knownPlayer == null

    public fun matches(request: PingRequest): Boolean {
        if (isEmpty) return false
        if (host != null && !globMatches(host, request.host)) return false
        if (ip != null && (request.address == null || !globMatches(ip, request.address))) return false
        if (protocol != null && request.protocolVersion != protocol) return false
        if (protocolBelow != null && request.protocolVersion >= protocolBelow) return false
        if (protocolAbove != null && request.protocolVersion <= protocolAbove) return false
        if (knownPlayer != null && (request.knownPlayer != null) != knownPlayer) return false
        return true
    }

    public companion object {
        public fun from(table: TomlValue.Table?): PingCondition =
            PingCondition(
                host = table?.stringOrNull("host"),
                ip = table?.stringOrNull("ip"),
                protocol = table?.intOrNull("protocol")?.toInt(),
                protocolBelow = table?.intOrNull("protocol-below")?.toInt(),
                protocolAbove = table?.intOrNull("protocol-above")?.toInt(),
                knownPlayer = table?.boolOrNull("known-player"),
            )
    }
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
    public val condition: PingCondition = PingCondition(),
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
                condition = PingCondition.from(table.tableOrNull("when")),
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
 * A profile with no `[when]` and `active = false` is not a mistake and is not reported as one: it is
 * how a profile that the operator switches on by hand, such as the maintenance one, is written.
 */
public class ServerListConfig(
    public val profiles: List<ServerListProfile>,
    public val defaultProfile: ServerListProfile,
    public val rememberIp: Boolean,
) {
    /**
     * Whether anything in this file actually looks a player up by address.
     *
     * Checked once here so that a server whose profiles never mention it does not pay a database
     * query per ping, which is a request anyone on the internet can send as fast as they like.
     */
    public val usesPlayerLookup: Boolean =
        rememberIp &&
            profiles.any { profile ->
                profile.condition.knownPlayer != null || profile.motd?.any { PLAYER_PLACEHOLDER in it } == true
            }

    public fun select(request: PingRequest): ServerListProfile {
        val winner =
            profiles
                .filter { it.isActive || it.condition.matches(request) }
                .maxByOrNull { it.priority }
        return (winner ?: defaultProfile).inheriting(defaultProfile)
    }

    public companion object {
        public const val FILE_NAME: String = "server_list.toml"
        public const val PLAYER_PLACEHOLDER: String = "<player>"

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

/** Case-insensitive glob with `*` standing for any run of characters, which is all these fields need. */
internal fun globMatches(
    pattern: String,
    value: String,
): Boolean {
    val parts = pattern.lowercase().split("*")
    val text = value.lowercase()
    if (parts.size == 1) return text == parts[0]

    if (!text.startsWith(parts.first())) return false
    if (!text.endsWith(parts.last())) return false
    var index = parts.first().length
    for (part in parts.subList(1, parts.size - 1)) {
        if (part.isEmpty()) continue
        val found = text.indexOf(part, index)
        if (found < 0) return false
        index = found + part.length
    }
    // The trailing literal has to fit after everything already matched, or "a*bc" would accept "abc"
    // twice over by counting the same characters as both.
    return index <= text.length - parts.last().length
}
