package org.basaltmc.server

import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.TcpConnection
import org.basaltmc.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.protocol.configuration.RegistryDataPacket
import org.basaltmc.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.protocol.configuration.TagsPacket
import org.basaltmc.protocol.handshake.ClientIntent
import org.basaltmc.protocol.handshake.HandshakePacket
import org.basaltmc.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.protocol.login.LoginCompressionPacket
import org.basaltmc.protocol.login.LoginFinishedPacket
import org.basaltmc.protocol.login.LoginStartPacket
import org.basaltmc.protocol.play.ChatMessagePacket
import org.basaltmc.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.protocol.play.ChunkBatchStartPacket
import org.basaltmc.protocol.play.EntityTeleportPacket
import org.basaltmc.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.protocol.play.GameEventPacket
import org.basaltmc.protocol.play.LoginPlayPacket
import org.basaltmc.protocol.play.MapChunkPacket
import org.basaltmc.protocol.play.PlayerInfoAddPacket
import org.basaltmc.protocol.play.PlayerPositionPacket
import org.basaltmc.protocol.play.ServerboundPositionPacket
import org.basaltmc.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.protocol.play.SetSimulationDistancePacket
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
    private val port = 39570

    // BasaltServer generates a fixed 7x7 chunk grid (radius 3) around spawn at startup.
    private val chunkCount = 49

    // The full 29-registry RegistryDataLoader.SYNCHRONIZED_REGISTRIES set: dimension_type +
    // worldgen/biome + the 13 hard-required-non-empty + trim_material/damage_type/jukebox_song/
    // instrument (needed by items' default holder components) + the 10 remaining sent empty.
    private val registryCount = 29

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

        val receivedRegistries = mutableListOf<String>()
        repeat(registryCount) {
            val registryBody = readFrame(connection, incoming, compressionEnabled = true)
            assertEquals(RegistryDataPacket.ID, registryBody.readVarInt())
            receivedRegistries += RegistryDataPacket.read(registryBody).registryId
        }
        assertTrue(receivedRegistries.contains("minecraft:dimension_type"))
        assertTrue(receivedRegistries.contains("minecraft:worldgen/biome"))
        assertTrue(receivedRegistries.contains("minecraft:wolf_variant"))
        assertTrue(receivedRegistries.contains("minecraft:painting_variant"))
        assertTrue(receivedRegistries.contains("minecraft:trim_material"))
        assertTrue(receivedRegistries.contains("minecraft:damage_type"))
        assertTrue(receivedRegistries.contains("minecraft:jukebox_song"))
        assertTrue(receivedRegistries.contains("minecraft:instrument"))
        assertTrue(receivedRegistries.contains("minecraft:enchantment"))
        assertTrue(receivedRegistries.contains("minecraft:banner_pattern"))

        val tagsBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(TagsPacket.ID, tagsBody.readVarInt())
        TagsPacket.read(tagsBody)

        val finishConfigBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(FinishConfigurationPacket.ID, finishConfigBody.readVarInt())

        sendCompressed(connection, FinishConfigurationPacket.ID, threshold) { }

        val loginPlayBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(LoginPlayPacket.ID, loginPlayBody.readVarInt())
        val loginPlay = LoginPlayPacket.read(loginPlayBody)

        val chunkCacheCenterBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetChunkCacheCenterPacket.ID, chunkCacheCenterBody.readVarInt())
        SetChunkCacheCenterPacket.read(chunkCacheCenterBody)

        val chunkCacheRadiusBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetChunkCacheRadiusPacket.ID, chunkCacheRadiusBody.readVarInt())
        SetChunkCacheRadiusPacket.read(chunkCacheRadiusBody)

        val simulationDistanceBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetSimulationDistancePacket.ID, simulationDistanceBody.readVarInt())
        SetSimulationDistancePacket.read(simulationDistanceBody)

        val spawnPositionBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SpawnPositionPacket.ID, spawnPositionBody.readVarInt())

        val playerPositionBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(PlayerPositionPacket.ID, playerPositionBody.readVarInt())

        val playerInfoBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(PlayerInfoAddPacket.ID, playerInfoBody.readVarInt())
        val playerInfo = PlayerInfoAddPacket.read(playerInfoBody)
        assertTrue(playerInfo.entries.any { it.name == name })

        val gameEventBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(GameEventPacket.ID, gameEventBody.readVarInt())
        assertEquals(GameEventPacket.LEVEL_CHUNKS_LOAD_START, GameEventPacket.read(gameEventBody).event)

        val chunkBatchStartBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(ChunkBatchStartPacket.ID, chunkBatchStartBody.readVarInt())

        repeat(chunkCount) {
            val chunkBody = readFrame(connection, incoming, compressionEnabled = true)
            assertEquals(MapChunkPacket.ID, chunkBody.readVarInt())
            MapChunkPacket.read(chunkBody)
            val lightBody = readFrame(connection, incoming, compressionEnabled = true)
            assertEquals(UpdateLightPacket.ID, lightBody.readVarInt())
            UpdateLightPacket.read(lightBody)
        }

        val chunkBatchFinishedBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(ChunkBatchFinishedPacket.ID, chunkBatchFinishedBody.readVarInt())
        assertEquals(chunkCount, ChunkBatchFinishedPacket.read(chunkBatchFinishedBody).batchSize)

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
        // (8.5, 8.5) -> (20.0, 20.0) crosses from chunk (0,0) into chunk (1,1): the view square
        // (radius 3) re-centers, so Alice's own connection also streams the resulting delta before
        // anything else, exercising ChunkTracker's real wiring, not just its standalone unit tests.
        sendCompressed(alice.connection, ServerboundPositionPacket.ID, alice.threshold) {
            it.writeDouble(20.0)
            it.writeDouble(4.0)
            it.writeDouble(20.0)
            it.writeBoolean(true)
        }

        val aliceCenterBody = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
        assertEquals(SetChunkCacheCenterPacket.ID, aliceCenterBody.readVarInt())
        assertEquals(SetChunkCacheCenterPacket(1, 1), SetChunkCacheCenterPacket.read(aliceCenterBody))

        // A 7x7 square shifted by (1,1) overlaps its old position in a 6x6 area: 49 - 36 = 13 chunks
        // newly in range, 13 fell out of range.
        //
        // What falls out of range is dropped immediately, before anything new is sent: the client
        // does not have to hold onto chunks it no longer needs while replacements are still being
        // generated. The replacements themselves arrive afterwards, across however many batches it
        // takes, because generation runs on a worker pool and the sending side never waits for it.
        val streamedChunkCount = 13
        repeat(streamedChunkCount) {
            val forgetBody = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
            assertEquals(ForgetLevelChunkPacket.ID, forgetBody.readVarInt())
            ForgetLevelChunkPacket.read(forgetBody)
        }

        var streamedChunksSeen = 0
        while (streamedChunksSeen < streamedChunkCount) {
            val batchStartBody = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
            assertEquals(ChunkBatchStartPacket.ID, batchStartBody.readVarInt())
            val batchSizeBody: ByteBuffer
            var inThisBatch = 0
            while (true) {
                val body = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
                val id = body.readVarInt()
                if (id == ChunkBatchFinishedPacket.ID) {
                    batchSizeBody = body
                    break
                }
                assertEquals(MapChunkPacket.ID, id)
                MapChunkPacket.read(body)
                val lightBody = readFrame(alice.connection, alice.incoming, compressionEnabled = true)
                assertEquals(UpdateLightPacket.ID, lightBody.readVarInt())
                UpdateLightPacket.read(lightBody)
                inThisBatch++
            }
            assertEquals(inThisBatch, ChunkBatchFinishedPacket.read(batchSizeBody).batchSize)
            // A real client answers every batch with the rate it can keep up with, and the server
            // paces itself by it. A client that stayed silent would be throttled to a trickle, on
            // purpose: that is the protocol's flow control doing its job.
            sendCompressed(alice.connection, ChunkBatchReceivedPacket.ID, alice.threshold) {
                it.writeFloat(16f)
            }
            assertTrue(inThisBatch > 0, "the server announced a chunk batch and then sent no chunks")
            streamedChunksSeen += inThisBatch
        }
        assertEquals(streamedChunkCount, streamedChunksSeen)

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
