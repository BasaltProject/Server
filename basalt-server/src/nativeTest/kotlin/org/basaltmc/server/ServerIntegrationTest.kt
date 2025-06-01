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
import org.basaltmc.io.TcpConnection
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
import org.basaltmc.text.PlainTextSerializer
import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Runs the actual [BasaltServer] on a real socket and drives it with `basalt-io`'s own
 * client socket, exercising the whole stack (sockets, framing, compression, packets) together:
 * not just each module's isolated unit tests.
 */
@OptIn(ExperimentalUuidApi::class)
class ServerIntegrationTest {
    private val port = 39566

    private fun startServer() {
        startTestServer(port)
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
        // Whatever the selected server_list.toml profile says, rendered: the description is no
        // longer a fixed string with a fixed colour.
        assertEquals("A Basalt Server", PlainTextSerializer.serialize(status.description))
        assertEquals(0, status.players?.online)
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
        val onlinePort = 39568
        val serverKeyPair = SessionKeyPair.generate()
        val authenticatedProfile = SessionProfile(Uuid.parse("11111111-2222-3333-4444-555555555555"), "Notch")
        var sawUsername: String? = null
        val sessionService =
            SessionService { username, _, _ ->
                sawUsername = username
                authenticatedProfile
            }
        startTestServer(onlinePort, OnlineMode(serverKeyPair, sessionService))

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
