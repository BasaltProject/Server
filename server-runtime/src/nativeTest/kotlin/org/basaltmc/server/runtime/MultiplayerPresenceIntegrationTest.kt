package org.basaltmc.server.runtime

import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.server.storage.StoredInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.ChatCommandPacket
import org.basaltmc.wire.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.play.RemoveEntitiesPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.RotateHeadPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * What one player sees of another, over two real sockets.
 *
 * Every assertion here is a bug that reached the game: no join message, another player invisible
 * until a relog, no held item, a head that only tipped up and down, no swing, no crouch, and a
 * player who changed world staying behind as a ghost that `/tp` teleported you to. They all shared
 * a cause (a packet nobody was sending) and none of them could fail a single-client test.
 *
 * Nothing here asserts an order. These packets come from the tick loop, from the mover's own
 * broadcast and from the tracker pairing players up, so what a test can claim is that each of them
 * arrives, not when.
 */
class MultiplayerPresenceIntegrationTest {
    /** Assigned by [startTestServer]: the server may land on a fallback port, and everything must follow it. */
    private var port = 0

    private val sword = "diamond_sword"

    @Test
    fun aSecondPlayerIsAnnouncedSpawnedAndEquipped() {
        port = startTestServer(18590)
        seedHolding("Bob", "minecraft:$sword")

        val alice = connect("Alice")
        val bob = connect("Bob")
        val swordId = requireNotNull(Items.REGISTRY.byIdentifier(Identifier.vanilla(sword))).protocolId

        alice.await(
            // Alice's own entry comes first, so this is about the one that names Bob.
            expect("Bob in the tab list", PlayerInfoUpdatePacket.ID) { body ->
                PlayerInfoUpdatePacket.read(body).entries.any { it.name == "Bob" }
            },
            // A tab entry alone leaves him invisible: he has to exist as an entity too.
            expect("Bob as an entity", SpawnEntityPacket.ID) { body ->
                SpawnEntityPacket.read(body).entityId == bob.entityId
            },
            expect("the join message", SystemChatPacket.ID) { body ->
                SystemChatPacket
                    .read(body)
                    .content
                    .toString()
                    .contains("multiplayer.player.joined")
            },
            // Without this everyone appears empty-handed whatever they are holding.
            expect("what Bob is holding", SetEquipmentPacket.ID) { body ->
                val equipment = SetEquipmentPacket.read(body)
                equipment.entityId == bob.entityId &&
                    equipment.slots.any { it.type == SetEquipmentPacket.Type.MAINHAND && it.stack?.itemId == swordId }
            },
        )

        alice.connection.close()
        bob.connection.close()
    }

    @Test
    fun aSecondPlayerTurnsSwingsAndCrouchesWhereEveryoneCanSeeIt() {
        port = startTestServer(18592)
        val alice = connect("Cleo")
        val bob = connect("Dima")
        drainUntil(alice.connection, alice.incoming, SpawnEntityPacket.ID)

        // Turning on the spot, which the body rotation alone never showed.
        sendCompressed(bob.connection, MOVE_PLAYER_ROT_ID, bob.threshold) {
            it.writeFloat(90f)
            it.writeFloat(0f)
            it.writeByte(1)
        }
        sendCompressed(bob.connection, SWING_PACKET_ID, bob.threshold) { it.writeVarInt(0) }
        sendCompressed(bob.connection, SERVERBOUND_PLAYER_INPUT_ID, bob.threshold) {
            it.writeByte(PLAYER_INPUT_SHIFT.toByte())
        }

        alice.await(
            expect("Dima's head turning", RotateHeadPacket.ID) { body ->
                val head = RotateHeadPacket.read(body)
                // A byte of yaw, so the angle arrives quantized to the nearest 360/256 of a turn.
                head.entityId == bob.entityId && head.headYaw in 88f..92f
            },
            expect("Dima's arm swinging", AnimatePacket.ID) { body ->
                val swing = AnimatePacket.read(body)
                swing.entityId == bob.entityId && swing.action == AnimatePacket.SWING_MAIN_HAND
            },
            // One bit of the shared flags, the same byte the drowning tick writes.
            expect("Dima crouching", SetEntityDataPacket.ID) { body ->
                val data = SetEntityDataPacket.read(body)
                data.entityId == bob.entityId &&
                    data.entries.any {
                        it is SetEntityDataPacket.Entry.Bytes &&
                            it.index == SetEntityDataPacket.SHARED_FLAGS_INDEX &&
                            it.value.toInt() and SetEntityDataPacket.FLAG_SNEAKING != 0
                    }
            },
        )

        bob.connection.close()
        alice.await(
            expect("Dima's entity going", RemoveEntitiesPacket.ID) { body ->
                bob.entityId in RemoveEntitiesPacket.read(body).entityIds
            },
            expect("Dima's tab entry going", PlayerInfoRemovePacket.ID) { body ->
                PlayerInfoRemovePacket.read(body).players.isNotEmpty()
            },
        )
        alice.connection.close()
    }

    @Test
    fun changingWorldMovesAPlayerOutOfSightAndBackAgain() {
        // The ghost bug: worlds were not part of who tracks whom, so someone who left for another
        // world stayed visible, frozen, and `/tp` took you to where that copy of them stood.
        port =
            startTestServer(
                18594,
                defaultPermissions = listOf("basalt.command.help", "basalt.command.me", "basalt.command.world"),
                withSecondWorld = true,
            )

        val alice = connect("Ada")
        val bob = connect("Blas")
        drainUntil(alice.connection, alice.incoming, SpawnEntityPacket.ID)

        bob.runCommand("world ${secondWorldName(port)}")
        alice.await(
            expect("Blas leaving the world", RemoveEntitiesPacket.ID) { body ->
                bob.entityId in RemoveEntitiesPacket.read(body).entityIds
            },
        )

        // And coming back puts him in front of her again, with no relog in between.
        bob.runCommand("world test_world_$port")
        alice.await(
            expect("Blas coming back", SpawnEntityPacket.ID) { body ->
                SpawnEntityPacket.read(body).entityId == bob.entityId
            },
        )

        alice.connection.close()
        bob.connection.close()
    }

    private fun connect(name: String): LoggedIn {
        val connection = TcpConnection.connect("127.0.0.1", port)
        return loginToPlay(connection, ByteBuffer(), name, OfflineUuid.forUsername(name))
    }

    /** Runs [command] and waits for the world swap it causes, so the caller knows it took effect. */
    private fun LoggedIn.runCommand(command: String) {
        sendCompressed(connection, ChatCommandPacket.ID, threshold) { ChatCommandPacket(command).write(it) }
        RespawnPacket.read(drainUntil(connection, incoming, RespawnPacket.ID))
    }

    private class Expectation(
        val description: String,
        val packetId: Int,
        val matches: (ByteBuffer) -> Boolean,
    )

    private fun expect(
        description: String,
        packetId: Int,
        matches: (ByteBuffer) -> Boolean,
    ) = Expectation(description, packetId, matches)

    /** Reads until every expectation has matched some frame, naming the ones that never did. */
    private fun LoggedIn.await(vararg expectations: Expectation) {
        val pending = expectations.toMutableList()
        // The read deadline is part of the assertion: a packet that never comes stops the traffic
        // instead of arriving late, and catching that is what turns a stalled socket into a name.
        try {
            repeat(FRAME_BUDGET) {
                if (pending.isEmpty()) return
                val body = readFrame(connection, incoming, compressionEnabled = true)
                val id = body.readVarInt()
                // One expectation per packet id in a call, since reading the body consumes it.
                val candidate = pending.firstOrNull { it.packetId == id }
                if (candidate != null && candidate.matches(body)) pending.remove(candidate)
            }
        } catch (e: IllegalStateException) {
            if (pending.isEmpty()) throw e
        }
        assertTrue(pending.isEmpty(), "$name never saw ${pending.joinToString { it.description }}")
    }

    /** A player already in the database holding [item], since a survival join starts empty-handed. */
    private fun seedHolding(
        name: String,
        item: String,
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
                    x = 8.5,
                    y = 300.0,
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
                    gamemode = StoredGamemode.SURVIVAL,
                    inventory =
                        StoredInventory(
                            List(StoredInventory.PLAYER_SLOTS) { slot ->
                                if (slot == PlayerInventory.HOTBAR_START) StoredItemStack(item, 1) else null
                            },
                        ),
                ),
            )
        }
    }
}

/** Enough frames for the chunk streaming of a join to go past without hiding what follows it. */
private const val FRAME_BUDGET = 400

// Serverbound ids this server reads straight from the buffer, so there is no packet class to ask.
private const val MOVE_PLAYER_ROT_ID = 0x20
private const val SWING_PACKET_ID = 0x3f
private const val SERVERBOUND_PLAYER_INPUT_ID = 0x2b

/** `ServerboundPlayerInputPacket`'s shift bit, which is what a modern client crouches with. */
private const val PLAYER_INPUT_SHIFT = 0x20
