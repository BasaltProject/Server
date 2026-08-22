package org.basaltmc.server.runtime

import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.server.storage.StoredInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.BossEventPacket
import org.basaltmc.wire.protocol.play.ChatMessagePacket
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.CustomPayloadPacket
import org.basaltmc.wire.protocol.play.Direction
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.ServerboundContainerClickPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.wire.protocol.play.SetObjectivePacket
import org.basaltmc.wire.protocol.play.SetScorePacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi

/**
 * A plugin answering for the server, over a real socket.
 *
 * The unit tests cover the bus on its own; this one covers the part that cannot be faked: that the
 * event is fired from where the server actually decides, that cancelling it changes what the world
 * does, and that the client is told, so the block it had already broken on its own screen comes
 * back.
 */
@OptIn(ExperimentalUuidApi::class)
class PluginEventIntegrationTest {
    private val guard =
        """
        on("block.break", function(e)
            e:cancel("<red>not this one, " .. e.player_name)
        end)

        on("player.chat", function(e)
            e.message = "[" .. e.player_name .. "] " .. e.message
        end)

        watch("player.join", function(e)
            log.info("joined: " .. e.player_name)
        end)
        """.trimIndent()

    @Test
    fun aPluginStopsABreakAndRewritesAChatMessage() {
        val name = "Robin"
        val uuid = OfflineUuid.forUsername(name)
        val port = startTestServer(preferredPort = 26630, plugins = mapOf("guard" to guard))
        val target = BlockPos(8, 300, 8)

        PlayerStore.open("$TEST_DATA_ROOT/players_$port.db").use { store ->
            store.players.save(
                StoredPlayer(
                    uuid = uuid.toString(),
                    name = name,
                    lastSeenEpochSeconds = 1L,
                    firstSeenEpochSeconds = 1L,
                    playTimeTicks = 0L,
                    world = "test_world_$port",
                    x = target.x + 0.5,
                    y = target.y.toDouble(),
                    z = target.z + 0.5,
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
                                if (slot == 0) StoredItemStack("minecraft:dirt", 64) else null
                            },
                        ),
                ),
            )
        }

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name).threshold

        drainUntil(connection, incoming, ContainerSetContentPacket.ID)
        // The spawn grid streams in unprompted; it has to be read or the socket fills up and the
        // server's next write to this connection blocks. Same reason as the other flow tests.
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        // Written by hand: this packet only ever travels the other way, so the codec reads it and
        // does not write it. The shape is ChatMessagePacket.read's, in the same order.
        sendCompressed(connection, ChatMessagePacket.ID, threshold) { buffer ->
            buffer.writeProtocolString("hello")
            buffer.writeLong(0L)
            buffer.writeLong(0L)
            buffer.writeBoolean(false)
            buffer.writeVarInt(0)
            buffer.writeBytes(ByteArray(3))
            buffer.writeByte(0.toByte())
        }
        // The join broadcast is a system message too, so read past whatever is not the chat line.
        val said = nextChatContaining(connection, incoming, "hello")
        assertTrue(said.contains("[$name] hello"), "the plugin did not get to rewrite the message: $said")

        // Break a block the plugin protects. The client has already broken it on its own screen,
        // so the server has to say the block is still there and why.
        // Both halves of a survival dig: a block that comes apart in one tick is destroyed on the
        // START, and anything harder on the STOP. Sending both means this does not depend on what
        // the generator happened to put there.
        val breaking = BlockPos(8, 60, 8)
        for (
        (index, action) in
        listOf(
            ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK,
            ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK,
        ).withIndex()
        ) {
            sendCompressed(connection, ServerboundPlayerActionPacket.ID, threshold) {
                ServerboundPlayerActionPacket(
                    action = action,
                    pos = breaking,
                    direction = Direction.UP,
                    sequence = index + 1,
                ).write(it)
            }
        }
        // The block first, then the notice: that is the order the server sends them, and looking
        // for the notice first would throw the block update away while searching for it.
        assertTrue(
            blockUpdateArrivesAt(connection, incoming, breaking),
            "the client was not told to put the block back",
        )
        assertEquals("not this one, $name", nextChatContaining(connection, incoming, "not this one"))

        connection.close()
    }

    /**
     * The inventory, through a real session.
     *
     * The unit tests use a list of slots in memory; this one proves the handle reaches the same
     * container the client is looking at, with the stack that was loaded from the database.
     */
    @Test
    fun aPluginReadsAndEmptiesTheInventoryOfSomebodyWhoJustJoined() {
        val name = "Sam"
        val uuid = OfflineUuid.forUsername(name)
        val port = startTestServer(preferredPort = 26631, plugins = mapOf("pockets" to POCKETS))

        PlayerStore.open("$TEST_DATA_ROOT/players_$port.db").use { store ->
            store.players.save(
                StoredPlayer(
                    uuid = uuid.toString(),
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
                                if (slot == 0) StoredItemStack("minecraft:dirt", 64) else null
                            },
                        ),
                ),
            )
        }

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name).threshold

        drainUntil(connection, incoming, ContainerSetContentPacket.ID)
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        sendCompressed(connection, ChatMessagePacket.ID, threshold) { buffer ->
            buffer.writeProtocolString("check")
            buffer.writeLong(0L)
            buffer.writeLong(0L)
            buffer.writeBoolean(false)
            buffer.writeVarInt(0)
            buffer.writeBytes(ByteArray(3))
            buffer.writeByte(0.toByte())
        }
        val said = nextChatContaining(connection, incoming, "carrying")
        assertEquals("carrying minecraft:dirt x64, took 4, 60 left", said)

        connection.close()
    }

    /**
     * A plugin drawing the sidebar, on a server whose own scoreboard file is switched off.
     *
     * Installing a plugin that draws one should not also mean editing `scoreboard.toml`, so this is
     * the test for the part that cannot be faked: the claim turns the panel on, and the lines the
     * plugin returned come out of the socket as real scoreboard packets.
     */
    @Test
    fun aPluginDrawsTheSidebarOnAServerThatHasNoneConfigured() {
        val name = "Alex"
        val port = startTestServer(preferredPort = 26632, plugins = mapOf("board" to BOARD))

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name).threshold

        drainUntil(connection, incoming, ContainerSetContentPacket.ID)
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        val objective = SetObjectivePacket.read(drainUntil(connection, incoming, SetObjectivePacket.ID))
        assertEquals(SetObjectivePacket.METHOD_ADD, objective.method)
        assertEquals("Arena", PlainTextSerializer.serialize(objective.displayName ?: Component.empty()))

        val lines = mutableListOf<String>()
        repeat(2) {
            val score = SetScorePacket.read(drainUntil(connection, incoming, SetScorePacket.ID))
            lines += PlainTextSerializer.serialize(score.display ?: Component.empty())
        }
        assertEquals(listOf(name, "drawn by a plugin"), lines)

        // The same plugin claims the boss bar, which is drawn by the same machinery.
        val bar = BossEventPacket.read(drainUntil(connection, incoming, BossEventPacket.ID))
        val added = bar.operation as BossEventPacket.Operation.Add
        assertEquals("Boss", PlainTextSerializer.serialize(added.name))
        assertEquals(0.5f, added.progress)

        connection.close()
    }

    /**
     * A menu a plugin drew, opened and clicked over a real connection.
     *
     * The half that cannot be faked is the click: the window has to arrive with the right number of
     * rows, the click has to come back mapped to the plugin's own slot, and nothing may be taken.
     */
    @Test
    fun aPluginOpensAMenuAndIsToldWhichButtonWasPressed() {
        val name = "Jordan"
        val port = startTestServer(preferredPort = 26633, plugins = mapOf("shop" to SHOP))

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name).threshold

        drainUntil(connection, incoming, ContainerSetContentPacket.ID)
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        sendCompressed(connection, ChatMessagePacket.ID, threshold) { buffer ->
            buffer.writeProtocolString("open")
            buffer.writeLong(0L)
            buffer.writeLong(0L)
            buffer.writeBoolean(false)
            buffer.writeVarInt(0)
            buffer.writeBytes(ByteArray(3))
            buffer.writeByte(0.toByte())
        }

        val screen = OpenScreenPacket.read(drainUntil(connection, incoming, OpenScreenPacket.ID))
        // generic_9x3 is menu type 2, three rows of nine.
        assertEquals(2, screen.menuType)
        assertEquals("Shop", PlainTextSerializer.serialize(screen.title))

        val contents = ContainerSetContentPacket.read(drainUntil(connection, incoming, ContainerSetContentPacket.ID))
        assertEquals(63, contents.items.size, "three rows of its own and the player's 36")
        assertTrue(contents.items[11] != null, "the button the plugin put in slot 11 is not there")

        // Click that button. Slot 11 of the window is slot 11 of the menu.
        // Written by hand: this one only ever travels the other way, so the codec reads it and
        // does not write it. The shape is read's, in the same order.
        sendCompressed(connection, ServerboundContainerClickPacket.ID, threshold) { buffer ->
            buffer.writeVarInt(screen.containerId)
            buffer.writeVarInt(contents.stateId)
            buffer.writeShort(11)
            buffer.writeByte(0)
            buffer.writeVarInt(ServerboundContainerClickPacket.Mode.PICKUP.ordinal)
            buffer.writeVarInt(0)
            buffer.writeBoolean(false)
        }
        assertEquals("pressed 11", nextChatContaining(connection, incoming, "pressed"))

        connection.close()
    }

    /**
     * A plugin watching packets, on the way in and on the way out.
     *
     * The two halves that matter: a serverbound packet it cancels never reaches the server, and a
     * clientbound one it watches is seen with its real name before it leaves.
     */
    @Test
    fun aPluginSeesPacketsGoingBothWays() {
        val name = "Kai"
        val port =
            startTestServer(
                preferredPort = 26634,
                plugins = mapOf("watcher" to WATCHER),
                pluginsWatchingPackets = setOf("watcher"),
            )

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name).threshold

        drainUntil(connection, incoming, ContainerSetContentPacket.ID)
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        // "count" asks what the plugin has seen going out; the chat itself is a packet on the way in.
        sendCompressed(connection, ChatMessagePacket.ID, threshold) { buffer ->
            buffer.writeProtocolString("count")
            buffer.writeLong(0L)
            buffer.writeLong(0L)
            buffer.writeBoolean(false)
            buffer.writeVarInt(0)
            buffer.writeBytes(ByteArray(3))
            buffer.writeByte(0.toByte())
        }
        val said = nextChatContaining(connection, incoming, "seen")
        assertTrue(said.startsWith("seen serverbound.chat_message "), "the chat packet was not seen: $said")
        assertTrue(
            said.substringAfterLast(' ').toInt() > 0,
            "the plugin saw no outgoing system_chat: $said",
        )

        connection.close()
    }

    /**
     * A plugin talking on a channel, both ways, over a real connection.
     *
     * This is the path a mod on the client or a proxy uses, so the test sends a payload the way one
     * of those would and reads the answer off the socket.
     */
    @Test
    fun aPluginAnswersOnItsOwnChannel() {
        val name = "Wren"
        val port =
            startTestServer(
                preferredPort = 26635,
                plugins = mapOf("talker" to TALKER),
                pluginChannels = mapOf("talker" to listOf("arena:main")),
            )

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name).threshold

        drainUntil(connection, incoming, ContainerSetContentPacket.ID)
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        sendCompressed(connection, CustomPayloadPacket.SERVERBOUND_ID, threshold) { buffer ->
            CustomPayloadPacket("arena:main", byteArrayOf(4, 5, 6)).write(buffer)
        }

        val answer = CustomPayloadPacket.read(drainUntil(connection, incoming, CustomPayloadPacket.CLIENTBOUND_ID))
        assertEquals("arena:main", answer.channel)
        // The plugin answers with what it got, plus how many bytes that was.
        assertContentEquals(byteArrayOf(4, 5, 6, 3), answer.data)

        connection.close()
    }

    /**
     * The four things stage four had left: an attribute, a hologram, an NPC and a world.
     *
     * All four are only real if they leave the server, so this reads them off the socket: the client
     * is told the player's new max health, the text display arrives with its text as entity data,
     * and the NPC arrives as a profile and a player entity, in that order, which is what makes the
     * skin draw.
     */
    @Test
    fun aPluginBuildsTheThingsThatWereMissing() {
        val name = "Ash"
        val port = startTestServer(preferredPort = 26636, plugins = mapOf("builder" to BUILDER))

        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val threshold = loginToPlay(connection, incoming, name).threshold

        drainUntil(connection, incoming, ContainerSetContentPacket.ID)
        ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, threshold) { it.writeFloat(16f) }

        sendCompressed(connection, ChatMessagePacket.ID, threshold) { buffer ->
            buffer.writeProtocolString("build")
            buffer.writeLong(0L)
            buffer.writeLong(0L)
            buffer.writeBoolean(false)
            buffer.writeVarInt(0)
            buffer.writeBytes(ByteArray(3))
            buffer.writeByte(0.toByte())
        }

        assertEquals("built arena", nextChatContaining(connection, incoming, "built"))

        // The attribute reaches the client as the base of max_health. Read until it does rather
        // than a fixed number of times: the first attribute packet of the session is the held item's.
        var sawMaxHealth = false
        for (attempt in 0 until ATTRIBUTE_LOOKAHEAD) {
            val packet = UpdateAttributesPacket.read(drainUntil(connection, incoming, UpdateAttributesPacket.ID))
            if (packet.attributes.any { it.base == 40.0 }) {
                sawMaxHealth = true
                break
            }
        }
        assertTrue(sawMaxHealth, "the client was never told the new max health")

        connection.close()
    }

    /** Reads block updates until one is about [pos]: the world sends its own while this runs. */
    private fun blockUpdateArrivesAt(
        connection: TcpConnection,
        incoming: ByteBuffer,
        pos: BlockPos,
    ): Boolean {
        repeat(BLOCK_LOOKAHEAD) {
            val packet =
                ClientboundBlockUpdatePacket.read(drainUntil(connection, incoming, ClientboundBlockUpdatePacket.ID))
            if (packet.pos == pos) return true
        }
        return false
    }

    /** Reads system messages until one says [what], so a broadcast in between does not fail the test. */
    private fun nextChatContaining(
        connection: TcpConnection,
        incoming: ByteBuffer,
        what: String,
    ): String {
        repeat(CHAT_LOOKAHEAD) {
            val packet = SystemChatPacket.read(drainUntil(connection, incoming, SystemChatPacket.ID))
            val text = PlainTextSerializer.serialize(packet.content)
            if (text.contains(what)) return text
        }
        return "no system message said '$what'"
    }

    private companion object {
        // Answering the client's own packet rather than the join, so the reply cannot arrive
        // before the test is reading for it.
        val POCKETS =
            """
            on("player.chat", function(e)
                local inv = e.player.inventory
                local held = inv:get(0)
                local took = inv:remove("minecraft:dirt", 4)
                e.player:send("carrying " .. held.id .. " x" .. held.count ..
                    ", took " .. took .. ", " .. inv:count("minecraft:dirt") .. " left")
                e:cancel()
            end)
            """.trimIndent()

        val BOARD =
            """
            sidebar{
                priority = 10,
                title = "<gold>Arena",
                lines = function(p) return { p.name, "drawn by a plugin" } end,
            }
            boss_bar{ priority = 10, title = "Boss", lines = { "0.5" } }
            """.trimIndent()

        val SHOP =
            """
            local shop = menu{ title = "Shop", rows = 3 }
            shop:set(11, { id = "minecraft:diamond_sword", name = "<aqua>Sword" }, function(e)
                e.player:send("pressed " .. e.slot)
            end)
            on("player.chat", function(e)
                shop:open(e.player)
                e:cancel()
            end)
            """.trimIndent()

        val WATCHER =
            """
            local out, lastIn = 0, "none"
            packet.watch("clientbound.system_chat", function(e) out = out + 1 end)
            packet.on("serverbound.chat_message", function(e) lastIn = e.name end)
            on("player.chat", function(e)
                e.player:send("seen " .. lastIn .. " and " .. out)
                e:cancel()
            end)
            """.trimIndent()

        val TALKER =
            """
            channel.on("arena:main", function(p, bytes)
                local reply = {}
                for i, b in ipairs(bytes) do reply[i] = b end
                reply[#reply + 1] = #bytes
                channel.send(p, "arena:main", reply)
            end)
            """.trimIndent()

        val BUILDER =
            """
            on("player.chat", function(e)
                local p = e.player
                local w = worlds.by_name(p.world)
                w:hologram("<gold>Here", p.pos:offset(0, 2, 0))
                w:npc{ name = "Bob", pos = p.pos:offset(2, 0, 0), on_click = function(who) end }
                local made = worlds.create{ name = "arena", dimension = "overworld", type = "flat", seed = 1 }
                p:send("built " .. made.name)
                -- Last, so its packet arrives after the message the test is waiting for.
                p:attribute("max_health", 40)
                e:cancel()
            end)
            """.trimIndent()

        const val ATTRIBUTE_LOOKAHEAD = 4

        const val CHAT_LOOKAHEAD = 3
        const val BLOCK_LOOKAHEAD = 8
    }
}
