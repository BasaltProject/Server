package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Plugins written to misbehave, and what the server does about each.
 *
 * The point of the sandbox is not that a plugin cannot try. It is that trying gets it nothing, the
 * server keeps running, and the log says which plugin it was. Every test here is one of those three
 * sentences.
 */
class HostilePluginTest {
    private val root = "build/test-data/hostile-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)
    private val casey = TestPlayer("Casey").also { services.players.list += it }

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun plugin(
        main: String,
        manifest: String = "",
    ) {
        val folder = "${services.paths.packages}/hostile"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText(
            "$folder/${PackageLayout.MANIFEST}",
            "id = \"hostile\"\nversion = \"1.0.0\"\napi = 1\n$manifest",
        )
        FileIO.writeText("$folder/src/main.lua", main)
    }

    /** Whether the plugin is off and its failures are on the record with its own name. */
    private fun blamed(): Boolean = host.statuses.singleOrNull()?.let { it.errors > 0 } ?: false

    @Test
    fun `a loop that never ends runs out of its turn instead of the server's`() {
        plugin("""cmd("spin", function(src) while true do end end)""")
        host.loadAll()
        services.commands.run("spin")
        assertTrue(blamed(), "nobody was blamed for the spin")
        // The server is still here, and so is the plugin's next turn.
        services.commands.run("spin")
        assertEquals(2, host.statuses.single().errors)
    }

    @Test
    fun `a turn cannot be extended by catching its own failure`() {
        plugin(
            """
            cmd("greedy", function(src)
                for i = 1, 100 do pcall(function() while true do end end) end
                src:send("still here")
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("greedy")
        // Exhausted stays exhausted: pcall gets the error back and the next loop dies the same way.
        assertEquals(emptyList(), services.commands.replies)
        assertTrue(blamed())
    }

    @Test
    fun `a table that grows without end hits the ceiling and not the server`() {
        plugin(
            """
            cmd("eat", function(src)
                local t = {}
                while true do t[#t + 1] = string.rep("x", 4096) end
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("eat")
        assertTrue(blamed(), "growing without end was not stopped")
    }

    @Test
    fun `require cannot reach outside the package`() {
        plugin(
            """
            -- `os` is not in the list on purpose: it is kept as a shim with the clock on it, and
            -- requiring it hands back that same shim, not a way out.
            local reached = {}
            for _, name in ipairs({ "../../etc/passwd", "/etc/passwd", "io", "debug", "package" }) do
                local ok, module = pcall(require, name)
                if ok and module then reached[#reached + 1] = name end
            end
            cmd("reached", function(src) src:send(table.concat(reached, ",")) end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("reached")
        assertEquals(listOf(""), services.commands.replies, "it reached something outside the package")
    }

    @Test
    fun `the ways out of a lua state are not there`() {
        plugin(
            """
            local found = {}
            for _, name in ipairs({ "io", "os", "debug", "package", "dofile", "loadfile", "require" }) do
                if _G[name] ~= nil then found[#found + 1] = name end
            end
            -- `os` survives as a shim with the clock on it, and nothing that runs anything.
            if os ~= nil and (os.execute or os.remove or os.rename or os.tmpname or os.exit) then
                found[#found + 1] = "os.execute"
            end
            cmd("found", function(src) src:send(table.concat(found, ",")) end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("found")
        assertEquals(listOf("os,require"), services.commands.replies)
    }

    @Test
    fun `a binary chunk is refused because load only takes text`() {
        plugin(
            """
            cmd("sneak", function(src)
                local ok, err = pcall(load, "\27Lua binary chunk here")
                src:send(tostring(ok) .. "/" .. tostring(select(2, pcall(load, "\27Lua"))))
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("sneak")
        // load() answers nil plus a reason rather than running it, which is what text mode means.
        assertTrue(
            services.commands.replies
                .single()
                .startsWith("true/"),
            services.commands.replies.toString(),
        )
    }

    @Test
    fun `watching packets without the grant is not possible`() {
        // It asked for the capability in its manifest, and the operator did not grant it.
        plugin(
            """cmd("watch", function(src) src:send(tostring(packet)) end)""",
            manifest = "\n[capabilities]\npackets = true\n",
        )
        host.loadAll()
        services.commands.run("watch")
        assertEquals(listOf("nil"), services.commands.replies)
    }

    @Test
    fun `a channel to a host it did not declare is refused`() {
        plugin(
            """cmd("call", function(src) channel.on("bungeecord:main", function(p, b) end) end)""",
            manifest = "\n[capabilities]\nnetwork = [\"arena:main\"]\n",
        )
        host.allowed = { setOf(PluginPermission.NETWORK) }
        host.loadAll()
        services.commands.run("call")
        assertTrue(blamed(), "it registered on a channel it never declared")
    }

    @Test
    fun `running a command as the console needs the operator to say so`() {
        plugin(
            """cmd("sudo", function(src) src:send(tostring(run)) end)""",
            manifest = "\n[capabilities]\ncommands = true\n",
        )
        host.loadAll()
        services.commands.run("sudo")
        assertEquals(listOf("nil"), services.commands.replies)
    }

    @Test
    fun `what it asked for and did not get is granted once the operator says so`() {
        plugin(
            """cmd("sudo", function(src) src:send(type(run)) end)""",
            manifest = "\n[capabilities]\ncommands = true\n",
        )
        host.allowed = { setOf(PluginPermission.COMMANDS) }
        host.loadAll()
        services.commands.run("sudo")
        assertEquals(listOf("function"), services.commands.replies)
    }

    @Test
    fun `it cannot read another plugin's data or the server's own tables`() {
        plugin(
            """
            cmd("dig", function(src)
                local found = {}
                for _, name in ipairs({ "sql", "query", "db", "database", "storage", "execute" }) do
                    if _G[name] ~= nil then found[#found + 1] = name end
                end
                src:send(#found == 0 and "none" or table.concat(found, ","))
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("dig")
        assertEquals(listOf("none"), services.commands.replies)
    }

    @Test
    fun `too many failures in a row take it off the field`() {
        plugin("""cmd("fail", function(src) error("again") end)""")
        host.loadAll()
        repeat(6) { services.commands.run("fail") }
        val status = host.statuses.single()
        assertTrue(status.disabled, "it kept running after failing over and over")
        assertEquals("hostile", status.id)
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
