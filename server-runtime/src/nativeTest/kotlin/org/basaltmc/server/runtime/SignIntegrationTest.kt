package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.server.storage.StoredInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.protocol.play.BlockEntityDataPacket
import org.basaltmc.wire.protocol.play.BlockHitResult
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.Direction
import org.basaltmc.wire.protocol.play.OpenSignEditorPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCarriedItemPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.wire.protocol.play.SignUpdatePacket
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Placing a sign, writing on it and having the other player read it, over real sockets.
 *
 * Three packets that did not exist until now: without them a sign is a blank board nobody can type
 * on, which is what the packet audit had it down as.
 */
class SignIntegrationTest {
    private var port = 0

    @Test
    fun aSignIsPlacedWrittenOnAndSeenByEveryone() {
        val name = "Sam"
        val target = BlockPos(8, 300, 8)
        port = startTestServer(18596)
        seedSignHolder(name, target)

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name, OfflineUuid.forUsername(name)).threshold
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        // The onlooker joins first, so the write below has somebody else to reach.
        val watcher = TcpConnection.connect("127.0.0.1", port)
        val watcherIncoming = ByteBuffer()
        val watcherThreshold = loginToPlay(watcher, watcherIncoming, "Watcher").threshold
        // Drained to the end of its first batch before anything else happens. Reads here are one at
        // a time, so a socket left with a view distance of chunks queued on it is one this test
        // cannot come back to quickly, and the sign update would be waiting behind all of it.
        ChunkBatchFinishedPacket.read(drainUntil(watcher, watcherIncoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(watcher, ChunkBatchReceivedPacket.ID, watcherThreshold) { it.writeFloat(16f) }

        // A floor first: a sign needs something under it now, so placing one in mid-air is refused.
        sendCompressed(connection, ServerboundUseItemOnPacket.ID, threshold) {
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.MAIN_HAND,
                blockHit =
                    BlockHitResult(
                        target,
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
        sendCompressed(connection, ServerboundSetCarriedItemPacket.ID, threshold) {
            ServerboundSetCarriedItemPacket(1).write(it)
        }
        sendCompressed(connection, ServerboundUseItemOnPacket.ID, threshold) {
            ServerboundUseItemOnPacket(
                hand = ServerboundUseItemOnPacket.Hand.MAIN_HAND,
                blockHit =
                    BlockHitResult(target, Direction.UP, 0.5f, 1f, 0.5f, inside = false, worldBorderHit = false),
                sequence = 2,
            ).write(it)
        }

        // Placing one opens its editor, which is the only way a client will let anyone type.
        val signPos = BlockPos(target.x, target.y + 1, target.z)
        val editor = OpenSignEditorPacket.read(drainUntil(connection, incoming, OpenSignEditorPacket.ID))
        assertEquals(signPos, editor.pos)
        assertTrue(editor.isFrontText)

        val lines = listOf("hola", "que tal", "", "adios")
        sendCompressed(connection, SignUpdatePacket.ID, threshold) {
            SignUpdatePacket(signPos, isFrontText = true, lines = lines).write(it)
        }

        // The writer reads its own copy first, and it has to: the broadcast runs on this
        // connection's read thread and writes to every player in the world, so leaving this socket
        // unread while waiting on the other one parks the very thread that would answer it.
        val own =
            BlockEntityDataPacket.read(drainUntil(connection, incoming, BlockEntityDataPacket.ID, limit = 4000))
        assertEquals(signPos, own.pos)

        // And it reaches the onlooker, which is the half a single client could never show.
        val data =
            BlockEntityDataPacket.read(
                drainUntil(watcher, watcherIncoming, BlockEntityDataPacket.ID, limit = 4000),
            )
        assertEquals(signPos, data.pos)
        val front = checkNotNull(data.tag?.get("front_text")) as NbtCompound
        val messages = checkNotNull(front["messages"]) as NbtList
        assertEquals(lines, messages.map { (it as NbtString).value })

        connection.close()
        watcher.close()
    }

    private fun seedSignHolder(
        name: String,
        at: BlockPos,
    ) {
        PlayerStore.open("$TEST_DATA_ROOT/players_$port.db").use { store ->
            store.players.save(
                StoredPlayer(
                    uuid = OfflineUuid.forUsername(name).toString(),
                    name = name,
                    lastSeenEpochSeconds = 1L,
                    firstSeenEpochSeconds = 1L,
                    playTimeTicks = 0L,
                    world = "test_world_$port",
                    x = at.x + 2.5,
                    y = at.y.toDouble(),
                    z = at.z + 0.5,
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
                                    PlayerInventory.HOTBAR_START -> StoredItemStack("minecraft:dirt", 1)
                                    PlayerInventory.HOTBAR_START + 1 -> StoredItemStack("minecraft:oak_sign", 1)
                                    else -> null
                                }
                            },
                        ),
                ),
            )
        }
    }
}
