package org.basaltmc.server.runtime

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.server.storage.StoredInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.wire.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.wire.protocol.handshake.ClientIntent
import org.basaltmc.wire.protocol.handshake.HandshakePacket
import org.basaltmc.wire.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.wire.protocol.login.LoginCompressionPacket
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.login.LoginStartPacket
import org.basaltmc.wire.protocol.play.BlockHitResult
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ChatCommandPacket
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockChangedAckPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.Direction
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.PalettedContainer
import org.basaltmc.wire.protocol.play.ServerboundPickItemFromBlockPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionLookPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCarriedItemPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetHeldSlotPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeProtocolString
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * One survival player over a real socket (same harness as [PlayFlowIntegrationTest]), seeded into
 * `players.db` holding dirt so this does not depend on the creative-slot path. Covers placing,
 * breaking, dropping (`Q`) and picking an item back up.
 *
 * A survival break's loot drop needs a downloaded `server.jar` this test server has not got, so
 * [RealLootTablesTest] and `LootTableTest` cover those halves instead.
 *
 * The target y is 300: y=100 came back solid dirt for seed 0, noise reaching higher than the
 * documented variation suggests. [BlockHitResult.inside] places at the target, needing no neighbour.
 */
@OptIn(ExperimentalUuidApi::class)
class BlockInteractionIntegrationTest {
    /** Assigned by [startTestServer]: the server may land on a fallback port, and everything must follow it. */
    private var port = 0

    @Test
    fun placingBreakingDroppingAndPickingUpAllRoundTripCorrectly() {
        val name = "Casey"
        val uuid = OfflineUuid.forUsername(name)
        val heldSlot = PlayerInventory.HOTBAR_START
        val emptySlot = PlayerInventory.HOTBAR_START + 1
        val tableSlot = PlayerInventory.HOTBAR_START + 2
        // Everything the client sees is addressed in container 0's own numbering, which is not
        // this server's storage order: see [PlayerInventory.MENU_SLOTS].
        val heldMenuSlot = PlayerInventory.menuSlotOf(heldSlot)
        val emptyMenuSlot = PlayerInventory.menuSlotOf(emptySlot)
        val dirtId = "minecraft:dirt"
        val expectedBlockStateId = requireNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("dirt")))
        val expectedItemProtocolId = requireNotNull(Items.REGISTRY.byIdentifier(Identifier.vanilla("dirt"))).protocolId
        val targetPos = BlockPos(8, 300, 8)
        val secondPos = BlockPos(9, 300, 8)

        // startTestServer wipes players_$port.db before the server opens it, so the seed below has
        // to happen after: seeding first would just get deleted, and Casey would join as brand new.
        port = startTestServer(18580)

        PlayerStore.open("$TEST_DATA_ROOT/players_$port.db").use { store ->
            store.players.save(
                StoredPlayer(
                    uuid = uuid.toString(),
                    name = name,
                    lastSeenEpochSeconds = 1L,
                    firstSeenEpochSeconds = 1L,
                    playTimeTicks = 0L,
                    world = "test_world_$port",
                    x = targetPos.x + 0.5,
                    y = targetPos.y.toDouble(),
                    z = targetPos.z + 0.5,
                    yaw = 0f,
                    pitch = 0f,
                    health = 20f,
                    food = 20,
                    saturation = 5f,
                    exhaustion = 0f,
                    xpLevel = 0,
                    xpProgress = 0f,
                    xpTotal = 0,
                    gamemode = StoredGamemode.SURVIVAL,
                    inventory =
                        StoredInventory(
                            List(StoredInventory.PLAYER_SLOTS) { slot ->
                                when (slot) {
                                    heldSlot -> StoredItemStack(dirtId, 64)
                                    tableSlot -> StoredItemStack("minecraft:crafting_table", 1)
                                    else -> null
                                }
                            },
                        ),
                ),
            )
        }

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = login(connection, incoming, name)

        // Drain everything up to (and including) the seeded inventory, to confirm the seed itself
        // reached the client: if this assertion fails, nothing downstream can be trusted either.
        val startingInventory =
            ContainerSetContentPacket.read(drainUntil(connection, incoming, ContainerSetContentPacket.ID))
        assertEquals(64, startingInventory.items[heldMenuSlot]?.count)
        assertEquals(expectedItemProtocolId, startingInventory.items[heldMenuSlot]?.itemId)
        drainUntil(connection, incoming, SetHeldSlotPacket.ID)

        // The server streams the whole spawn-radius chunk grid right after this (49 chunks at
        // viewDistance 3), unprompted. A real client reads it as it arrives; this test has to as
        // well, or the OS socket buffer for this connection fills up with unread chunk bytes and
        // the *server's* next write (the block-update broadcast below) blocks forever waiting for
        // room in it.
        val chunkBatchFinished =
            ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        assertEquals(49, chunkBatchFinished.batchSize)
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        // The stored row's y is not actually restored on join (a separate, real bug found while
        // writing this test: the player comes back near the world's default spawn y instead), so
        // pickup range would never line up with the drop below without this. Not what is under
        // test here, so worked around rather than chased: a plain move to where the test needs to
        // stand, which the server already has a passing test for elsewhere.
        sendCompressed(connection, ServerboundPositionPacket.ID, threshold) {
            it.writeDouble(targetPos.x + 0.5)
            it.writeDouble(targetPos.y.toDouble())
            it.writeDouble(targetPos.z + 0.5)
            it.writeByte(1)
        }

        // --- Place ---
        sendCompressed(connection, ServerboundUseItemOnPacket.ID, threshold) {
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.MAIN_HAND,
                blockHit =
                    BlockHitResult(
                        targetPos,
                        Direction.UP,
                        0.5f,
                        0.5f,
                        0.5f,
                        inside = true,
                        worldBorderHit = false,
                    ),
                sequence = 1,
            ).write(it)
        }

        val placedAck =
            ClientboundBlockChangedAckPacket.read(drainUntil(connection, incoming, ClientboundBlockChangedAckPacket.ID))
        assertEquals(1, placedAck.sequence)

        // Wire order matches ClientSession.handleUseItemOn exactly: the inventory slot update is
        // sent before the block-update broadcast, not after.
        val afterPlaceSlot = ContainerSetSlotPacket.read(drainUntil(connection, incoming, ContainerSetSlotPacket.ID))
        assertEquals(heldMenuSlot, afterPlaceSlot.slot)
        assertEquals(63, afterPlaceSlot.item?.count)

        val placedUpdate =
            ClientboundBlockUpdatePacket.read(drainUntil(connection, incoming, ClientboundBlockUpdatePacket.ID))
        assertEquals(targetPos, placedUpdate.pos)
        assertEquals(
            expectedBlockStateId,
            placedUpdate.blockStateId,
            "placing the held item produced state ${placedUpdate.blockStateId} " +
                "(${Blocks.byStateId(placedUpdate.blockStateId)?.block}), not dirt ($expectedBlockStateId)",
        )

        // --- Break, in survival: the client only sends STOP_DESTROY_BLOCK, after timing the dig itself ---
        sendCompressed(connection, ServerboundPlayerActionPacket.ID, threshold) {
            ServerboundPlayerActionPacket(
                action = ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK,
                pos = targetPos,
                direction = Direction.UP,
                sequence = 2,
            ).write(it)
        }

        val brokenAck =
            ClientboundBlockChangedAckPacket.read(drainUntil(connection, incoming, ClientboundBlockChangedAckPacket.ID))
        assertEquals(2, brokenAck.sequence)

        val brokenUpdate =
            ClientboundBlockUpdatePacket.read(drainUntil(connection, incoming, ClientboundBlockUpdatePacket.ID))
        assertEquals(targetPos, brokenUpdate.pos)
        assertEquals(Blocks.AIR.stateId, brokenUpdate.blockStateId)

        // A floor to drop onto. Items obey gravity now, so without something solid under the
        // player the dropped stack simply falls out of pickup range and keeps going, which is
        // correct behaviour and useless for testing a pickup.
        sendCompressed(connection, ServerboundUseItemOnPacket.ID, threshold) {
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.MAIN_HAND,
                blockHit =
                    BlockHitResult(
                        BlockPos(targetPos.x, targetPos.y - 1, targetPos.z),
                        Direction.UP,
                        0.5f,
                        0.5f,
                        0.5f,
                        inside = true,
                        worldBorderHit = false,
                    ),
                sequence = 3,
            ).write(it)
        }
        drainUntil(connection, incoming, ClientboundBlockUpdatePacket.ID)

        // --- Drop the held stack with Q: unlike a survival break's drop, this never touches a
        // loot table (it is just whatever is already in hand), so it is fully testable here.
        //
        // Looking straight down first, on purpose: a toss follows the look direction now, and a
        // level throw lands the item a block or two ahead, outside pickup range, which is exactly
        // what vanilla does and not what this test wants to measure. Straight down puts it at the
        // player's feet so the pickup below is about the pickup and not about the arc. ---
        sendCompressed(connection, ServerboundPositionLookPacket.ID, threshold) {
            it.writeDouble(targetPos.x + 0.5)
            it.writeDouble(targetPos.y.toDouble())
            it.writeDouble(targetPos.z + 0.5)
            it.writeFloat(0f)
            it.writeFloat(90f)
            it.writeByte(1)
        }

        sendCompressed(connection, ServerboundPlayerActionPacket.ID, threshold) {
            ServerboundPlayerActionPacket(
                action = ServerboundPlayerActionPacket.Action.DROP_ITEM,
                pos = BlockPos(0, 0, 0),
                direction = Direction.DOWN,
                sequence = 3,
            ).write(it)
        }

        val afterDropSlot = ContainerSetSlotPacket.read(drainUntil(connection, incoming, ContainerSetSlotPacket.ID))
        assertEquals(heldMenuSlot, afterDropSlot.slot)
        assertEquals(61, afterDropSlot.item?.count)

        val spawn = SpawnEntityPacket.read(drainUntil(connection, incoming, SpawnEntityPacket.ID))
        assertEquals(SpawnEntityPacket.ITEM_ENTITY_TYPE, spawn.entityType)

        val metadata = SetEntityDataPacket.read(drainUntil(connection, incoming, SetEntityDataPacket.ID))
        assertEquals(spawn.entityId, metadata.entityId)
        val itemEntry = metadata.entries.single() as SetEntityDataPacket.Entry.Item
        assertEquals(expectedItemProtocolId, itemEntry.stack?.itemId)
        assertEquals(1, itemEntry.stack?.count)

        // Picking it back up is deliberately *not* asserted here. It now depends on item physics
        // and a 40-tick pickup delay, both of which are timing, and a socket-level test that waits
        // on them measures the scheduler as much as the server. [WorldStateItemTest] covers the
        // fall, the settling, the despawn and the pickup delay directly and deterministically.

        // --- Pick block: place a second dirt block with the dirt still in slot 0, switch to the
        // empty slot 1, then middle-click the block just placed. Survival pick-block only swaps in
        // an item the player already carries (vanilla cannot conjure one from nothing), which is
        // exactly what this exercises: dirt is still in slot 0, not the now-selected slot 1. ---
        sendCompressed(connection, ServerboundUseItemOnPacket.ID, threshold) {
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.MAIN_HAND,
                blockHit =
                    BlockHitResult(
                        secondPos,
                        Direction.UP,
                        0.5f,
                        0.5f,
                        0.5f,
                        inside = true,
                        worldBorderHit = false,
                    ),
                sequence = 4,
            ).write(it)
        }
        drainUntil(connection, incoming, ContainerSetSlotPacket.ID)
        val secondPlaceUpdate =
            ClientboundBlockUpdatePacket.read(drainUntil(connection, incoming, ClientboundBlockUpdatePacket.ID))
        assertEquals(secondPos, secondPlaceUpdate.pos)
        assertEquals(expectedBlockStateId, secondPlaceUpdate.blockStateId)

        sendCompressed(connection, ServerboundSetCarriedItemPacket.ID, threshold) {
            ServerboundSetCarriedItemPacket(1).write(it)
        }
        sendCompressed(connection, ServerboundPickItemFromBlockPacket.ID, threshold) {
            ServerboundPickItemFromBlockPacket(secondPos, includeData = false).write(it)
        }

        // parity: `ServerGamePacketListenerImpl.tryPickItem`. The dirt is already in hotbar slot 0,
        // so vanilla moves the *selection* back to it and touches no slot at all: it neither
        // duplicates the stack into the empty slot nor conjures a second one.
        val heldAfterPick = SetHeldSlotPacket.read(drainUntil(connection, incoming, SetHeldSlotPacket.ID))
        assertEquals(0, heldAfterPick.slot, "pick block should select the slot already holding the item")

        val pickedInventory =
            ContainerSetContentPacket.read(
                drainUntil(connection, incoming, ContainerSetContentPacket.ID),
            )
        assertEquals(
            expectedItemProtocolId,
            pickedInventory.items[heldMenuSlot]?.itemId,
            "the stack pick block selected should still be where it already was",
        )
        assertNull(pickedInventory.items[emptyMenuSlot], "pick block must not duplicate the stack into a second slot")

        // --- Place a crafting table, then right-click it with an empty hand and expect its screen.
        //
        // The empty hand is the point. An earlier version checked for a placeable item *before* it
        // looked at what was clicked, so the usual way of opening a table (empty hand) was rejected
        // before the block was ever considered and the screen simply never appeared. ---
        val tablePos = BlockPos(10, 300, 8)
        sendCompressed(connection, ServerboundSetCarriedItemPacket.ID, threshold) {
            ServerboundSetCarriedItemPacket(2).write(it)
        }
        sendCompressed(connection, ServerboundUseItemOnPacket.ID, threshold) {
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.MAIN_HAND,
                blockHit =
                    BlockHitResult(
                        tablePos,
                        Direction.UP,
                        0.5f,
                        0.5f,
                        0.5f,
                        inside = true,
                        worldBorderHit = false,
                    ),
                sequence = 6,
            ).write(it)
        }
        val tablePlaced =
            ClientboundBlockUpdatePacket.read(drainUntil(connection, incoming, ClientboundBlockUpdatePacket.ID))
        assertEquals(tablePos, tablePlaced.pos)
        assertEquals(
            Blocks.REGISTRY.idOf(Identifier.vanilla("crafting_table")),
            tablePlaced.blockStateId,
        )

        // Hotbar slot 2 is empty now that the only table was placed, so this really is an empty hand.
        sendCompressed(connection, ServerboundUseItemOnPacket.ID, threshold) {
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.MAIN_HAND,
                blockHit =
                    BlockHitResult(
                        tablePos,
                        Direction.UP,
                        0.5f,
                        0.5f,
                        0.5f,
                        inside = false,
                        worldBorderHit = false,
                    ),
                sequence = 7,
            ).write(it)
        }

        val opened = OpenScreenPacket.read(drainUntil(connection, incoming, OpenScreenPacket.ID))
        assertEquals(OpenScreenPacket.CRAFTING_MENU, opened.menuType)
        assertTrue(opened.containerId != 0, "a block's window must not reuse the player's own container id")

        // The window that follows is the table's, and it is addressed by that same container id.
        val tableWindow =
            ContainerSetContentPacket.read(drainUntil(connection, incoming, ContainerSetContentPacket.ID))
        assertEquals(opened.containerId, tableWindow.containerId)
        assertEquals(46, tableWindow.items.size)

        connection.close()
    }

    /**
     * A single water block, and what the client is handed for it.
     *
     * The client decides on its own whether its head is in water: it needs the block state at that
     * position and, before it even looks, a section whose fluid count says there is fluid in it
     * (`LevelChunkSection.hasFluid`, read straight off the two shorts this server writes). Get
     * either wrong and there is no overlay, no sound and no air bar, however much water renders.
     */
    @Test
    fun aSingleWaterBlockReachesTheClientAsWaterInASectionThatDeclaresFluid() {
        port = startTestServer(18583, defaultPermissions = listOf("*"))
        val waterPos = BlockPos(8, 100, 8)

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = login(connection, incoming, "Wade")
        val batch = ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        assertEquals(49, batch.batchSize)
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        sendCompressed(connection, ChatCommandPacket.ID, threshold) {
            it.writeProtocolString("setblock ${waterPos.x} ${waterPos.y} ${waterPos.z} minecraft:water", 256)
        }
        val update =
            ClientboundBlockUpdatePacket.read(
                drainUntil(connection, incoming, ClientboundBlockUpdatePacket.ID),
            )
        assertEquals(waterPos, update.pos)
        assertEquals(Blocks.WATER.stateId, update.blockStateId, "the client was not told this block is water")

        // A second client gets the chunk itself, which is the path a player who logs in later, or
        // walks back into view, takes. The block update above never carries the counts: they ride
        // in the chunk, and this is where they were wrong.
        val second = TcpConnection.connect("127.0.0.1", port)
        val secondIncoming = ByteBuffer()
        login(second, secondIncoming, "Wanda")
        var found = false
        var frames = 0
        while (frames++ < 400 && !found) {
            val body = readFrame(second, secondIncoming, compressionEnabled = true)
            val id = body.readVarInt()
            if (id == ChunkBatchFinishedPacket.ID) break
            if (id != MapChunkPacket.ID) continue
            val chunk = MapChunkPacket.read(body)
            if (chunk.x != waterPos.x shr 4 || chunk.z != waterPos.z shr 4) continue
            val sections = ByteBuffer.wrap(chunk.chunkData)
            val sectionIndex = (waterPos.y - ChunkHeight.OVERWORLD.minY) / 16
            for (index in 0..sectionIndex) {
                val nonAir = sections.readShort().toInt()
                val fluid = sections.readShort().toInt()
                val states = PalettedContainer.read(sections, 4096, PalettedContainer.BLOCK_MAX_INDIRECT_BITS)
                PalettedContainer.read(sections, 64, PalettedContainer.BIOME_MAX_INDIRECT_BITS)
                if (index != sectionIndex) continue
                val localY = waterPos.y - ChunkHeight.OVERWORLD.minY - index * 16
                val state = states[((localY * 16) + (waterPos.z and 15)) * 16 + (waterPos.x and 15)]
                assertEquals(Blocks.WATER.stateId, state, "the chunk does not carry the water block")
                assertTrue(nonAir > 0, "the section says it is empty")
                assertTrue(fluid > 0, "the section says it holds no fluid, so the client never looks for water in it")
                found = true
            }
        }
        assertTrue(found, "the chunk holding the water block never arrived")

        // Using an item with no block under the crosshair has to be acknowledged even when the
        // server does nothing with it: that acknowledgement is what makes the client drop the
        // change it already made to its own copy of the world.
        sendCompressed(connection, ServerboundUseItemPacket.ID, threshold) {
            ServerboundUseItemPacket(ServerboundUseItemOnPacket.Hand.MAIN_HAND, 77, 0f, 0f).write(it)
        }
        val ack =
            ClientboundBlockChangedAckPacket.read(
                drainUntil(connection, incoming, ClientboundBlockChangedAckPacket.ID),
            )
        assertEquals(77, ack.sequence)

        second.close()
        connection.close()
    }
}

/** Reads and discards frames until one with this packet id arrives, returning its body with the id already consumed. */
private fun drainUntil(
    connection: TcpConnection,
    incoming: ByteBuffer,
    packetId: Int,
): ByteBuffer {
    var seen = 0
    repeat(400) {
        val body = readFrame(connection, incoming, compressionEnabled = true)
        seen++
        val id = body.readVarInt()
        if (id == packetId) return body
    }
    error("packet 0x${packetId.toString(16)} never arrived after $seen other frames")
}

/** The Login+Configuration handshake, stripped to just what is needed to reach Play. */
private fun login(
    connection: TcpConnection,
    incoming: ByteBuffer,
    name: String,
): Int {
    sendUncompressed(connection, HandshakePacket.ID) {
        HandshakePacket(776, "127.0.0.1", 0, ClientIntent.LOGIN).write(it)
    }
    sendUncompressed(connection, LoginStartPacket.ID) {
        LoginStartPacket(name, Uuid.fromLongs(0L, 0L)).write(it)
    }

    val compressionBody = readFrame(connection, incoming, compressionEnabled = false)
    check(compressionBody.readVarInt() == LoginCompressionPacket.ID)
    val threshold = LoginCompressionPacket.read(compressionBody).threshold

    check(readFrame(connection, incoming, compressionEnabled = true).readVarInt() == LoginFinishedPacket.ID)
    sendCompressed(connection, LoginAcknowledgedPacket.ID, threshold) { }

    drainUntil(connection, incoming, SelectKnownPacksPacket.CLIENTBOUND_ID)
    sendCompressed(connection, SelectKnownPacksPacket.SERVERBOUND_ID, threshold) {
        SelectKnownPacksPacket(emptyList()).write(it)
    }

    drainUntil(connection, incoming, FinishConfigurationPacket.ID)
    sendCompressed(connection, FinishConfigurationPacket.ID, threshold) { }

    drainUntil(connection, incoming, LoginPlayPacket.ID)
    return threshold
}
