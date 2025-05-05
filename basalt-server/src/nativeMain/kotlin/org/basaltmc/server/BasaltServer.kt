package org.basaltmc.server

import org.basaltmc.auth.OnlineMode
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpListener
import org.basaltmc.protocol.status.ServerStatusPayload
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor

/**
 * A thread-per-connection server, per `basalt-io`'s current scope (no event loop yet).
 * Handshake, Status (full Server List Ping) and Login (offline-mode, and
 * online-mode when [onlineMode] is supplied) are implemented; nothing past
 * `ServerboundLoginAcknowledgedPacket` (Configuration/Play) exists yet.
 */
public class BasaltServer(
    private val port: Int,
    private val onlineMode: OnlineMode? = null,
) {
    public fun start() {
        val listener = TcpListener.bind(port)
        println("Basalt listening on port $port (${if (onlineMode != null) "online" else "offline"}-mode)")
        while (true) {
            val connection = listener.accept()
            NativeThread.start {
                ClientSession(connection, ::currentStatus, onlineMode = onlineMode).run()
            }
        }
    }

    private fun currentStatus(): ServerStatusPayload =
        ServerStatusPayload(
            description = Component.text("A Basalt Server", TextColor.GRAY),
            players = ServerStatusPayload.Players(max = 20, online = 0),
            version = ServerStatusPayload.Version(BuildInfo.MINECRAFT_VERSION, BuildInfo.PROTOCOL_VERSION),
            favicon = Favicon.bytes,
        )
}
