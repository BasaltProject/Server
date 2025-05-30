package org.basaltmc.server

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import org.basaltmc.protocol.status.ServerStatusPayload
import org.basaltmc.server.config.PingRequest
import org.basaltmc.server.config.ServerListConfig
import org.basaltmc.server.config.ServerListProfile
import org.basaltmc.text.Component
import org.basaltmc.text.MiniMessage

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
 * the operator wrote and which profile applies, and this knows what the server currently is (who is
 * online, which version it speaks, what icon is on disk). Only the second half needs a running
 * server, which is what makes profile selection testable without one.
 *
 * Everything here runs on a connection thread, before login, for a request anyone on the internet
 * can send as fast as they like. So: icons are read once and cached, and the database is only asked
 * about an address when a profile actually mentions one.
 */
public class ServerListStatus(
    private val config: ServerListConfig,
    private val slots: Int,
    private val versionName: String,
    private val protocolVersion: Int,
    private val onlinePlayers: () -> List<ListedPlayer>,
    private val configRoot: String = "config",
    private val lookupPlayerByAddress: ((String) -> String?)? = null,
    private val logger: Logger = Logger("ServerList"),
) {
    private val icons = HashMap<String, ByteArray?>()

    public fun statusFor(
        protocolVersion: Int,
        host: String,
        address: String?,
    ): ServerStatusPayload {
        val known = if (config.usesPlayerLookup && address != null) lookupPlayerByAddress?.invoke(address) else null
        val request = PingRequest(protocolVersion, host, address, known)
        val profile = config.select(request)
        val players = onlinePlayers()

        val online = profile.onlinePlayers ?: players.size
        val max = profile.maxPlayers ?: slots
        val values =
            mapOf(
                "<online>" to online.toString(),
                "<max>" to max.toString(),
                "<version>" to versionName,
                ServerListConfig.PLAYER_PLACEHOLDER to (known ?: "player"),
            )

        return ServerStatusPayload(
            description = render(profile.motd.orEmpty(), values),
            players =
                if (profile.hidePlayerCount == true) {
                    null
                } else {
                    ServerStatusPayload.Players(
                        max = max,
                        online = online,
                        sample = sampleFor(profile, players, values),
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
        values: Map<String, String>,
    ): List<ServerStatusPayload.SamplePlayer> {
        val hover = profile.hover
        if (hover != null) {
            return hover.map { ServerStatusPayload.SamplePlayer(substitute(it, values), NIL_UUID) }
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
        return icons.getOrPut(name) {
            val path = "$configRoot/$name"
            val bytes = FileIO.readBytes(path)
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
        } ?: Favicon.bytes
    }

    private fun render(
        lines: List<String>,
        values: Map<String, String>,
    ): Component {
        if (lines.isEmpty()) return Component.empty()
        var result = MiniMessage.parse(substitute(lines.first(), values))
        for (line in lines.drop(1)) {
            result = result.append(Component.text("\n")).append(MiniMessage.parse(substitute(line, values)))
        }
        return result
    }

    /** Substituted before MiniMessage parses, so a placeholder is never mistaken for a tag. */
    private fun substitute(
        text: String,
        values: Map<String, String>,
    ): String {
        var result = text
        for ((placeholder, value) in values) {
            if (placeholder in result) result = result.replace(placeholder, value)
        }
        return result
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
