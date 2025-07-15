package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * A library, and the line around it.
 *
 * An exported module is code, not state: it is read from its owner's package and loaded into the
 * caller's, so two plugins that require the same module get one each. These tests are mostly about
 * the wall: what is not exported cannot be reached, and being told which is the difference between
 * a one-line fix and an afternoon.
 */
class PluginLibraryTest {
    private val root = "build/test-data/library-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun plugin(
        id: String,
        manifest: String,
        sources: Map<String, String>,
    ) {
        val folder = "${services.paths.packages}/$id"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText("$folder/${PackageLayout.MANIFEST}", manifest)
        for ((name, body) in sources) {
            val path = "$folder/src/$name"
            FileIO.createDirectories(path.substringBeforeLast('/'))
            FileIO.writeText(path, body)
        }
    }

    private fun library(
        modules: String = "[\"api\"]",
        sources: Map<String, String>,
    ) = plugin(
        "money",
        "id = \"money\"\nversion = \"1.0.0\"\napi = 1\n\n[exports]\nmodules = $modules\n",
        sources,
    )

    @Test
    fun `a library has no main and loads anyway`() {
        library(sources = mapOf("api.lua" to "return { format = function(n) return n .. \" coins\" end }"))
        val report = host.loadAll()
        assertEquals(listOf("money"), report.loaded, report.refused.toString())
    }

    @Test
    fun `another plugin requires the exported module`() {
        library(sources = mapOf("api.lua" to "return { format = function(n) return n .. \" coins\" end }"))
        plugin(
            "shop",
            "id = \"shop\"\nversion = \"1.0.0\"\napi = 1\n\n[depends]\nmoney = \"*\"\n",
            mapOf(
                "main.lua" to
                    """
                    local money = require("money.api")
                    cmd("price", function(src) src:send(money.format(12)) end)
                    """.trimIndent(),
            ),
        )
        host.loadAll()
        services.commands.run("price")
        assertEquals(listOf("12 coins"), services.commands.replies)
    }

    @Test
    fun `what is not exported is refused with the reason`() {
        library(
            sources =
                mapOf(
                    "api.lua" to "return {}",
                    "secret.lua" to "return { key = \"hunter2\" }",
                ),
        )
        plugin(
            "nosy",
            "id = \"nosy\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """local s = require("money.secret")"""),
        )
        val report = host.loadAll()
        assertEquals(listOf("money"), report.loaded, "the nosy one should not have loaded")
        assertTrue(report.refused.isNotEmpty())
    }

    @Test
    fun `an asterisk opens the whole src because the owner said so`() {
        library(
            modules = "[\"*\"]",
            sources = mapOf("api.lua" to "return {}", "util/text.lua" to "return { shout = function(s) return s end }"),
        )
        plugin(
            "user",
            "id = \"user\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf(
                "main.lua" to
                    """
                    local text = require("money.util.text")
                    cmd("shout", function(src) src:send(text.shout("hi")) end)
                    """.trimIndent(),
            ),
        )
        host.loadAll()
        services.commands.run("shout")
        assertEquals(listOf("hi"), services.commands.replies)
    }

    @Test
    fun `a module is code and not shared state`() {
        library(sources = mapOf("api.lua" to "local n = 0 return { bump = function() n = n + 1 return n end }"))
        for (id in listOf("one", "two")) {
            plugin(
                id,
                "id = \"$id\"\nversion = \"1.0.0\"\napi = 1\n",
                mapOf(
                    "main.lua" to
                        """
                        local money = require("money.api")
                        cmd("bump_$id", function(src) src:send(tostring(money.bump())) end)
                        """.trimIndent(),
                ),
            )
        }
        host.loadAll()
        services.commands.run("bump_one")
        services.commands.run("bump_one")
        services.commands.run("bump_two")
        // Each caller has its own copy with its own upvalues, which is the whole point.
        assertEquals(listOf("1", "2", "1"), services.commands.replies)
    }

    @Test
    fun `a plugin still requires its own files without exporting anything`() {
        plugin(
            "alone",
            "id = \"alone\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf(
                "helper.lua" to "return { two = function() return 2 end }",
                "main.lua" to
                    """
                    local helper = require("helper")
                    cmd("two", function(src) src:send(tostring(helper.two())) end)
                    """.trimIndent(),
            ),
        )
        host.loadAll()
        services.commands.run("two")
        assertEquals(listOf("2"), services.commands.replies)
    }

    @Test
    fun `a service nobody declared is refused at the call`() {
        plugin(
            "sneaky",
            "id = \"sneaky\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """service("economy", { give = function() end })"""),
        )
        val report = host.loadAll()
        assertTrue(report.loaded.isEmpty(), "offering an undeclared service should not load")
    }

    @Test
    fun `an event a plugin made up reaches whoever listens`() {
        plugin(
            "arena",
            "id = \"arena\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf(
                "main.lua" to
                    """
                    cmd("start", function(src)
                        src:send(tostring(emit("arena:round_start", { map = "dust", players = 4 })))
                    end)
                    """.trimIndent(),
            ),
        )
        plugin(
            "stats",
            "id = \"stats\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf(
                "main.lua" to
                    """
                    local last
                    on("arena:round_start", function(e) last = e.map .. "/" .. e.players end)
                    cmd("last", function(src) src:send(tostring(last)) end)
                    """.trimIndent(),
            ),
        )
        host.loadAll()
        services.commands.run("start")
        services.commands.run("last")
        assertEquals(listOf("1", "dust/4"), services.commands.replies)
    }

    @Test
    fun `emitting into an empty room is not a failure`() {
        plugin(
            "arena",
            "id = \"arena\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """cmd("start", function(src) src:send(tostring(emit("arena:nobody", {}))) end)"""),
        )
        host.loadAll()
        services.commands.run("start")
        assertEquals(listOf("0"), services.commands.replies)
        assertEquals(0, host.statuses.single().errors)
    }

    @Test
    fun `a listener that breaks is its own fault and does not stop the others`() {
        plugin(
            "arena",
            "id = \"arena\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """cmd("start", function(src) src:send(tostring(emit("arena:go", {}))) end)"""),
        )
        plugin(
            "bad",
            "id = \"bad\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """on("arena:go", function(e) error("boom") end)"""),
        )
        plugin(
            "good",
            "id = \"good\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf(
                "main.lua" to
                    """
                    local seen = 0
                    on("arena:go", function(e) seen = seen + 1 end)
                    cmd("seen", function(src) src:send(tostring(seen)) end)
                    """.trimIndent(),
            ),
        )
        host.loadAll()
        services.commands.run("start")
        services.commands.run("seen")
        assertEquals(listOf("1", "1"), services.commands.replies, "the good listener should still have run")
        assertEquals(1, host.statuses.first { it.id == "bad" }.errors)
        assertEquals(0, host.statuses.first { it.id == "arena" }.errors)
    }

    @Test
    fun `an event without a colon would collide with the server's own`() {
        plugin(
            "arena",
            "id = \"arena\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """cmd("start", function(src) emit("round_start", {}) end)"""),
        )
        host.loadAll()
        services.commands.run("start")
        assertEquals(1, host.statuses.single().errors)
    }

    @Test
    fun `unloading the listener takes the listening with it`() {
        plugin(
            "arena",
            "id = \"arena\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """cmd("start", function(src) src:send(tostring(emit("arena:go", {}))) end)"""),
        )
        plugin(
            "stats",
            "id = \"stats\"\nversion = \"1.0.0\"\napi = 1\n",
            mapOf("main.lua" to """on("arena:go", function(e) end)"""),
        )
        host.loadAll()
        services.commands.run("start")
        host.unload("stats")
        services.commands.run("start")
        assertEquals(listOf("1", "0"), services.commands.replies)
    }

    @Test
    fun `a plugin can name its own modules the way everyone else names them`() {
        library(
            sources =
                mapOf(
                    "util.lua" to "return { two = function() return 2 end }",
                    "main.lua" to
                        """
                        local util = require("money.util")
                        cmd("two", function(src) src:send(tostring(util.two())) end)
                        """.trimIndent(),
                ),
        )
        host.loadAll()
        services.commands.run("two")
        assertEquals(listOf("2"), services.commands.replies)
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
