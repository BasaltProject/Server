package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * A shop window, which is the thing every server has and every plugin rewrites.
 *
 * What is being tested is mostly that the plugin never sees a slot number it did not write: it sets
 * slot 11, it gets told slot 11 was clicked, and nothing about the player's own inventory sharing a
 * window ever reaches it.
 */
class PluginMenuTest {
    private val root = "build/test-data/menus-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)
    private val casey = TestPlayer("Casey").also { services.players.list += it }

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun plugin(main: String) {
        val folder = "${services.paths.packages}/shop"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText("$folder/${PackageLayout.MANIFEST}", "id = \"shop\"\nversion = \"1.0.0\"\napi = 1\n")
        FileIO.writeText("$folder/src/main.lua", main)
        val report = host.loadAll()
        assertTrue(report.loaded.contains("shop"), report.refused.toString())
    }

    private val shop =
        """
        local shop = menu{ title = "<dark_gray>Shop", rows = 3 }
        shop:set(11, { id = "minecraft:diamond_sword", name = "<aqua>Sword" }, function(e)
            e.player:send("you bought a sword in slot " .. e.slot)
        end)
        shop:set(15, "minecraft:bread")
        cmd("shop", function(src) shop:open(players.by_name(src.name)) end)
        cmd("shut", function(src) shop:close(players.by_name(src.name)) end)
        """.trimIndent()

    @Test
    fun `a menu opens with what the plugin put in it`() {
        plugin(shop)
        services.commands.run("shop", by = casey)
        val open = services.menus.open.getValue(casey.uuid)
        assertEquals("<dark_gray>Shop", open.title)
        assertEquals(3, open.rows)
        assertEquals(PluginItem("minecraft:diamond_sword", 1, name = "<aqua>Sword"), open.items[11])
        assertEquals(PluginItem("minecraft:bread", 1), open.items[15])
    }

    @Test
    fun `a click reaches the handler of that slot and nobody else's`() {
        plugin(shop)
        services.commands.run("shop", by = casey)
        services.menus.click(casey, 11)
        assertEquals(listOf("you bought a sword in slot 11"), casey.received)
        // A slot with an item but no handler, and an empty one: neither is an error.
        services.menus.click(casey, 15)
        services.menus.click(casey, 3)
        assertEquals(1, casey.received.size)
        assertEquals(0, host.statuses.single().errors)
    }

    @Test
    fun `closing takes the window away`() {
        plugin(shop)
        services.commands.run("shop", by = casey)
        services.commands.run("shut", by = casey)
        assertTrue(services.menus.open.isEmpty())
    }

    @Test
    fun `unloading the plugin shuts what it had open`() {
        plugin(shop)
        services.commands.run("shop", by = casey)
        host.unload("shop")
        assertTrue(services.menus.open.isEmpty(), "a menu outlived the plugin that drew it")
    }

    @Test
    fun `changing an item redraws it for whoever is looking`() {
        plugin(
            """
            local m = menu{ title = "T", rows = 1 }
            m:set(0, "minecraft:stone")
            cmd("open", function(src) m:open(players.by_name(src.name)) end)
            cmd("swap", function(src) m:set(0, { id = "minecraft:gold_ingot", count = 5 }) end)
            """.trimIndent(),
        )
        services.commands.run("open", by = casey)
        services.commands.run("swap", by = casey)
        assertEquals(
            PluginItem("minecraft:gold_ingot", 5),
            services.menus.open
                .getValue(casey.uuid)
                .items[0],
        )
    }

    @Test
    fun `a slot outside the menu is refused rather than silently dropped`() {
        plugin(
            """
            local m = menu{ rows = 1 }
            cmd("bad", function(src) m:set(30, "minecraft:stone") end)
            """.trimIndent(),
        )
        services.commands.run("bad", by = casey)
        assertEquals(1, host.statuses.single().errors)
    }

    @Test
    fun `a menu taller than the client can draw is refused at once`() {
        val folder = "${services.paths.packages}/shop"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText("$folder/${PackageLayout.MANIFEST}", "id = \"shop\"\nversion = \"1.0.0\"\napi = 1\n")
        FileIO.writeText("$folder/src/main.lua", "local m = menu{ rows = 9 }")
        val report = host.loadAll()
        assertTrue(report.loaded.isEmpty(), "nine rows is not a window the client can draw")
    }

    @Test
    fun `a click handler that fails is a strike and not a dead window`() {
        plugin(
            """
            local m = menu{ rows = 1 }
            m:set(0, "minecraft:stone", function(e) error("bad button") end)
            cmd("open", function(src) m:open(players.by_name(src.name)) end)
            """.trimIndent(),
        )
        services.commands.run("open", by = casey)
        services.menus.click(casey, 0)
        assertEquals(1, host.statuses.single().errors)
        assertTrue(services.menus.open.containsKey(casey.uuid), "the window should still be there")
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
