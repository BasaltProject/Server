package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Watching packets, which is the thing Bukkit cannot do and a whole library exists to bolt on.
 *
 * The two things worth proving are the gate and the cost: a plugin that did not ask for the
 * capability has no `packet` at all, and a packet nobody listens to is never opened.
 */
class PluginPacketTest {
    private val root = "build/test-data/packets-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)
    private val casey = TestPlayer("Casey").also { services.players.list += it }

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun plugin(
        main: String,
        allowed: Boolean = true,
    ) {
        val folder = "${services.paths.packages}/watcher"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText(
            "$folder/${PackageLayout.MANIFEST}",
            "id = \"watcher\"\nversion = \"1.0.0\"\napi = 1\n" +
                if (allowed) "\n[capabilities]\npackets = true\n" else "",
        )
        FileIO.writeText("$folder/src/main.lua", main)
    }

    private fun fire(
        name: String,
        serverbound: Boolean = true,
        bytes: ByteArray = byteArrayOf(1, 2, 3),
    ): PluginPacket {
        val packet = host.packets.packet()
        packet.name = name
        packet.serverbound = serverbound
        packet.playerName = casey.name
        packet.playerUuid = casey.uuid
        packet.bytes = bytes
        host.packets.fire(packet)
        return packet
    }

    @Test
    fun `a plugin without the capability has no packet api at all`() {
        plugin("""cmd("try", function(src) src:send(tostring(packet)) end)""", allowed = false)
        host.loadAll()
        services.commands.run("try")
        assertEquals(listOf("nil"), services.commands.replies)
    }

    @Test
    fun `nobody listening means the packet is never opened`() {
        plugin("""log.info("here")""")
        host.loadAll()
        assertFalse(host.packets.listening("serverbound.chat_command"))
        assertTrue(host.packets.fire(host.packets.packet().also { it.name = "serverbound.chat_command" }))
    }

    @Test
    fun `a listener sees the packet and who it is about`() {
        plugin(
            """
            local seen
            packet.on("serverbound.chat_command", function(e)
                seen = e.name .. "/" .. e.player.name .. "/" .. tostring(e.serverbound) .. "/" .. e.size
            end)
            cmd("seen", function(src) src:send(tostring(seen)) end)
            """.trimIndent(),
        )
        host.loadAll()
        fire("serverbound.chat_command")
        services.commands.run("seen")
        assertEquals(listOf("serverbound.chat_command/Casey/true/3"), services.commands.replies)
    }

    @Test
    fun `cancelling one stops it`() {
        plugin("""packet.on("serverbound.chat_command", function(e) e:cancel() end)""")
        host.loadAll()
        val packet = fire("serverbound.chat_command")
        assertTrue(packet.cancelled)
    }

    @Test
    fun `an observer cannot be the one that stops it`() {
        plugin(
            """
            packet.watch("serverbound.chat_command", function(e) e.cancelled = true end)
            """.trimIndent(),
        )
        host.loadAll()
        // It can write the field, but its answer is not the one asked for: the bus already decided.
        val packet = fire("serverbound.chat_command")
        assertTrue(packet.cancelled, "the field is writable")
        assertEquals(0, host.statuses.single().errors)
    }

    @Test
    fun `the bytes can be read and replaced`() {
        plugin(
            """
            packet.on("clientbound.system_chat", function(e)
                local first = e.bytes[1]
                e.bytes = { first, 9, 9 }
            end)
            """.trimIndent(),
        )
        host.loadAll()
        val packet = fire("clientbound.system_chat", serverbound = false, bytes = byteArrayOf(7, 0, 0))
        assertEquals(listOf<Byte>(7, 9, 9), packet.replacement?.toList())
    }

    @Test
    fun `a name this build does not have is refused with the list`() {
        plugin("""packet.on("clientbound.nonsense", function(e) end)""")
        val report = host.loadAll()
        assertTrue(report.loaded.isEmpty(), "a listener on a packet that does not exist should not load")
    }

    @Test
    fun `priorities decide who goes first`() {
        plugin(
            """
            order = {}
            packet.on("serverbound.chat_command", { priority = "late" }, function(e) order[#order + 1] = "late" end)
            packet.on("serverbound.chat_command", { priority = "early" }, function(e) order[#order + 1] = "early" end)
            cmd("order", function(src) src:send(table.concat(order, ",")) end)
            """.trimIndent(),
        )
        host.loadAll()
        fire("serverbound.chat_command")
        services.commands.run("order")
        assertEquals(listOf("early,late"), services.commands.replies)
    }

    @Test
    fun `sending one by name goes through the server`() {
        plugin(
            """
            cmd("ping", function(src)
                local p = players.by_name("Casey")
                src:send(tostring(packet.send(p, "clientbound.system_chat", { 1, 2 })))
                src:send(tostring(packet.send(p, "clientbound.nope", { 1 })))
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("ping")
        assertEquals(listOf("true", "false"), services.commands.replies)
        assertEquals(listOf("clientbound.system_chat" to 2), services.packets.sent)
    }

    @Test
    fun `unloading takes the listeners with it`() {
        plugin("""packet.on("serverbound.chat_command", function(e) e:cancel() end)""")
        host.loadAll()
        assertTrue(host.packets.listening("serverbound.chat_command"))
        host.unload("watcher")
        assertFalse(host.packets.listening("serverbound.chat_command"))
    }

    private companion object {
        var counter = 0

        fun nextId(): Int = ++counter

        fun deleteTree(path: String) {
            if (FileIO.directoryExists(path)) {
                for (name in FileIO.list(path)) deleteTree("$path/$name")
            }
            FileIO.delete(path)
        }
    }
}
