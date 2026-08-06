package org.basaltmc.server

import org.basaltmc.auth.OfflineUuid
import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpConnection
import org.basaltmc.io.Zlib
import org.basaltmc.protocol.PacketFraming
import org.basaltmc.protocol.handshake.ClientIntent
import org.basaltmc.protocol.handshake.HandshakePacket
import org.basaltmc.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.protocol.login.LoginCompressionPacket
import org.basaltmc.protocol.login.LoginFinishedPacket
import org.basaltmc.protocol.login.LoginStartPacket
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.status.PingRequestPacket
import org.basaltmc.protocol.status.PongResponsePacket
import org.basaltmc.protocol.status.ServerStatusPayload
import org.basaltmc.protocol.status.StatusRequestPacket
import org.basaltmc.protocol.status.StatusResponsePacket
import org.basaltmc.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi

/**
 * Runs the actual [BasaltServer] on a real socket and drives it with `basalt-io`'s own
 * client socket, exercising the whole stack (sockets, framing, compression, packets) together —
 * not just each module's isolated unit tests.
 */
@OptIn(ExperimentalUuidApi::class)
class ServerIntegrationTest {
    private val port = 25566

    private fun startServer() {
        NativeThread.start { BasaltServer(port).start() }
        // Give the listener a moment to bind before the client connects.
        val deadline =
            kotlin.time.TimeSource.Monotonic
                .markNow()
        while (deadline.elapsedNow().inWholeMilliseconds < 500) {
            try {
                TcpConnection.connect("127.0.0.1", port).close()
                return
            } catch (_: Exception) {
                // not up yet, keep trying
            }
        }
    }

    private fun sendUncompressed(
        connection: TcpConnection,
        id: Int,
        writeBody: (ByteBuffer) -> Unit = {},
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeUncompressed(wire, body.toByteArray())
        connection.write(wire.toByteArray())
    }

    private fun sendCompressed(
        connection: TcpConnection,
        id: Int,
        threshold: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeCompressed(wire, body.toByteArray(), threshold, Zlib::compress)
        connection.write(wire.toByteArray())
    }

    private fun readFrame(
        connection: TcpConnection,
        incoming: ByteBuffer,
        compressionEnabled: Boolean,
    ): ByteBuffer {
        val chunk = ByteArray(4096)
        while (true) {
            PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)?.let { frame ->
                incoming.discardReadBytes()
                return ByteBuffer(frame.size).also { it.writeBytes(frame) }
            }
            val n = connection.read(chunk)
            check(n > 0) { "connection closed while waiting for a frame" }
            incoming.writeBytes(chunk, length = n)
        }
    }

    @Test
    fun statusFlowOverRealSockets() {
        startServer()
        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()

        sendUncompressed(connection, HandshakePacket.ID) {
            HandshakePacket(776, "127.0.0.1", port, ClientIntent.STATUS).write(it)
        }
        sendUncompressed(connection, StatusRequestPacket.ID)

        val statusBody = readFrame(connection, incoming, compressionEnabled = false)
        assertEquals(StatusResponsePacket.ID, statusBody.readVarInt())
        val status = StatusResponsePacket.read(statusBody).status
        assertEquals(
            ServerStatusPayload.Version(BuildInfo.MINECRAFT_VERSION, BuildInfo.PROTOCOL_VERSION),
            status.version,
        )

        sendUncompressed(connection, PingRequestPacket.ID) { PingRequestPacket(987654321L).write(it) }
        val pongBody = readFrame(connection, incoming, compressionEnabled = false)
        assertEquals(PongResponsePacket.ID, pongBody.readVarInt())
        assertEquals(987654321L, PongResponsePacket.read(pongBody).time)

        connection.close()
    }

    @Test
    fun offlineLoginFlowOverRealSockets() {
        startServer()
        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()

        sendUncompressed(connection, HandshakePacket.ID) {
            HandshakePacket(776, "127.0.0.1", port, ClientIntent.LOGIN).write(it)
        }
        sendUncompressed(connection, LoginStartPacket.ID) {
            LoginStartPacket("Alex", kotlin.uuid.Uuid.fromLongs(0L, 0L)).write(it)
        }

        val compressionBody = readFrame(connection, incoming, compressionEnabled = false)
        assertEquals(LoginCompressionPacket.ID, compressionBody.readVarInt())
        val threshold = LoginCompressionPacket.read(compressionBody).threshold
        assertTrue(threshold > 0)

        val finishedBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(LoginFinishedPacket.ID, finishedBody.readVarInt())
        val finished = LoginFinishedPacket.read(finishedBody)
        assertEquals("Alex", finished.gameProfile.name)
        assertEquals(OfflineUuid.forUsername("Alex"), finished.gameProfile.id)

        sendCompressed(connection, LoginAcknowledgedPacket.ID, threshold) { }
        connection.close()
    }
}
