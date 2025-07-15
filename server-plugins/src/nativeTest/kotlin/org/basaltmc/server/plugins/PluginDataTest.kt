package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * What a plugin keeps: about a player, about itself, and in a file.
 *
 * The isolation tests are the ones that matter. A plugin has to be unable to read or write another
 * one's data, and unable to reach the server's own, and it has to be unable to do it *by
 * construction* rather than because it did not think of trying.
 */
class PluginDataTest {
    private val root = "build/test-data/plugindata-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)
    private val casey = TestPlayer("Casey").also { services.players.list += it }

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
        val report = host.loadAll()
        assertTrue(report.loaded.contains(id), report.refused.toString())
    }

    @Test
    fun `a player's data is a field of the player`() {
        plugin(
            "stats",
            """
            cmd("kill", function(src)
                local p = players.by_name("Casey")
                p.data.kills = (p.data.kills or 0) + 1
                src:send(tostring(p.data.kills))
            end)
            """.trimIndent(),
        )
        services.commands.run("kill", emptyMap())
        services.commands.run("kill", emptyMap())
        assertEquals(0, host.statuses.single().errors, "the plugin failed")
        assertEquals(listOf("1", "2"), services.commands.replies)
        assertEquals("2", services.storage.rows[Triple("stats", casey.uuid, "kills")])
    }

    @Test
    fun `numbers come back as numbers and nil removes`() {
        plugin(
            "kinds",
            """
            cmd("kinds", function(src)
                local p = players.by_name("Casey")
                p.data.count = 7
                p.data.ratio = 1.5
                p.data.flag = true
                p.data.note = "hello"
                src:send(type(p.data.count) .. "," .. type(p.data.ratio) .. "," ..
                    type(p.data.flag) .. "," .. type(p.data.note))
                p.data.note = nil
                src:send(tostring(p.data.note))
            end)
            """.trimIndent(),
        )
        services.commands.run("kinds", emptyMap())
        assertEquals(listOf("number,number,boolean,string", "nil"), services.commands.replies)
    }

    @Test
    fun `one plugin cannot see another's data`() {
        plugin("first", """cmd("write", function(src) players.by_name("Casey").data.secret = "mine" end)""")
        plugin("second", """cmd("peek", function(src) src:send(tostring(players.by_name("Casey").data.secret)) end)""")
        services.commands.run("write", emptyMap())
        services.commands.run("peek", emptyMap())
        assertEquals(listOf("nil"), services.commands.replies, "a plugin read another one's data")
        assertEquals("mine", services.storage.rows[Triple("first", casey.uuid, "secret")])
    }

    @Test
    fun `nothing in the api reaches the server's own tables`() {
        // There is no call that takes a table name, a query or a connection: the only door is
        // `data` and `store`, and both name the plugin for you.
        plugin(
            "curious",
            """
            cmd("try", function(src)
                local names = {}
                for _, name in ipairs({ "sql", "query", "database", "db", "execute" }) do
                    if _G[name] ~= nil then names[#names + 1] = name end
                end
                src:send(#names == 0 and "none" or table.concat(names, ","))
            end)
            """.trimIndent(),
        )
        services.commands.run("try", emptyMap())
        assertEquals(listOf("none"), services.commands.replies)
    }

    @Test
    fun `the plugin's own store is not tied to a player`() {
        plugin(
            "records",
            """
            cmd("set", function(src)
                store.best = 42
                store.holder = "Casey"
                src:send(store.best .. "/" .. store.holder)
            end)
            """.trimIndent(),
        )
        services.commands.run("set", emptyMap())
        assertEquals(listOf("42/Casey"), services.commands.replies)
        assertEquals("42", services.storage.rows[Triple("records", "server", "best")])
    }

    @Test
    fun `top is the query a file cannot answer`() {
        val robin = TestPlayer("Robin").also { services.players.list += it }
        val sam = TestPlayer("Sam").also { services.players.list += it }
        services.storage.rows[Triple("ladder", casey.uuid, "kills")] = "3"
        services.storage.rows[Triple("ladder", robin.uuid, "kills")] = "11"
        services.storage.rows[Triple("ladder", sam.uuid, "kills")] = "7"
        plugin(
            "ladder",
            """
            cmd("top", function(src)
                local lines = {}
                for _, row in ipairs(store.top("kills", 2)) do
                    lines[#lines + 1] = row.name .. "=" .. row.value
                end
                src:send(table.concat(lines, " "))
            end)
            """.trimIndent(),
        )
        services.commands.run("top", emptyMap())
        assertEquals(listOf("Robin=11 Sam=7"), services.commands.replies)
    }

    @Test
    fun `a json file keeps what is not about a player`() {
        plugin(
            "arenas",
            """
            cmd("save", function(src)
                local arenas = data.json("arenas")
                arenas.duel = { spawn = { x = 1, y = 64, z = 2 }, kit = "pvp" }
                arenas.count = 1
                data.save("arenas")
                src:send("saved")
            end)
            cmd("load", function(src)
                local arenas = data.json("arenas")
                src:send(arenas.duel.kit .. "/" .. arenas.duel.spawn.y .. "/" .. arenas.count)
            end)
            """.trimIndent(),
        )
        services.commands.run("save", emptyMap())
        // A folder plugin is its own working folder, so its data lands inside its resources/.
        assertTrue(
            FileIO.exists("$root/plugins/arenas/resources/data/arenas.json"),
            "the file was not written",
        )

        // Read back by a fresh load, which is what a restart is.
        host.unload("arenas")
        services.reset()
        host.loadAll()
        services.commands.run("load", emptyMap())
        assertEquals(listOf("pvp/64/1"), services.commands.replies)
    }

    @Test
    fun `a json file that is not there yet is an empty table not an error`() {
        plugin(
            "fresh",
            """cmd("count", function(src) src:send(tostring(#data.json("nothing"))) end)""",
        )
        services.commands.run("count", emptyMap())
        assertEquals(listOf("0"), services.commands.replies)
        assertEquals(0, host.statuses.single().errors)
    }

    @Test
    fun `data survives the plugin being unloaded and loaded again`() {
        plugin(
            "memory",
            """
            cmd("bump", function(src)
                local p = players.by_name("Casey")
                p.data.visits = (p.data.visits or 0) + 1
                src:send(tostring(p.data.visits))
            end)
            """.trimIndent(),
        )
        services.commands.run("bump", emptyMap())
        host.unload("memory")
        services.reset()
        host.loadAll()
        services.commands.run("bump", emptyMap())
        assertEquals(listOf("2"), services.commands.replies)
    }

    @Test
    fun `data of a player who is offline is still theirs`() {
        services.storage.rows[Triple("offline", "uuid-Ghost", "kills")] = "5"
        plugin(
            "offline",
            """cmd("ghost", function(src) src:send(tostring(players.by_name("Ghost"))) end)""",
        )
        services.commands.run("ghost", emptyMap())
        // Nobody by that name is online, so there is no handle: the data is still in the table,
        // which is what reinstalling a plugin tomorrow depends on.
        assertEquals(listOf("nil"), services.commands.replies)
        assertEquals("5", services.storage.rows[Triple("offline", "uuid-Ghost", "kills")])
    }

    @Test
    fun `unloading a plugin does not delete a thing`() {
        plugin("keeper", """cmd("write", function(src) players.by_name("Casey").data.kept = "yes" end)""")
        services.commands.run("write", emptyMap())
        host.unload("keeper")
        assertEquals("yes", services.storage.rows[Triple("keeper", casey.uuid, "kept")])
        assertNull(services.storage.rows[Triple("keeper", casey.uuid, "gone")])
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
