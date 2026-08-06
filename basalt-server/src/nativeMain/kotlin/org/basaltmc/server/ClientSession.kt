package org.basaltmc.server

import org.basaltmc.auth.OfflineUuid
import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.TcpConnection
import org.basaltmc.io.Zlib
import org.basaltmc.protocol.PacketFraming
import org.basaltmc.protocol.ProtocolState
import org.basaltmc.protocol.handshake.ClientIntent
import org.basaltmc.protocol.handshake.HandshakePacket
import org.basaltmc.protocol.login.GameProfile
import org.basaltmc.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.protocol.login.LoginCompressionPacket
import org.basaltmc.protocol.login.LoginFinishedPacket
import org.basaltmc.protocol.login.LoginStartPacket
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.status.PingRequestPacket
import org.basaltmc.protocol.status.PongResponsePacket
import org.basaltmc.protocol.status.StatusRequestPacket
import org.basaltmc.protocol.status.StatusResponsePacket
import org.basaltmc.protocol.writeVarInt
import kotlin.uuid.ExperimentalUuidApi

/**
 * Drives one client connection through Handshake and (Status | Login). Offline-mode only: there
 * is no `ClientboundHelloPacket`/`ServerboundKeyPacket` exchange, so a real vanilla client must
 * connect to a server with `online-mode=false` to reach [LoginFinishedPacket] here: RSA
 * encryption is deferred. After [LoginAcknowledgedPacket] the
 * connection would move to CONFIGURATION, which is not implemented yet; the session ends there.
 */
public class ClientSession(
    private val connection: TcpConnection,
    private val statusProvider: () -> org.basaltmc.protocol.status.ServerStatusPayload,
    private val compressionThreshold: Int = 256,
) {
    private val incoming = ByteBuffer()
    private var state = ProtocolState.HANDSHAKE
    private var compressionEnabled = false

    public fun run() {
        val readChunk = ByteArray(4096)
        try {
            while (true) {
                val read = connection.read(readChunk)
                if (read <= 0) return
                incoming.writeBytes(readChunk, length = read)
                drainFrames()
            }
        } finally {
            connection.close()
        }
    }

    private fun drainFrames() {
        while (true) {
            val frame =
                PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)
                    ?: break
            incoming.discardReadBytes()
            handleFrame(ByteBuffer(frame.size).also { it.writeBytes(frame) })
        }
    }

    private fun handleFrame(body: ByteBuffer) {
        val packetId = body.readVarInt()
        when (state) {
            ProtocolState.HANDSHAKE -> handleHandshake(packetId, body)
            ProtocolState.STATUS -> handleStatus(packetId, body)
            ProtocolState.LOGIN -> handleLogin(packetId, body)
            else -> Unit
        }
    }

    private fun handleHandshake(
        packetId: Int,
        body: ByteBuffer,
    ) {
        if (packetId != HandshakePacket.ID) return
        val packet = HandshakePacket.read(body)
        state =
            when (packet.intent) {
                ClientIntent.STATUS -> ProtocolState.STATUS
                ClientIntent.LOGIN -> ProtocolState.LOGIN
                ClientIntent.TRANSFER -> return // not supported yet; drop the connection
            }
    }

    private fun handleStatus(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (packetId) {
            StatusRequestPacket.ID ->
                sendUncompressed(
                    StatusResponsePacket.ID,
                ) { StatusResponsePacket(statusProvider()).write(it) }
            PingRequestPacket.ID -> {
                val ping = PingRequestPacket.read(body)
                sendUncompressed(PongResponsePacket.ID) { PongResponsePacket(ping.time).write(it) }
            }
        }
    }

    @OptIn(ExperimentalUuidApi::class)
    private fun handleLogin(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (packetId) {
            LoginStartPacket.ID -> {
                val loginStart = LoginStartPacket.read(body)
                sendUncompressed(LoginCompressionPacket.ID) { LoginCompressionPacket(compressionThreshold).write(it) }
                compressionEnabled = true

                val profile = GameProfile(OfflineUuid.forUsername(loginStart.name), loginStart.name)
                val sessionId = OfflineUuid.forUsername("session:${loginStart.name}")
                sendCompressed(LoginFinishedPacket.ID) { LoginFinishedPacket(profile, sessionId).write(it) }
            }
            LoginAcknowledgedPacket.ID -> {
                state = ProtocolState.CONFIGURATION // unimplemented past this point; session ends on next read
            }
        }
    }

    private fun sendUncompressed(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeUncompressed(wire, body.toByteArray())
        connection.write(wire.toByteArray())
    }

    private fun sendCompressed(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeCompressed(wire, body.toByteArray(), compressionThreshold, Zlib::compress)
        connection.write(wire.toByteArray())
    }
}
