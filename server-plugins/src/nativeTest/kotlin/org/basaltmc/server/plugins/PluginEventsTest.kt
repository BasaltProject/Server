package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The bus, from the outside: what a plugin writes and what the server gets back. */
class PluginEventsTest {
    private val root = "build/test-data/events-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun plugin(
        id: String,
        main: String,
    ) {
        val folder = "${services.paths.packages}/$id"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText("$folder/${PackageLayout.MANIFEST}", "id = \"$id\"\nversion = \"1.0.0\"\napi = 1\n")
        FileIO.writeText("$folder/src/main.lua", main)
    }

    /** Fires a block break the way the server does, and says whether it may go ahead. */
    private fun breakBlock(
        who: String = "tester",
        world: String = "world",
        x: Int = 1,
        y: Int = 2,
        z: Int = 3,
        block: String = "minecraft:stone",
    ): PluginEvent {
        val event = host.events.event(PluginEventTypes.BLOCK_BREAK)
        event.set(0, who)
        event.set(1, "uuid-$who")
        event.set(2, world)
        event.set(3, x)
        event.set(4, y)
        event.set(5, z)
        event.set(6, block)
        host.events.fire(event)
        return event
    }

    @Test
    fun `a decider stops what the server was about to do`() {
        plugin(
            "guard",
            """
            on("block.break", function(e)
                if e.block == "minecraft:bedrock" then e:cancel("<red>not that one") end
            end)
            """.trimIndent(),
        )
        host.loadAll()

        assertFalse(breakBlock(block = "minecraft:stone").cancelled)
        val stopped = breakBlock(block = "minecraft:bedrock")
        assertTrue(stopped.cancelled)
        assertEquals("<red>not that one", stopped.cancelMessage)
    }

    @Test
    fun `priority decides who goes first`() {
        plugin(
            "order",
            """
            local seen = {}
            on("block.break", { priority = "last" }, function(e) seen[#seen + 1] = "last" end)
            on("block.break", { priority = "first" }, function(e) seen[#seen + 1] = "first" end)
            on("block.break", function(e) seen[#seen + 1] = "normal" end)
            cmd("order", function(src) src:send(table.concat(seen, ",")) end)
            """.trimIndent(),
        )
        host.loadAll()
        breakBlock()
        services.commands.run("order", emptyMap())
        assertEquals(listOf("first,normal,last"), services.commands.replies)
    }

    @Test
    fun `a cancelled event does not wake the rest unless they asked`() {
        plugin(
            "after",
            """
            local seen = {}
            on("block.break", { priority = "first" }, function(e) e:cancel() end)
            on("block.break", function(e) seen[#seen + 1] = "unaware" end)
            on("block.break", { cancelled = true }, function(e) seen[#seen + 1] = "asked" end)
            cmd("seen", function(src) src:send(table.concat(seen, ",")) end)
            """.trimIndent(),
        )
        host.loadAll()
        breakBlock()
        services.commands.run("seen", emptyMap())
        assertEquals(listOf("asked"), services.commands.replies)
    }

    @Test
    fun `a filter saves the handler from running at all`() {
        plugin(
            "filtered",
            """
            local runs = 0
            on("block.break", { world = "arena" }, function(e) runs = runs + 1 end)
            cmd("runs", function(src) src:send(tostring(runs)) end)
            """.trimIndent(),
        )
        host.loadAll()
        breakBlock(world = "world")
        breakBlock(world = "arena")
        breakBlock(world = "nether")
        services.commands.run("runs", emptyMap())
        assertEquals(listOf("1"), services.commands.replies)
    }

    @Test
    fun `a field that does not exist is an error not a nil`() {
        plugin("typo", """on("block.break", function(e) local x = e.blocky end)""")
        host.loadAll()
        breakBlock()
        assertEquals(1, host.statuses.single().errors)
    }

    @Test
    fun `only what the event says is mutable can be written`() {
        plugin(
            "writer",
            """
            on("player.chat", function(e) e.message = "[edited] " .. e.message end)
            on("block.break", function(e) e.block = "minecraft:dirt" end)
            """.trimIndent(),
        )
        host.loadAll()

        val chat = host.events.event(PluginEventTypes.PLAYER_CHAT)
        chat.set(0, "tester")
        chat.set(1, "uuid")
        chat.set(2, "world")
        chat.set(3, "hello")
        host.events.fire(chat)
        assertEquals("[edited] hello", chat.get("message"))

        breakBlock()
        assertEquals(1, host.statuses.single().errors, "writing a read-only field should be an error")
    }

    @Test
    fun `an event kept past its handler is refused`() {
        plugin(
            "hoarder",
            """
            local kept
            on("block.break", function(e) kept = e end)
            cmd("peek", function(src) src:send(tostring(kept.block)) end)
            """.trimIndent(),
        )
        host.loadAll()
        breakBlock()
        services.commands.run("peek", emptyMap())
        assertTrue(services.commands.replies.isEmpty(), "reading a stale event should have failed")
        assertEquals(1, host.statuses.single().errors)
    }

    @Test
    fun `a decider may not wait an observer may`() {
        plugin(
            "waiter",
            """
            on("block.break", function(e) wait(1) end)
            watch("block.break", function(e)
                wait(1)
                log.info("watched " .. e.block)
            end)
            """.trimIndent(),
        )
        host.loadAll()
        breakBlock()
        assertEquals(1, host.statuses.single().errors, "the decider should have been refused")
        services.tasks.advance(1)
        services.tasks.advance(1)
        assertEquals(1, host.statuses.single().errors, "the observer should have been allowed to wait")
        assertEquals(0, host.statuses.single().errorStreak, "the observer's turn should have cleared the streak")
    }

    @Test
    fun `an observer sees what it was told even after the event moved on`() {
        plugin(
            "late",
            """
            local seen = {}
            watch("block.break", function(e) seen[#seen + 1] = e.block end)
            cmd("seen", function(src) src:send(table.concat(seen, ",")) end)
            """.trimIndent(),
        )
        host.loadAll()
        breakBlock(block = "minecraft:stone")
        breakBlock(block = "minecraft:dirt")
        services.tasks.advance(1)
        services.commands.run("seen", emptyMap())
        assertEquals(listOf("minecraft:stone,minecraft:dirt"), services.commands.replies)
    }

    @Test
    fun `an event nobody knows is refused when it is registered`() {
        plugin("dreamer", """on("block.exploded_into_confetti", function(e) end)""")
        val report = host.loadAll()
        assertTrue(report.loaded.isEmpty(), "a plugin listening to nothing should not have loaded")
    }

    @Test
    fun `one handler failing does not stop the others`() {
        plugin(
            "mixed",
            """
            local ran = 0
            on("block.break", { priority = "first" }, function(e) error("boom") end)
            on("block.break", function(e) ran = ran + 1 end)
            cmd("ran", function(src) src:send(tostring(ran)) end)
            """.trimIndent(),
        )
        host.loadAll()
        breakBlock()
        services.commands.run("ran", emptyMap())
        assertEquals(listOf("1"), services.commands.replies)
    }

    @Test
    fun `unloading takes the handlers with it`() {
        plugin(
            "leaver",
            """
            on("block.break", function(e) e:cancel() end)
            """.trimIndent(),
        )
        host.loadAll()
        assertTrue(breakBlock().cancelled)
        host.unload("leaver")
        assertFalse(breakBlock().cancelled, "a handler outlived its plugin")
        assertFalse(host.events.listening(PluginEventTypes.BLOCK_BREAK))
    }

    @Test
    fun `an event nobody listens to is free`() {
        assertFalse(host.events.listening(PluginEventTypes.BLOCK_BREAK))
        assertFalse(breakBlock().cancelled)
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
