package org.basaltmc.server

import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.TcpConnection
import org.basaltmc.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.protocol.configuration.RegistryDataPacket
import org.basaltmc.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.protocol.handshake.ClientIntent
import org.basaltmc.protocol.handshake.HandshakePacket
import org.basaltmc.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.protocol.login.LoginCompressionPacket
import org.basaltmc.protocol.login.LoginFinishedPacket
import org.basaltmc.protocol.login.LoginStartPacket
import org.basaltmc.protocol.play.ChatMessagePacket
import org.basaltmc.protocol.play.EntityTeleportPacket
import org.basaltmc.protocol.play.LoginPlayPacket
import org.basaltmc.protocol.play.MapChunkPacket
import org.basaltmc.protocol.play.PlayerInfoAddPacket
import org.basaltmc.protocol.play.PlayerPositionPacket
import org.basaltmc.protocol.play.ServerboundPositionPacket
import org.basaltmc.protocol.play.SpawnPositionPacket
import org.basaltmc.protocol.play.SystemChatPacket
import org.basaltmc.protocol.play.UpdateLightPacket
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeBoolean
import org.basaltmc.protocol.writeProtocolString
import org.basaltmc.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Drives two real players through Login, Configuration and Play over real sockets: the
 * acceptance criterion ("a player joins, sees terrain, moves, chats, and sees another player
 * move"), against Basalt's own synthetic client (see `MojangSessionService`'s/`SystemChatPacket`'s
 * KDocs for what real-vanilla-client compatibility still needs from later phases).
 */
@OptIn(ExperimentalUuidApi::class)
class PlayFlowIntegrationTest {
    private val port = 25570

    // BasaltServer generates a fixed 3x3 chunk grid (radius 1) around spawn at startup.
    private val chunkCount = 9

    private class PlayerHandle(
        val connection: TcpConnection,
        val incoming: ByteBuffer,
        val threshold: Int,
        val entityId: Int,
        val name: String,
    )

    private fun joinAsPlayer(name: String): PlayerHandle {
        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()

        sendUncompressed(connection, HandshakePacket.ID) {
            HandshakePacket(776, "127.0.0.1", port, ClientIntent.LOGIN).write(it)
        }
        sendUncompressed(connection, LoginStartPacket.ID) {
            LoginStartPacket(name, Uuid.fromLongs(0L, 0L)).write(it)
        }

        val compressionBody = readFrame(connection, incoming, compressionEnabled = false)
        assertEquals(LoginCompressionPacket.ID, compressionBody.readVarInt())
        val threshold = LoginCompressionPacket.read(compressionBody).threshold

        val finishedBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(LoginFinishedPacket.ID, finishedBody.readVarInt())

        sendCompressed(connection, LoginAcknowledgedPacket.ID, threshold) { }

        val selectKnownPacksBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SelectKnownPacksPacket.CLIENTBOUND_ID, selectKnownPacksBody.readVarInt())

        sendCompressed(connection, SelectKnownPacksPacket.SERVERBOUND_ID, threshold) {
            SelectKnownPacksPacket(emptyList()).write(it)
        }

        val dimensionRegistryBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(RegistryDataPacket.ID, dimensionRegistryBody.readVarInt())
        val dimensionRegistry = RegistryDataPacket.read(dimensionRegistryBody)
        assertEquals("minecraft:dimension_type", dimensionRegistry.registryId)

        val biomeRegistryBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(RegistryDataPacket.ID, biomeRegistryBody.readVarInt())
        val biomeRegistry = RegistryDataPacket.read(biomeRegistryBody)
        assertEquals("minecraft:worldgen/biome", biomeRegistry.registryId)

        val finishConfigBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(FinishConfigurationPacket.ID, finishConfigBody.readVarInt())

        sendCompressed(connection, FinishConfigurationPacket.ID, threshold) { }

        val loginPlayBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(LoginPlayPacket.ID, loginPlayBody.readVarInt())
        val loginPlay = LoginPlayPacket.read(loginPlayBody)

        repeat(chunkCount) {
            val chunkBody = readFrame(connection, incoming, compressionEnabled = true)
            assertEquals(MapChunkPacket.ID, chunkBody.readVarInt())
            MapChunkPacket.read(chunkBody)
            val lightBody = readFrame(connection, incoming, compressionEnabled = true)
            assertEquals(UpdateLightPacket.ID, lightBody.readVarInt())
            UpdateLightPacket.read(lightBody)
        }

        val spawnPositionBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SpawnPositionPacket.ID, spawnPositionBody.readVarInt())

        val playerPositionBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(PlayerPositionPacket.ID, playerPositionBody.readVarInt())

        val playerInfoBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(PlayerInfoAddPacket.ID, playerInfoBody.readVarInt())
        val playerInfo = PlayerInfoAddPacket.read(playerInfoBody)
        assertTrue(playerInfo.entries.any { it.name == name })

        return PlayerHandle(connection, incoming, threshold, loginPlay.entityId, name)
    }

    @Test
    fun aPlayerJoinsSeesTerrainMovesAndChats() {
        startTestServer(port)
        val alice = joinAsPlayer("Alice")

        // "ve terreno": the fixed chunk grid was received and parsed above (chunkCount pairs).
        assertTrue(chunkCount > 0)

        // "se mueve": a movement packet does not error out or desync framing.
        sendCompressed(alice.connection, ServerboundPositionPacket.ID, alice.threshold) {
            it.writeDouble(10.0)
            it.writeDouble(4.0)
            it.writeDouble(10.0)
            it.writeBoolean(true)
        }

        val bob = joinAsPlayer("Bob")

        // Alice sees Bob join: a player_info add, then a teleport placing him at spawn.
        val aliceSeesBobInfoBody = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
        assertEquals(PlayerInfoAddPacket.ID, aliceSeesBobInfoBody.readVarInt())
        assertEquals(
            "Bob",
            PlayerInfoAddPacket
                .read(aliceSeesBobInfoBody)
                .entries
                .single()
                .name,
        )

        val aliceSeesBobTeleportBody = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
        assertEquals(EntityTeleportPacket.ID, aliceSeesBobTeleportBody.readVarInt())
        val bobSpawnTeleport = EntityTeleportPacket.read(aliceSeesBobTeleportBody)
        assertEquals(bob.entityId, bobSpawnTeleport.entityId)

        // Bob, in turn, was told where Alice already is (her moved-to position from above).
        val bobSeesAliceBody = readFrame(bob.connection, bob.incoming, compressionEnabled = true)
        assertEquals(EntityTeleportPacket.ID, bobSeesAliceBody.readVarInt())
        val aliceTeleport = EntityTeleportPacket.read(bobSeesAliceBody)
        assertEquals(alice.entityId, aliceTeleport.entityId)
        assertEquals(10.0, aliceTeleport.x)
        assertEquals(10.0, aliceTeleport.z)

        // "ve a otro jugador moverse": Alice moves again, Bob must see a fresh teleport for her.
        sendCompressed(alice.connection, ServerboundPositionPacket.ID, alice.threshold) {
            it.writeDouble(20.0)
            it.writeDouble(4.0)
            it.writeDouble(20.0)
            it.writeBoolean(true)
        }
        val bobSeesAliceMoveBody = readFrame(bob.connection, bob.incoming, compressionEnabled = true)
        assertEquals(EntityTeleportPacket.ID, bobSeesAliceMoveBody.readVarInt())
        val aliceMoved = EntityTeleportPacket.read(bobSeesAliceMoveBody)
        assertEquals(alice.entityId, aliceMoved.entityId)
        assertEquals(20.0, aliceMoved.x)
        assertEquals(20.0, aliceMoved.z)

        // "chatea": Alice's chat reaches both herself and Bob.
        sendCompressed(alice.connection, ChatMessagePacket.ID, alice.threshold) {
            it.writeProtocolString("hello world", 256)
            it.writeLong(0L)
            it.writeLong(0L)
            it.writeBoolean(false)
            it.writeVarInt(0)
            it.writeBytes(byteArrayOf(0, 0, 0))
            it.writeByte(0)
        }

        val aliceSeesChatBody = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
        assertEquals(SystemChatPacket.ID, aliceSeesChatBody.readVarInt())
        val aliceChat = SystemChatPacket.read(aliceSeesChatBody)
        assertTrue(aliceChat.content.toString().contains("hello world"))

        val bobSeesChatBody = readFrame(bob.connection, bob.incoming, compressionEnabled = true)
        assertEquals(SystemChatPacket.ID, bobSeesChatBody.readVarInt())
        val bobChat = SystemChatPacket.read(bobSeesChatBody)
        assertTrue(bobChat.content.toString().contains("hello world"))

        alice.connection.close()
        bob.connection.close()
    }
}
