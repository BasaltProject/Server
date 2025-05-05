package org.basaltmc.server

import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.DelicateCryptographyApi
import dev.whyoleg.cryptography.algorithms.RSA
import dev.whyoleg.cryptography.algorithms.SHA1
import org.basaltmc.auth.Cfb8Cipher
import org.basaltmc.auth.OfflineUuid
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionKeyPair
import org.basaltmc.auth.SessionProfile
import org.basaltmc.auth.SessionService
import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpConnection
import org.basaltmc.io.Zlib
import org.basaltmc.protocol.PacketFraming
import org.basaltmc.protocol.handshake.ClientIntent
import org.basaltmc.protocol.handshake.HandshakePacket
import org.basaltmc.protocol.login.HelloPacket
import org.basaltmc.protocol.login.KeyPacket
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
import org.basaltmc.text.TextColor
import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Runs the actual [BasaltServer] on a real socket and drives it with `basalt-io`'s own
 * client socket, exercising the whole stack (sockets, framing, compression, packets) together —
 * not just each module's isolated unit tests.
 */
@OptIn(ExperimentalUuidApi::class)
class ServerIntegrationTest {
    private val port = 25566

    private fun startServer() {
        startServerOn(port, onlineMode = null)
    }

    private fun startServerOn(
        boundPort: Int,
        onlineMode: OnlineMode?,
    ) {
        NativeThread.start { BasaltServer(boundPort, onlineMode).start() }
        // Give the listener a moment to bind before the client connects.
        val deadline =
            kotlin.time.TimeSource.Monotonic
                .markNow()
        while (deadline.elapsedNow().inWholeMilliseconds < 500) {
            try {
                TcpConnection.connect("127.0.0.1", boundPort).close()
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

    private fun sendUncompressedEncrypted(
        connection: TcpConnection,
        id: Int,
        cipher: Cfb8Cipher,
        writeBody: (ByteBuffer) -> Unit = {},
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeUncompressed(wire, body.toByteArray())
        connection.write(cipher.update(wire.toByteArray(), encrypting = true))
    }

    private fun sendCompressedEncrypted(
        connection: TcpConnection,
        id: Int,
        threshold: Int,
        cipher: Cfb8Cipher,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeCompressed(wire, body.toByteArray(), threshold, Zlib::compress)
        connection.write(cipher.update(wire.toByteArray(), encrypting = true))
    }

    private fun readFrameEncrypted(
        connection: TcpConnection,
        incoming: ByteBuffer,
        compressionEnabled: Boolean,
        cipher: Cfb8Cipher,
    ): ByteBuffer {
        val chunk = ByteArray(4096)
        while (true) {
            PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)?.let { frame ->
                incoming.discardReadBytes()
                return ByteBuffer(frame.size).also { it.writeBytes(frame) }
            }
            val n = connection.read(chunk)
            check(n > 0) { "connection closed while waiting for a frame" }
            incoming.writeBytes(cipher.update(chunk, length = n, encrypting = false))
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
        assertEquals(TextColor.GRAY, status.description.style.color)
        assertEquals(Favicon.bytes.size, status.favicon?.size)
        val pngSignature = byteArrayOf(0x89.toByte(), 'P'.code.toByte(), 'N'.code.toByte(), 'G'.code.toByte())
        assertEquals(pngSignature.toList(), status.favicon?.take(4))

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

    /**
     * Drives the real RSA/AES-CFB8 key exchange end to end over a real socket: this test plays
     * the client side (decode the server's DER public key, RSA-encrypt a shared secret and the
     * challenge, then switch to AES-128/CFB8 for everything after [KeyPacket]), against the real
     * [BasaltServer]/[ClientSession] server side, with a fake [SessionService] standing in for
     * the not-yet-implemented Mojang HTTPS call (see [SessionService]'s KDoc).
     */
    @Test
    @OptIn(DelicateCryptographyApi::class)
    fun onlineLoginFlowOverRealSockets() {
        val onlinePort = 25568
        val serverKeyPair = SessionKeyPair.generate()
        val authenticatedProfile = SessionProfile(Uuid.parse("11111111-2222-3333-4444-555555555555"), "Notch")
        var sawUsername: String? = null
        val sessionService =
            SessionService { username, _, _ ->
                sawUsername = username
                authenticatedProfile
            }
        startServerOn(onlinePort, OnlineMode(serverKeyPair, sessionService))

        val connection = TcpConnection.connect("127.0.0.1", onlinePort)
        val incoming = ByteBuffer()

        sendUncompressed(connection, HandshakePacket.ID) {
            HandshakePacket(776, "127.0.0.1", onlinePort, ClientIntent.LOGIN).write(it)
        }
        sendUncompressed(connection, LoginStartPacket.ID) {
            LoginStartPacket("Notch", Uuid.fromLongs(0L, 0L)).write(it)
        }

        val helloBody = readFrame(connection, incoming, compressionEnabled = false)
        assertEquals(HelloPacket.ID, helloBody.readVarInt())
        val hello = HelloPacket.read(helloBody)
        assertEquals("", hello.serverId)
        assertTrue(hello.shouldAuthenticate)

        val rsa = CryptographyProvider.Default.get(RSA.PKCS1)
        val serverPublicKey =
            rsa.publicKeyDecoder(SHA1).decodeFromByteArrayBlocking(RSA.PublicKey.Format.DER, hello.publicKey)
        val sharedSecret = Random.nextBytes(16)
        val encryptedSecret = serverPublicKey.encryptor().encryptBlocking(sharedSecret)
        val encryptedChallenge = serverPublicKey.encryptor().encryptBlocking(hello.challenge)
        sendUncompressed(connection, KeyPacket.ID) { KeyPacket(encryptedSecret, encryptedChallenge).write(it) }

        val clientEncryptCipher = Cfb8Cipher.create(sharedSecret)
        val clientDecryptCipher = Cfb8Cipher.create(sharedSecret)

        val compressionBody =
            readFrameEncrypted(connection, incoming, compressionEnabled = false, cipher = clientDecryptCipher)
        assertEquals(LoginCompressionPacket.ID, compressionBody.readVarInt())
        val threshold = LoginCompressionPacket.read(compressionBody).threshold
        assertTrue(threshold > 0)

        val finishedBody =
            readFrameEncrypted(connection, incoming, compressionEnabled = true, cipher = clientDecryptCipher)
        assertEquals(LoginFinishedPacket.ID, finishedBody.readVarInt())
        val finished = LoginFinishedPacket.read(finishedBody)
        assertEquals("Notch", finished.gameProfile.name)
        assertEquals(authenticatedProfile.id, finished.gameProfile.id)
        assertEquals("Notch", sawUsername)

        sendCompressedEncrypted(connection, LoginAcknowledgedPacket.ID, threshold, clientEncryptCipher) { }
        connection.close()
    }
}
