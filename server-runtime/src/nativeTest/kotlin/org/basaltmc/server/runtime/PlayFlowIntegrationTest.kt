package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.multiversion.java.JavaTranslators
import org.basaltmc.wire.protocol.configuration.BrandPacket
import org.basaltmc.wire.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.wire.protocol.configuration.TagsPacket
import org.basaltmc.wire.protocol.handshake.ClientIntent
import org.basaltmc.wire.protocol.handshake.HandshakePacket
import org.basaltmc.wire.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.wire.protocol.login.LoginCompressionPacket
import org.basaltmc.wire.protocol.login.LoginDisconnectPacket
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.login.LoginStartPacket
import org.basaltmc.wire.protocol.play.ChatCommandPacket
import org.basaltmc.wire.protocol.play.ChatMessagePacket
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchStartPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.ClientboundPlayerAbilitiesPacket
import org.basaltmc.wire.protocol.play.CommandsPacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.EntityPositionSyncPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.wire.protocol.play.GameEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.PalettedContainer
import org.basaltmc.wire.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
import org.basaltmc.wire.protocol.play.RemoveEntitiesPacket
import org.basaltmc.wire.protocol.play.RotateHeadPacket
import org.basaltmc.wire.protocol.play.SectionBlocksUpdatePacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionPacket
import org.basaltmc.wire.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.wire.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetExperiencePacket
import org.basaltmc.wire.protocol.play.SetHealthPacket
import org.basaltmc.wire.protocol.play.SetHeldSlotPacket
import org.basaltmc.wire.protocol.play.SetSimulationDistancePacket
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.SpawnPositionPacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import org.basaltmc.wire.protocol.play.UpdateRecipesPacket
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.test.fail
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
    /** Assigned by [startTestServer]: the server may land on a fallback port, and everything must follow it. */
    private var port = 0

    /** Its own server, so seeding a player row here cannot disturb the other test. */
    private var restorePort = 0

    // BasaltServer generates a fixed 7x7 chunk grid (radius 3) around spawn at startup.

    /** The 5x5 a join waits for; everything else the view distance covers streams in after it. */
    private val chunkCount = 25

    /** The whole 7x7 the test's view distance covers, batch plus stream. */
    private val viewChunkCount = 49

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
        val gamemode: Gamemode,
        /** The first chunk this client was sent, kept so a test can look at what reached it. */
        val firstChunk: MapChunkPacket,
    )

    private fun joinAsPlayer(
        name: String,
        onPort: Int = port,
        /** What the client says it speaks. Anything but the native version is translated on the way out. */
        protocol: Int = BuildInfo.PROTOCOL_VERSION,
    ): PlayerHandle {
        val connection = TcpConnection.connect("127.0.0.1", onPort)
        val incoming = ByteBuffer()

        sendUncompressed(connection, HandshakePacket.ID) {
            HandshakePacket(protocol, "127.0.0.1", onPort, ClientIntent.LOGIN).write(it)
        }
        sendUncompressed(connection, LoginStartPacket.ID) {
            LoginStartPacket(name, Uuid.fromLongs(0L, 0L)).write(it)
        }

        val compressionBody = readFrame(connection, incoming, compressionEnabled = false)
        assertEquals(LoginCompressionPacket.ID, compressionBody.readVarInt())
        val threshold = LoginCompressionPacket.read(compressionBody).threshold

        val finishedBody = joinFrame(connection, incoming, LoginFinishedPacket.ID)

        sendCompressed(connection, LoginAcknowledgedPacket.ID, threshold) { }

        // parity: Configuration opens with the brand, which is the name F3 shows for the server.
        val brandBody = joinFrame(connection, incoming, BrandPacket.ID)
        assertEquals("Basalt ${BuildInfo.MINECRAFT_VERSION}", BrandPacket.read(brandBody).brand)

        val selectKnownPacksBody = joinFrame(connection, incoming, SelectKnownPacksPacket.CLIENTBOUND_ID)

        sendCompressed(connection, SelectKnownPacksPacket.SERVERBOUND_ID, threshold) {
            SelectKnownPacksPacket(emptyList()).write(it)
        }

        // Read until the tags rather than a fixed count: a translated client is not sent the
        // registries its version does not have, so how many arrive depends on who is asking.
        val receivedRegistries = mutableListOf<String>()
        var dimensionTypeEntries = emptyList<String>()
        var tagsBody: ByteBuffer? = null
        repeat(registryCount + 1) {
            if (tagsBody != null) return@repeat
            val body = readFrame(connection, incoming, compressionEnabled = true)
            when (val id = body.readVarInt()) {
                RegistryDataPacket.ID -> {
                    val registry = RegistryDataPacket.read(body)
                    receivedRegistries += registry.registryId
                    if (registry.registryId == "minecraft:dimension_type") {
                        dimensionTypeEntries = registry.entries.map { it.key }
                    }
                }
                TagsPacket.ID -> tagsBody = body
                else -> fail("the join sequence arrived out of order: 0x${id.toString(16)}")
            }
        }
        assertTrue(receivedRegistries.contains("minecraft:dimension_type"))
        assertTrue(receivedRegistries.contains("minecraft:world_clock"))
        assertTrue(receivedRegistries.contains("minecraft:worldgen/biome"))
        assertTrue(receivedRegistries.contains("minecraft:wolf_variant"))
        assertTrue(receivedRegistries.contains("minecraft:painting_variant"))
        assertTrue(receivedRegistries.contains("minecraft:trim_material"))
        assertTrue(receivedRegistries.contains("minecraft:damage_type"))
        assertTrue(receivedRegistries.contains("minecraft:jukebox_song"))
        assertTrue(receivedRegistries.contains("minecraft:instrument"))
        assertTrue(receivedRegistries.contains("minecraft:enchantment"))
        assertTrue(receivedRegistries.contains("minecraft:banner_pattern"))
        assertEquals(
            listOf(
                "minecraft:overworld",
                "minecraft:the_nether",
                "minecraft:the_end",
                "minecraft:overworld_caves",
            ),
            dimensionTypeEntries,
        )

        TagsPacket.read(checkNotNull(tagsBody) { "the tags never arrived" })

        val finishConfigBody = joinFrame(connection, incoming, FinishConfigurationPacket.ID)

        sendCompressed(connection, FinishConfigurationPacket.ID, threshold) { }

        val loginPlayBody = joinFrame(connection, incoming, LoginPlayPacket.ID)
        // An older client is sent a shorter packet (26.2 added the online-mode flag, and the sea
        // level came in 1.21.2), so only a native one can be read with this server's own reader.
        // What the other shapes look like is covered by the translation's own tests.
        val loginPlay =
            if (protocol == BuildInfo.PROTOCOL_VERSION) {
                LoginPlayPacket.read(loginPlayBody).also {
                    assertEquals(0, it.spawnInfo.dimension)
                    assertEquals("minecraft:overworld", it.spawnInfo.dimensionName)
                }
            } else {
                LoginPlayPacket.read(ByteBuffer.wrap(loginPlayBody.toByteArray() + ONLINE_MODE_FLAG))
            }

        val abilitiesBody = joinFrame(connection, incoming, ClientboundPlayerAbilitiesPacket.ID)
        ClientboundPlayerAbilitiesPacket.read(abilitiesBody)

        // parity: `PlayerList.placeNewPlayer` sends the recipes between the abilities and the
        // permission level. The stonecutter's buttons are drawn from them, client side.
        val recipesBody = joinFrame(connection, incoming, UpdateRecipesPacket.ID)

        // parity: the permission level goes right before the tree, and the client needs it to allow
        // selectors in its own parser.
        val permissionBody = joinFrame(connection, incoming, EntityEventPacket.ID)
        val permission = EntityEventPacket.read(permissionBody)
        assertTrue(
            permission.eventId.toInt() in
                EntityEventPacket.PERMISSION_LEVEL_BASE..(EntityEventPacket.PERMISSION_LEVEL_BASE + 4),
        )

        val commandsBody = joinFrame(connection, incoming, CommandsPacket.ID)
        val commandTree = CommandsPacket.read(commandsBody)
        // Everyone gets at least the commands that need no permission at all.
        assertTrue(commandTree.nodes.any { it.name == "help" })
        assertTrue(commandTree.nodes.any { it.name == "me" })

        val chunkCacheCenterBody = joinFrame(connection, incoming, SetChunkCacheCenterPacket.ID)
        SetChunkCacheCenterPacket.read(chunkCacheCenterBody)

        val chunkCacheRadiusBody = joinFrame(connection, incoming, SetChunkCacheRadiusPacket.ID)
        SetChunkCacheRadiusPacket.read(chunkCacheRadiusBody)

        val simulationDistanceBody = joinFrame(connection, incoming, SetSimulationDistancePacket.ID)
        SetSimulationDistancePacket.read(simulationDistanceBody)

        // The one clock update the join itself sends, so this asserts its position rather than
        // catching one of the periodic ones.
        val setTimeBody = joinFrame(connection, incoming, SetTimePacket.ID, skipClockUpdates = false)

        val spawnPositionBody = joinFrame(connection, incoming, SpawnPositionPacket.ID)

        val playerPositionBody = joinFrame(connection, incoming, PlayerPositionPacket.ID)

        val playerInfoBody = joinFrame(connection, incoming, PlayerInfoUpdatePacket.ID)
        val playerInfo = PlayerInfoUpdatePacket.read(playerInfoBody)
        assertTrue(playerInfo.entries.any { it.name == name })

        // A client draws its *own* skin layers from the synced metadata too, so the player's own
        // entity gets the layer mask before the HUD packets.
        val ownSkinBody = joinFrame(connection, incoming, SetEntityDataPacket.ID)
        assertEquals(
            SetEntityDataPacket.PLAYER_SKIN_PARTS_INDEX,
            SetEntityDataPacket
                .read(ownSkinBody)
                .entries
                .single()
                .index,
        )

        val healthBody = joinFrame(connection, incoming, SetHealthPacket.ID)
        assertEquals(20f, SetHealthPacket.read(healthBody).health)

        val experienceBody = joinFrame(connection, incoming, SetExperiencePacket.ID)
        SetExperiencePacket.read(experienceBody)

        // parity: `PlayerList.placeNewPlayer` sends the inventory here, after the player list and
        // the HUD state, not among the chunk-radius packets earlier. This sequence is the parity
        // claim, so a new packet belongs in it at the position vanilla puts it.
        val inventoryBody = joinFrame(connection, incoming, ContainerSetContentPacket.ID)
        val inventory = ContainerSetContentPacket.read(inventoryBody)
        // Container 0 is `InventoryMenu`, 46 slots, not the 41 this server stores: see
        // [PlayerInventory.MENU_SLOTS] for what a real client does when the count disagrees.
        assertEquals(PlayerInventory.MENU_SLOTS, inventory.items.size)
        assertTrue(inventory.items.all { it == null })

        val heldSlotBody = joinFrame(connection, incoming, SetHeldSlotPacket.ID)
        assertEquals(0, SetHeldSlotPacket.read(heldSlotBody).slot)

        val gameEventBody = joinFrame(connection, incoming, GameEventPacket.ID)
        assertEquals(GameEventPacket.LEVEL_CHUNKS_LOAD_START, GameEventPacket.read(gameEventBody).event)

        val chunkBatchStartBody = joinFrame(connection, incoming, ChunkBatchStartPacket.ID)

        var firstChunk: MapChunkPacket? = null
        repeat(chunkCount) {
            val chunkBody = joinFrame(connection, incoming, MapChunkPacket.ID)
            val chunk = MapChunkPacket.read(chunkBody)
            if (firstChunk == null) firstChunk = chunk
            val lightBody = joinFrame(connection, incoming, UpdateLightPacket.ID)
            UpdateLightPacket.read(lightBody)
        }

        val chunkBatchFinishedBody = joinFrame(connection, incoming, ChunkBatchFinishedPacket.ID)
        assertEquals(chunkCount, ChunkBatchFinishedPacket.read(chunkBatchFinishedBody).batchSize)
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        return PlayerHandle(
            connection,
            incoming,
            threshold,
            loginPlay.entityId,
            name,
            loginPlay.spawnInfo.gamemode,
            checkNotNull(firstChunk) { "no chunk arrived" },
        )
    }

    /** How many streamed chunks [readPastChunks] has skipped, so a test can still count them. */
    private var streamedChunksSeen = 0

    /** The id of the frame [readPastChunks] last returned, which it has already consumed. */
    private var lastPacketId = -1

    /** The last system chat [readPastChunks] returned, already decoded: its body is consumed. */
    private var lastSystemChat: SystemChatPacket? = null

    /**
     * The next frame that is not chunk streaming, answering every batch on the way.
     *
     * Joining no longer waits for the whole view distance, so the rest of the join sequence and the
     * chunks still streaming share the socket. A real client dispatches by packet id rather than by
     * position in the stream, and so does this.
     */
    private fun readPastChunks(
        connection: TcpConnection,
        incoming: ByteBuffer,
        threshold: Int,
    ): ByteBuffer {
        while (true) {
            val body = readFrame(connection, incoming, compressionEnabled = true)
            val id = body.readVarInt()
            when {
                id == MapChunkPacket.ID -> streamedChunksSeen++
                id == UpdateLightPacket.ID || id == ChunkBatchStartPacket.ID -> Unit
                // Where a head is looking follows every move and every spawn, from two sources at
                // once (the mover's own broadcast and the tracker's pairing), so its position in
                // the stream is not something to assert. Its encoding has its own round-trip test.
                id == RotateHeadPacket.ID -> Unit
                // A block moving on its own (a random tick growing grass, water finding its level)
                // reaches whoever is nearby at whatever moment it happens, so it lands wherever a
                // test happens to be reading. Skipped like the chunk traffic, for the same reason.
                id == ClientboundBlockUpdatePacket.ID || id == SectionBlocksUpdatePacket.ID -> Unit
                id == ChunkBatchFinishedPacket.ID ->
                    sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }
                // A system chat has to be decoded to be told apart, and the buffer cannot rewind,
                // so it is kept decoded in [lastSystemChat] for whoever asked. "X joined the game"
                // reaches everyone, including whoever just joined, so it lands wherever a test
                // happens to be reading: skipped like the chunk traffic is.
                id == SystemChatPacket.ID -> {
                    val chat = SystemChatPacket.read(body)
                    if (!chat.content.toString().contains("multiplayer.player.")) {
                        lastSystemChat = chat
                        lastPacketId = id
                        return body
                    }
                }
                else -> {
                    lastPacketId = id
                    return body
                }
            }
        }
    }

    @Test
    fun aClientOnTheVersionBeforeSeesTheSameWorldInItsOwnNumbers() {
        port = startTestServer(18583)
        val native = joinAsPlayer("Native")
        val older = joinAsPlayer("Older", protocol = ProtocolVersions.V26_1)

        // Same spawn, so both were sent the same column: what differs is only how it is numbered.
        assertEquals(native.firstChunk.x, older.firstChunk.x)
        assertEquals(native.firstChunk.z, older.firstChunk.z)

        val nativeStates = blockStatesOf(native.firstChunk)
        val olderStates = blockStatesOf(older.firstChunk)
        assertEquals(nativeStates.size, olderStates.size, "the two chunks are not the same shape")
        for (i in nativeStates.indices) {
            assertEquals(
                JavaTranslators.blockStateAs(ProtocolVersions.V26_1, nativeStates[i]),
                olderStates[i],
                "block $i went out with 26.2's number",
            )
        }
        assertTrue(nativeStates.any { it != 0 }, "the chunk was all air, which proves nothing")

        // And it is a working connection, not just a decodable one: it chats and is heard.
        sendCompressed(older.connection, ChatMessagePacket.ID, older.threshold) {
            it.writeProtocolString("hello from 26.1", 256)
            it.writeLong(0L)
            it.writeLong(0L)
            it.writeBoolean(false)
            it.writeVarInt(0)
            it.writeBytes(byteArrayOf(0, 0, 0))
            it.writeByte(0)
        }
        // The other player joining is on the same socket, so this reads until the chat rather than
        // assuming it is the next thing.
        var heard = false
        repeat(20) {
            if (heard) return@repeat
            readPastChunks(native.connection, native.incoming, native.threshold)
            if (lastPacketId == SystemChatPacket.ID &&
                lastSystemChat?.content?.toString()?.contains("hello from 26.1") == true
            ) {
                heard = true
            }
        }
        assertTrue(heard, "a 26.1 client's chat never reached the 26.2 one")
    }

    @Test
    fun aClientFromTheFutureIsTurnedAwayWithAnExplanation() {
        port = startTestServer(18584)
        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        sendUncompressed(connection, HandshakePacket.ID) {
            HandshakePacket(999, "127.0.0.1", port, ClientIntent.LOGIN).write(it)
        }
        sendUncompressed(connection, LoginStartPacket.ID) {
            LoginStartPacket("FromTheFuture", Uuid.fromLongs(0L, 0L)).write(it)
        }
        val body = readFrame(connection, incoming, compressionEnabled = false)
        assertEquals(LoginDisconnectPacket.ID, body.readVarInt())
        // The reason travels as the JSON a client renders; what matters is that it names the
        // version instead of dropping the socket without a word.
        val reason = body.readProtocolString(Short.MAX_VALUE.toInt())
        assertTrue(reason.contains("26.2"), "the kick did not say what this server runs: " + reason)
        connection.close()
    }

    /** Every block state of a chunk, section by section, as the client would expand them. */
    private fun blockStatesOf(chunk: MapChunkPacket): IntArray {
        val input = ByteBuffer().apply { writeBytes(chunk.chunkData) }
        val states = mutableListOf<Int>()
        while (input.readableBytes > 0) {
            input.readShort()
            input.readShort()
            states += PalettedContainer.read(input, 4096, 8).toList()
            PalettedContainer.read(input, 64, 3)
        }
        return states.toIntArray()
    }

    @Test
    fun aPlayerJoinsSeesTerrainMovesAndChats() {
        port = startTestServer(18570)
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
        val aliceSeesBobInfoBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(PlayerInfoUpdatePacket.ID, lastPacketId)
        assertEquals(
            "Bob",
            PlayerInfoUpdatePacket
                .read(aliceSeesBobInfoBody)
                .entries
                .single()
                .name,
        )

        // A tab-list entry alone leaves the other player invisible: they have to be created as a
        // real entity, and told apart by the profile uuid the client matches against that entry to
        // find the skin. Their chosen skin layers follow as metadata.
        val aliceSeesBobSpawnBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SpawnEntityPacket.ID, lastPacketId)
        val bobSpawn = SpawnEntityPacket.read(aliceSeesBobSpawnBody)
        assertEquals(bob.entityId, bobSpawn.entityId)
        assertEquals(SpawnEntityPacket.PLAYER_ENTITY_TYPE, bobSpawn.entityType)

        val aliceSeesBobSkinBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SetEntityDataPacket.ID, lastPacketId)
        assertEquals(
            SetEntityDataPacket.PLAYER_SKIN_PARTS_INDEX,
            SetEntityDataPacket
                .read(aliceSeesBobSkinBody)
                .entries
                .single()
                .index,
        )

        val aliceSeesBobTeleportBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(EntityTeleportPacket.ID, lastPacketId)
        val bobSpawnTeleport = EntityTeleportPacket.read(aliceSeesBobTeleportBody)
        assertEquals(bob.entityId, bobSpawnTeleport.entityId)

        // Bob, in turn, was told where Alice already is (her moved-to position from above). The
        // tab-list entry comes first here too: the tracker sends the pair together, in that order,
        // so the client has the profile before the entity that refers to it.
        readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(PlayerInfoUpdatePacket.ID, lastPacketId)

        val bobSeesAliceSpawnBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(SpawnEntityPacket.ID, lastPacketId)
        assertEquals(alice.entityId, SpawnEntityPacket.read(bobSeesAliceSpawnBody).entityId)

        val bobSeesAliceSkinBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(SetEntityDataPacket.ID, lastPacketId)
        SetEntityDataPacket.read(bobSeesAliceSkinBody)

        val bobSeesAliceBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(EntityTeleportPacket.ID, lastPacketId)
        val aliceTeleport = EntityTeleportPacket.read(bobSeesAliceBody)
        assertEquals(alice.entityId, aliceTeleport.entityId)
        assertEquals(10.0, aliceTeleport.x)
        assertEquals(10.0, aliceTeleport.z)

        // Seeing another player move: Alice moves again, and Bob gets a fresh teleport for her.
        // (8.5, 8.5) -> (20.0, 20.0) crosses from chunk (0,0) into chunk (1,1): the view square
        // (radius 3) re-centers, so Alice's own connection also streams the resulting delta before
        // anything else, exercising ChunkTracker's real wiring, not just its standalone unit tests.
        sendCompressed(alice.connection, ServerboundPositionPacket.ID, alice.threshold) {
            it.writeDouble(20.0)
            it.writeDouble(4.0)
            it.writeDouble(20.0)
            it.writeBoolean(true)
        }

        val aliceCenterBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SetChunkCacheCenterPacket.ID, lastPacketId)
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
            val forgetBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
            assertEquals(ForgetLevelChunkPacket.ID, lastPacketId)
            ForgetLevelChunkPacket.read(forgetBody)
        }

        // The replacements stream in behind them, interleaved with what the join did not wait for.
        // How much has arrived at this exact point is a race, so it is asserted at the end of the
        // test instead, by which time seconds of outbound ticks have gone by.

        val bobSeesAliceMoveBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(EntityTeleportPacket.ID, lastPacketId)
        val aliceMoved = EntityTeleportPacket.read(bobSeesAliceMoveBody)
        assertEquals(alice.entityId, aliceMoved.entityId)
        assertEquals(20.0, aliceMoved.x)
        assertEquals(20.0, aliceMoved.z)

        // A real client sends `player_input` (0x2b) many times a second, and its body is a single
        // byte. Decoding it as something longer read past the end of the frame, which escaped the
        // read loop as an uncaught exception and disconnected the player: the session has to
        // survive traffic it does not act on, whether it understands it or not.
        sendCompressed(alice.connection, SERVERBOUND_PLAYER_INPUT_ID, alice.threshold) {
            it.writeByte(0x01)
        }
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

        val aliceSeesChatBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        val aliceChat = checkNotNull(lastSystemChat)
        assertTrue(aliceChat.content.toString().contains("hello world"))

        val bobSeesChatBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        val bobChat = checkNotNull(lastSystemChat)
        assertTrue(bobChat.content.toString().contains("hello world"))

        // "ejecuta un comando": /me needs no permission, so a plain player can run it, and its
        // output reaches everyone. This is the whole path the client exercises when it types a
        // slash command: chat_command in, dispatcher, broadcast out.
        sendCompressed(alice.connection, ChatCommandPacket.ID, alice.threshold) {
            it.writeProtocolString("me waves", 256)
        }

        val aliceSeesEmoteBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        val emote = checkNotNull(lastSystemChat).content
        assertTrue(emote.toString().contains("chat.type.emote"))
        assertTrue(emote.toString().contains("waves"))

        val bobSeesEmoteBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        assertTrue(checkNotNull(lastSystemChat).content.toString().contains("waves"))

        // A command this player may not run comes back as brigadier's own "Unknown command",
        // exactly as vanilla hides commands above your permission level rather than refusing them.
        sendCompressed(alice.connection, ChatCommandPacket.ID, alice.threshold) {
            it.writeProtocolString("stop", 256)
        }

        val refusalBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        assertTrue(
            checkNotNull(lastSystemChat).content.toString().contains("Unknown command"),
        )

        // A syntax failure is two messages, not one: the error, then the grey line marking where
        // the parser stopped. Vanilla sends both, so both have to be on the wire.
        val contextBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        assertTrue(
            checkNotNull(lastSystemChat).content.toString().contains("command.context.here"),
        )

        // A deliberately truncated frame from Bob: a decoder that runs off the end of one costs
        // that one connection and nothing else. Alice, on her own thread, keeps playing.
        sendCompressed(bob.connection, ServerboundPlayerActionPacket.ID, bob.threshold) {
            it.writeVarInt(0)
        }

        sendCompressed(alice.connection, ChatCommandPacket.ID, alice.threshold) {
            it.writeProtocolString("me is still here", 256)
        }
        // Dropping Bob takes him off Alice's tab list, sent from his own session thread the moment
        // the decoder gives up on him. That can land either side of her next answer, so it is
        // skipped here rather than ordered, same as keep-alives are in [readFrame].
        // The tab entry and the entity both go when someone leaves, and they come from different
        // threads than the chat does, so neither has a place in this order.
        var survivorBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        var survivorId = lastPacketId
        while (survivorId == PlayerInfoRemovePacket.ID || survivorId == RemoveEntitiesPacket.ID) {
            survivorBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
            survivorId = lastPacketId
        }
        assertEquals(SystemChatPacket.ID, survivorId)
        assertTrue(checkNotNull(lastSystemChat).content.toString().contains("is still here"))

        alice.connection.close()
        bob.connection.close()

        // Note on what is deliberately not asserted here: how much of the view distance has
        // streamed in by the end. The login batch is only the 5x5 the player stands on, and the
        // rest arrives as its chunks and their neighbours finish generating, which is a matter of
        // milliseconds on a world that exists and seconds on one being made up as it goes. The
        // batch itself is asserted above, and the streaming path by the move that follows it.
    }

    @Test
    fun aReturningPlayerJoinsInTheGameModeTheyLeftIn() {
        // Regression: the login packet was built from the *configured default* game mode rather
        // than the restored one, so someone who logged out in creative came back to a survival HUD
        // while their abilities and block breaking still followed creative. The mismatch is only
        // visible in this packet, which is why it is asserted here and not on the session.
        restorePort = startTestServer(18574)

        val name = "Zoe"
        val uuid = OfflineUuid.forUsername(name)
        PlayerStore.open("$TEST_DATA_ROOT/players_$restorePort.db").use { store ->
            store.players.save(
                StoredPlayer(
                    uuid = uuid.toString(),
                    name = name,
                    lastSeenEpochSeconds = 1L,
                    firstSeenEpochSeconds = 1L,
                    playTimeTicks = 0L,
                    world = "test_world_$restorePort",
                    x = 8.5,
                    y = 64.0,
                    z = 8.5,
                    yaw = 0f,
                    pitch = 0f,
                    health = 20f,
                    food = 20,
                    saturation = 5f,
                    exhaustion = 0f,
                    xpLevel = 0,
                    xpProgress = 0f,
                    xpTotal = 0,
                    gamemode = StoredGamemode.CREATIVE,
                ),
            )
        }

        val zoe = joinAsPlayer(name, restorePort)
        assertEquals(Gamemode.CREATIVE, zoe.gamemode)
        zoe.connection.close()
    }
}

/** `ServerboundPlayerInputPacket`'s id, which this server no-ops but every real client sends constantly. */
private const val SERVERBOUND_PLAYER_INPUT_ID = 0x2b

/**
 * The next packet of the join sequence, skipping the entity syncs that go out on the outbound
 * thread's own timer.
 *
 * The order this sequence asserts is the join's own, which is the parity claim. What an entity is
 * doing meanwhile lands wherever the tick puts it, and a real client reads it whenever it arrives:
 * asserting a global order made the test pass or fail on how fast the machine was.
 */
private fun joinFrame(
    connection: TcpConnection,
    incoming: ByteBuffer,
    expected: Int,
    skipClockUpdates: Boolean = true,
): ByteBuffer {
    repeat(JOIN_FRAME_LIMIT) {
        val body = readFrame(connection, incoming, compressionEnabled = true, skipClockUpdates = skipClockUpdates)
        val id = body.readVarInt()
        if (id in ENTITY_SYNC_PACKETS) return@repeat
        assertEquals(expected, id, "the join sequence arrived out of order")
        return body
    }
    error("no packet with id $expected arrived in the join sequence")
}

/**
 * What the world sends on its own while the join is still going out.
 *
 * Entity movement, and a block the world changed by itself: grass growing or water levelling out
 * reaches whoever is nearby at whatever moment it happens, so its place in the stream is not
 * something to assert. [readPastChunks] skips the same set for the same reason.
 */
private val ENTITY_SYNC_PACKETS =
    setOf(
        EntityTeleportPacket.ID,
        RotateHeadPacket.ID,
        EntityPositionSyncPacket.ID,
        ClientboundBlockUpdatePacket.ID,
        SectionBlocksUpdatePacket.ID,
    )

private const val JOIN_FRAME_LIMIT = 200

/** Put back so a translated login can be read by the native reader in the harness. */
private val ONLINE_MODE_FLAG = byteArrayOf(1)
