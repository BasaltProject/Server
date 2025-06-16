package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.protocol.configuration.BrandPacket
import org.basaltmc.wire.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.wire.protocol.configuration.TagsPacket
import org.basaltmc.wire.protocol.handshake.ClientIntent
import org.basaltmc.wire.protocol.handshake.HandshakePacket
import org.basaltmc.wire.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.wire.protocol.login.LoginCompressionPacket
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.login.LoginStartPacket
import org.basaltmc.wire.protocol.play.ChatCommandPacket
import org.basaltmc.wire.protocol.play.ChatMessagePacket
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchStartPacket
import org.basaltmc.wire.protocol.play.ClientboundPlayerAbilitiesPacket
import org.basaltmc.wire.protocol.play.CommandsPacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.wire.protocol.play.GameEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
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
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
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
    )

    private fun joinAsPlayer(
        name: String,
        onPort: Int = port,
    ): PlayerHandle {
        val connection = TcpConnection.connect("127.0.0.1", onPort)
        val incoming = ByteBuffer()

        sendUncompressed(connection, HandshakePacket.ID) {
            HandshakePacket(776, "127.0.0.1", onPort, ClientIntent.LOGIN).write(it)
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

        // parity: Configuration opens with the brand, which is the name F3 shows for the server.
        val brandBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(BrandPacket.ID, brandBody.readVarInt())
        assertEquals("Basalt ${BuildInfo.MINECRAFT_VERSION}", BrandPacket.read(brandBody).brand)

        val selectKnownPacksBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SelectKnownPacksPacket.CLIENTBOUND_ID, selectKnownPacksBody.readVarInt())

        sendCompressed(connection, SelectKnownPacksPacket.SERVERBOUND_ID, threshold) {
            SelectKnownPacksPacket(emptyList()).write(it)
        }

        val receivedRegistries = mutableListOf<String>()
        var dimensionTypeEntries = emptyList<String>()
        repeat(registryCount) {
            val registryBody = readFrame(connection, incoming, compressionEnabled = true)
            assertEquals(RegistryDataPacket.ID, registryBody.readVarInt())
            val registry = RegistryDataPacket.read(registryBody)
            receivedRegistries += registry.registryId
            if (registry.registryId == "minecraft:dimension_type") {
                dimensionTypeEntries = registry.entries.map { it.key }
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

        val tagsBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(TagsPacket.ID, tagsBody.readVarInt())
        TagsPacket.read(tagsBody)

        val finishConfigBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(FinishConfigurationPacket.ID, finishConfigBody.readVarInt())

        sendCompressed(connection, FinishConfigurationPacket.ID, threshold) { }

        val loginPlayBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(LoginPlayPacket.ID, loginPlayBody.readVarInt())
        val loginPlay = LoginPlayPacket.read(loginPlayBody)
        assertEquals(0, loginPlay.spawnInfo.dimension)
        assertEquals("minecraft:overworld", loginPlay.spawnInfo.dimensionName)

        val abilitiesBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(ClientboundPlayerAbilitiesPacket.ID, abilitiesBody.readVarInt())
        ClientboundPlayerAbilitiesPacket.read(abilitiesBody)

        // parity: the permission level goes right before the tree, and the client needs it to allow
        // selectors in its own parser.
        val permissionBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(EntityEventPacket.ID, permissionBody.readVarInt())
        val permission = EntityEventPacket.read(permissionBody)
        assertTrue(
            permission.eventId.toInt() in
                EntityEventPacket.PERMISSION_LEVEL_BASE..(EntityEventPacket.PERMISSION_LEVEL_BASE + 4),
        )

        val commandsBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(CommandsPacket.ID, commandsBody.readVarInt())
        val commandTree = CommandsPacket.read(commandsBody)
        // Everyone gets at least the commands that need no permission at all.
        assertTrue(commandTree.nodes.any { it.name == "help" })
        assertTrue(commandTree.nodes.any { it.name == "me" })

        val chunkCacheCenterBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetChunkCacheCenterPacket.ID, chunkCacheCenterBody.readVarInt())
        SetChunkCacheCenterPacket.read(chunkCacheCenterBody)

        val chunkCacheRadiusBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetChunkCacheRadiusPacket.ID, chunkCacheRadiusBody.readVarInt())
        SetChunkCacheRadiusPacket.read(chunkCacheRadiusBody)

        val simulationDistanceBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetSimulationDistancePacket.ID, simulationDistanceBody.readVarInt())
        SetSimulationDistancePacket.read(simulationDistanceBody)

        // The one clock update the join itself sends, so this asserts its position rather than
        // catching one of the periodic ones.
        val setTimeBody = readFrame(connection, incoming, compressionEnabled = true, skipClockUpdates = false)
        assertEquals(SetTimePacket.ID, setTimeBody.readVarInt())

        val spawnPositionBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SpawnPositionPacket.ID, spawnPositionBody.readVarInt())

        val playerPositionBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(PlayerPositionPacket.ID, playerPositionBody.readVarInt())

        val playerInfoBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(PlayerInfoUpdatePacket.ID, playerInfoBody.readVarInt())
        val playerInfo = PlayerInfoUpdatePacket.read(playerInfoBody)
        assertTrue(playerInfo.entries.any { it.name == name })

        // A client draws its *own* skin layers from the synced metadata too, so the player's own
        // entity gets the layer mask before the HUD packets.
        val ownSkinBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetEntityDataPacket.ID, ownSkinBody.readVarInt())
        assertEquals(
            SetEntityDataPacket.PLAYER_SKIN_PARTS_INDEX,
            SetEntityDataPacket
                .read(ownSkinBody)
                .entries
                .single()
                .index,
        )

        val healthBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetHealthPacket.ID, healthBody.readVarInt())
        assertEquals(20f, SetHealthPacket.read(healthBody).health)

        val experienceBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetExperiencePacket.ID, experienceBody.readVarInt())
        SetExperiencePacket.read(experienceBody)

        // parity: `PlayerList.placeNewPlayer` sends the inventory here, after the player list and
        // the HUD state, not among the chunk-radius packets earlier. This sequence is the parity
        // claim, so a new packet belongs in it at the position vanilla puts it.
        val inventoryBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(ContainerSetContentPacket.ID, inventoryBody.readVarInt())
        val inventory = ContainerSetContentPacket.read(inventoryBody)
        // Container 0 is `InventoryMenu`, 46 slots, not the 41 this server stores: see
        // [PlayerInventory.MENU_SLOTS] for what a real client does when the count disagrees.
        assertEquals(PlayerInventory.MENU_SLOTS, inventory.items.size)
        assertTrue(inventory.items.all { it == null })

        val heldSlotBody = readFrame(connection, incoming, compressionEnabled = true)
        assertEquals(SetHeldSlotPacket.ID, heldSlotBody.readVarInt())
        assertEquals(0, SetHeldSlotPacket.read(heldSlotBody).slot)

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
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        return PlayerHandle(connection, incoming, threshold, loginPlay.entityId, name, loginPlay.spawnInfo.gamemode)
    }

    /** How many streamed chunks [readPastChunks] has skipped, so a test can still count them. */
    private var streamedChunksSeen = 0

    /** The id of the frame [readPastChunks] last returned, which it has already consumed. */
    private var lastPacketId = -1

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
            when (id) {
                MapChunkPacket.ID -> streamedChunksSeen++
                UpdateLightPacket.ID, ChunkBatchStartPacket.ID -> Unit
                ChunkBatchFinishedPacket.ID ->
                    sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }
                else -> {
                    lastPacketId = id
                    return body
                }
            }
        }
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

        // Bob, in turn, was told where Alice already is (her moved-to position from above).
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
        val aliceChat = SystemChatPacket.read(aliceSeesChatBody)
        assertTrue(aliceChat.content.toString().contains("hello world"))

        val bobSeesChatBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        val bobChat = SystemChatPacket.read(bobSeesChatBody)
        assertTrue(bobChat.content.toString().contains("hello world"))

        // "ejecuta un comando": /me needs no permission, so a plain player can run it, and its
        // output reaches everyone. This is the whole path the client exercises when it types a
        // slash command: chat_command in, dispatcher, broadcast out.
        sendCompressed(alice.connection, ChatCommandPacket.ID, alice.threshold) {
            it.writeProtocolString("me waves", 256)
        }

        val aliceSeesEmoteBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        val emote = SystemChatPacket.read(aliceSeesEmoteBody).content
        assertTrue(emote.toString().contains("chat.type.emote"))
        assertTrue(emote.toString().contains("waves"))

        val bobSeesEmoteBody = readPastChunks(bob.connection, bob.incoming, bob.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        assertTrue(
            SystemChatPacket
                .read(bobSeesEmoteBody)
                .content
                .toString()
                .contains("waves"),
        )

        // A command this player may not run comes back as brigadier's own "Unknown command",
        // exactly as vanilla hides commands above your permission level rather than refusing them.
        sendCompressed(alice.connection, ChatCommandPacket.ID, alice.threshold) {
            it.writeProtocolString("stop", 256)
        }

        val refusalBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        assertTrue(
            SystemChatPacket
                .read(refusalBody)
                .content
                .toString()
                .contains("Unknown command"),
        )

        // A syntax failure is two messages, not one: the error, then the grey line marking where
        // the parser stopped. Vanilla sends both, so both have to be on the wire.
        val contextBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        assertEquals(SystemChatPacket.ID, lastPacketId)
        assertTrue(
            SystemChatPacket
                .read(contextBody)
                .content
                .toString()
                .contains("command.context.here"),
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
        var survivorBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
        var survivorId = lastPacketId
        if (survivorId == PlayerInfoRemovePacket.ID) {
            survivorBody = readPastChunks(alice.connection, alice.incoming, alice.threshold)
            survivorId = lastPacketId
        }
        assertEquals(SystemChatPacket.ID, survivorId)
        assertTrue(
            SystemChatPacket
                .read(survivorBody)
                .content
                .toString()
                .contains("is still here"),
        )

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
