package org.basaltmc.server

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.placeholder.PlaceholderText
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.io.FileIO
import org.basaltmc.protocol.status.ServerStatusPayload
import org.basaltmc.server.config.PingRequest
import org.basaltmc.server.config.ServerListConfig
import org.basaltmc.server.config.ServerListProfile
import org.basaltmc.server.placeholder.PlaceholderSlots
import org.basaltmc.text.Component
import org.basaltmc.text.MiniMessage
import kotlin.concurrent.AtomicReference

/** One player, as the list's hover text names them. */
public data class ListedPlayer(
    public val name: String,
    public val uuid: String,
)

/**
 * Turns a ping into the status the client draws, by picking a profile out of `server_list.toml` and
 * rendering it.
 *
 * Kept apart from [ServerListConfig] because they answer different questions: the config knows what
 * the operator wrote and which profile applies, and this knows how to draw it. Neither knows what a
 * `%basalt_player_protocol%` is worth, which is the placeholder registry's job, and that is what
 * lets the same text work here, in a chat format, or anywhere else text is written by an operator.
 *
 * Everything here runs on a connection thread, before login, for a request anyone on the internet
 * can send as fast as they like. So: icons are read once and cached, and a database lookup only
 * happens if the file asked a question that needs one.
 */
public class ServerListStatus(
    private val config: () -> ServerListConfig,
    private val placeholders: PlaceholderRegistry,
    private val slots: Int,
    private val versionName: String,
    private val protocolVersion: Int,
    private val onlinePlayers: () -> List<ListedPlayer>,
    private val configRoot: String = "config",
    private val lookupPlayerByAddress: ((String) -> String?)? = null,
    private val logger: Logger = Logger("ServerList"),
) {
    private val icons = AtomicReference<Map<String, ByteArray?>>(emptyMap())
    private val reported = AtomicReference<Set<String>>(emptySet())

    public fun statusFor(
        protocolVersion: Int,
        host: String,
        address: String?,
    ): ServerStatusPayload {
        val request =
            PingRequest(protocolVersion, host, address) {
                if (config().rememberIp && address != null) lookupPlayerByAddress?.invoke(address) else null
            }
        val context = PlaceholderContext.EMPTY.with(PlaceholderSlots.PING, request)
        val current = config()
        val profile = current.select(placeholders, context, logger)
        val players = onlinePlayers()

        val online = profile.onlinePlayers ?: players.size
        val max = profile.maxPlayers ?: slots

        return ServerStatusPayload(
            description = render(profile.motd.orEmpty(), context),
            players =
                if (profile.hidePlayerCount == true) {
                    null
                } else {
                    ServerStatusPayload.Players(
                        max = max,
                        online = online,
                        sample = sampleFor(profile, players, context),
                    )
                },
            version =
                ServerStatusPayload.Version(
                    name = profile.versionName ?: versionName,
                    protocol = profile.versionProtocol ?: this.protocolVersion,
                ),
            favicon = iconFor(profile),
        )
    }

    /**
     * The hover text under the player count: the operator's own lines if a profile wrote any,
     * otherwise the players actually online, which is what vanilla puts there.
     *
     * Custom lines are sent with a nil UUID because the field is not optional in the packet and a
     * made-up one would be a real player's identifier somewhere.
     */
    private fun sampleFor(
        profile: ServerListProfile,
        players: List<ListedPlayer>,
        context: PlaceholderContext,
    ): List<ServerStatusPayload.SamplePlayer> {
        val hover = profile.hover
        if (hover != null) {
            return hover.map { ServerStatusPayload.SamplePlayer(substitute(it, context), NIL_UUID) }
        }
        return players.take(SAMPLE_LIMIT).map { ServerStatusPayload.SamplePlayer(it.name, it.uuid) }
    }

    /**
     * Reads a profile's icon once and remembers the answer, including a failure.
     *
     * Remembering the failure matters as much as remembering the file: a missing icon must not turn
     * into a failed `open()` on every ping, and it must not log the same complaint forever either.
     */
    private fun iconFor(profile: ServerListProfile): ByteArray? {
        val name = profile.icon ?: return Favicon.bytes
        icons.value[name]?.let { return it }
        if (name in icons.value) return Favicon.bytes

        val path = "$configRoot/$name"
        val bytes = FileIO.readBytes(path)
        val usable =
            when {
                bytes == null -> {
                    logger.warn { "Server list icon $path not found; using the built-in one." }
                    null
                }
                !isPng64(bytes) -> {
                    logger.warn { "Server list icon $path is not a 64x64 PNG; using the built-in one." }
                    null
                }
                else -> bytes
            }
        while (true) {
            val current = icons.value
            if (icons.compareAndSet(current, current + (name to usable))) break
        }
        return usable ?: Favicon.bytes
    }

    private fun render(
        lines: List<String>,
        context: PlaceholderContext,
    ): Component {
        if (lines.isEmpty()) return Component.empty()
        var result = MiniMessage.parse(substitute(lines.first(), context))
        for (line in lines.drop(1)) {
            result = result.append(Component.text("\n")).append(MiniMessage.parse(substitute(line, context)))
        }
        return result
    }

    /**
     * Placeholders are filled in before MiniMessage parses, so `%...%` and `<...>` never have to
     * agree about anything.
     *
     * The two failures are treated differently on purpose, and the difference is what the MOTD is:
     * a line strangers read.
     *
     * A name nobody registered is the operator's mistake. It stays visible and is warned about, so
     * that a typo is found rather than silently eating a word.
     *
     * A real name this ping cannot answer is *ordinary*. `%basalt_player_name%` is unanswerable for
     * everyone who has never connected before, which is most of the people who will ever ping. It
     * resolves to nothing and is only noted in the debug log: showing `%basalt_player_name%` to a
     * stranger, or warning once per server about something working as designed, are both wrong.
     */
    private fun substitute(
        text: String,
        context: PlaceholderContext,
    ): String =
        PlaceholderText.substitute(text, placeholders, context) { name, value ->
            if (value is PlaceholderValue.Unknown) {
                reportOnce("The server list uses %$name%, which is not a placeholder this server knows.")
                null
            } else {
                val reason = (value as? PlaceholderValue.Unavailable)?.reason ?: "no reason given"
                logger.debug { "Server list placeholder %$name% is empty here: $reason." }
                ""
            }
        }

    private fun reportOnce(message: String) {
        while (true) {
            val current = reported.value
            if (message in current) return
            if (reported.compareAndSet(current, current + message)) {
                logger.warn { message }
                return
            }
        }
    }

    /** The PNG signature plus the width and height out of the IHDR chunk, which is always first. */
    private fun isPng64(bytes: ByteArray): Boolean {
        if (bytes.size < 24) return false
        val signature = byteArrayOf(-119, 80, 78, 71, 13, 10, 26, 10)
        for (index in signature.indices) if (bytes[index] != signature[index]) return false
        return readInt(bytes, 16) == ICON_SIZE && readInt(bytes, 20) == ICON_SIZE
    }

    private fun readInt(
        bytes: ByteArray,
        offset: Int,
    ): Int =
        ((bytes[offset].toInt() and 0xFF) shl 24) or
            ((bytes[offset + 1].toInt() and 0xFF) shl 16) or
            ((bytes[offset + 2].toInt() and 0xFF) shl 8) or
            (bytes[offset + 3].toInt() and 0xFF)

    private companion object {
        const val ICON_SIZE = 64
        const val SAMPLE_LIMIT = 12
        const val NIL_UUID = "00000000-0000-0000-0000-000000000000"
    }
}
