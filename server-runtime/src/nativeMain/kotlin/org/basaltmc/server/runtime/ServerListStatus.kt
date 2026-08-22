package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.config.PingRequest
import org.basaltmc.server.runtime.config.ServerListConfig
import org.basaltmc.server.runtime.config.ServerListProfile
import org.basaltmc.server.runtime.placeholder.PlaceholderSlots
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.common.placeholder.PlaceholderText
import org.basaltmc.wire.common.placeholder.PlaceholderValue
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.protocol.status.ServerStatusPayload
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.MiniMessage
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.concurrent.AtomicReference

/** One player, as the list's hover text names them. */
public data class ListedPlayer(
    public val name: String,
    public val uuid: String,
)

/**
 * Turns a ping into the status the client draws, by picking a profile out of `server_list.toml`
 * and rendering it. [ServerListConfig] knows what the operator wrote and which profile applies;
 * this knows how to draw it, and neither resolves placeholders, which is what lets the same text
 * work in a chat format too.
 *
 * This runs on a connection thread, before login, for a request anyone can send as fast as they
 * like: icons are cached, and the database is only touched if the file asked something that needs
 * it.
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
    /** Where plugin events go, read late: the plugin host is built after this. */
    private val pluginEvents: () -> org.basaltmc.server.plugins.PluginEventBus? = { null },
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
        var max = profile.maxPlayers ?: slots
        var motd = render(profile.motd.orEmpty(), context)

        // The last word on what a pinger sees, after the profile decided it: a plugin can change
        // the line and the slots without the operator's file having to know about it.
        val ping =
            pluginEvents().emit(PluginEventTypes.SERVER_PING) { event ->
                event.set(0, address)
                event.set(1, PlainTextSerializer.serialize(motd))
                event.set(2, online)
                event.set(3, max)
            }
        if (ping != null) {
            (ping.get("motd") as? String)?.let { motd = MiniMessage.parse(it) }
            (ping.get("max_players") as? Long)?.let { max = it.toInt() }
        }

        return ServerStatusPayload(
            description = motd,
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
     * The two failures differ because the MOTD is a line strangers read. An unregistered name is
     * the operator's typo: it stays visible and is warned about. A registered name this ping
     * cannot answer, like `%basalt_player_name%` for someone who never connected, is ordinary: it
     * resolves to nothing and only reaches the debug log.
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
