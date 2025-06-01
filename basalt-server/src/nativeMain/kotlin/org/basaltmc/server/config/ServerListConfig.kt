package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry

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
    private val header: ProfileHeader,
    public val motd: List<String>? = null,
    public val versionName: String? = null,
    public val versionProtocol: Int? = null,
    public val hidePlayerCount: Boolean? = null,
    public val maxPlayers: Int? = null,
    public val onlinePlayers: Int? = null,
    public val hover: List<String>? = null,
    public val icon: String? = null,
) : ConditionalProfile by header {
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
            header: ProfileHeader,
            table: TomlValue.Table,
        ): ServerListProfile =
            ServerListProfile(
                header = header,
                motd = table.linesOrNull("motd"),
                versionName = table.stringOrNull("version-name"),
                versionProtocol = table.intOrNull("version-protocol")?.toInt(),
                hidePlayerCount = table.boolOrNull("hide-player-count"),
                maxPlayers = table.intOrNull("max-players")?.toInt(),
                onlinePlayers = table.intOrNull("online-players")?.toInt(),
                hover = table.stringListOrNull("hover"),
                icon = table.stringOrNull("icon"),
            )
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
    private val reporter = OnceReporter()

    /**
     * The profile this ping gets, by the same rule every UI file uses (see [selectProfile]).
     *
     * A condition that cannot be decided at all is not a no: the profile is skipped and the reason
     * is logged once, because "your condition is nonsense here" and "your condition said no" are
     * different things and only one of them is the operator's mistake.
     */
    public fun select(
        registry: PlaceholderRegistry,
        context: PlaceholderContext = PlaceholderContext.EMPTY,
        logger: Logger = Logger("ServerList"),
    ): ServerListProfile =
        profiles
            .selectProfile(defaultProfile, registry, context, logger, reporter, "server list")
            .inheriting(defaultProfile)

    public companion object {
        public const val FILE_NAME: String = "server_list.toml"

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            legacyMotd: String? = null,
            logger: Logger = Logger("Config"),
        ): ServerListConfig {
            val root = directory.load(FILE_NAME, defaultContents(legacyMotd, logger))
            val path = directory.pathOf(FILE_NAME)
            val (profiles, default) =
                parseProfiles(
                    root,
                    path,
                ) { header, table -> ServerListProfile.from(header, table) }
            val active = profiles.filter { it.isActive }.sortedByDescending { it.priority }
            if (active.isNotEmpty()) {
                logger.info { "Server list profile '${active.first().name}' is switched on in $path." }
            }
            return ServerListConfig(
                profiles = profiles,
                defaultProfile = default,
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
