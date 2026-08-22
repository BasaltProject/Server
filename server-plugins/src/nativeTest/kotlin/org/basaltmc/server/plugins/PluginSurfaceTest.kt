package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Two plugins wanting the sidebar, which in Bukkit is a coin toss nobody can see.
 *
 * The rule is the one the config files already use: highest priority among those that apply, and
 * declaration order breaking a tie. What these tests are really about is the three promises around
 * it: losing is said out loud, losing is not dying, and a plugin that falls over lets go.
 */
class PluginSurfaceTest {
    private val root = "build/test-data/surfaces-${nextId()}"
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
    }

    private fun sidebar(
        id: String,
        priority: Int,
        title: String,
        appliesTo: String? = null,
    ) = plugin(
        id,
        buildString {
            append("sidebar{\n")
            append("    priority = $priority,\n")
            if (appliesTo != null) append("    when = $appliesTo,\n")
            append("    title = \"$title\",\n")
            append("    lines = function(p) return { p.name, \"$id\" } end,\n")
            append("}\n")
        },
    )

    private fun drawn(): Pair<String, PluginSurfaceContent>? =
        host.surfaces.content(PluginSurface.SIDEBAR, casey)?.let { it.first.pluginId to it.second }

    @Test
    fun `the higher priority draws it`() {
        sidebar("low", 10, "LOW")
        sidebar("high", 50, "HIGH")
        host.loadAll()
        assertEquals("high", drawn()?.first)
        assertEquals("HIGH", drawn()?.second?.title)
        assertEquals(listOf("Casey", "high"), drawn()?.second?.lines)
    }

    @Test
    fun `losing is not dying and the loser gets it back`() {
        sidebar("low", 10, "LOW")
        sidebar("high", 50, "HIGH")
        host.loadAll()
        assertEquals("high", drawn()?.first)
        host.unload("high")
        assertEquals("low", drawn()?.first, "the one below should have taken over on its own")
    }

    @Test
    fun `a claim that does not apply to this player steps aside`() {
        sidebar("arena", 90, "ARENA", appliesTo = "function(p) return p.world == \"arena\" end")
        sidebar("lobby", 10, "LOBBY")
        host.loadAll()
        assertEquals("lobby", drawn()?.first, "the arena one wanted a world this player is not in")
        casey.world = "arena"
        assertEquals("arena", drawn()?.first)
    }

    @Test
    fun `a priority is asked for and never guessed`() {
        plugin("careless", """sidebar{ title = "NO PRIORITY", lines = {} }""")
        val report = host.loadAll()
        assertTrue(report.loaded.isEmpty(), "a sidebar without a priority should not have been accepted")
    }

    @Test
    fun `a tie keeps whoever asked first and says so`() {
        sidebar("first", 50, "FIRST")
        sidebar("second", 50, "SECOND")
        host.loadAll()
        assertEquals("first", drawn()?.first)
    }

    @Test
    fun `the plugin that falls over lets go of the screen`() {
        sidebar("steady", 10, "STEADY")
        plugin(
            "flaky",
            """
            sidebar{ priority = 99, title = "FLAKY", lines = function(p) return { "fine" } end }
            cmd("boom", function(src) error("no") end)
            """.trimIndent(),
        )
        host.loadAll()
        assertEquals("flaky", drawn()?.first)
        repeat(5) { services.commands.run("boom") }
        assertTrue(host.statuses.first { it.id == "flaky" }.disabled, "it should be off by now")
        assertEquals("steady", drawn()?.first, "a plugin that is off must not keep the sidebar")
    }

    @Test
    fun `a claim with nothing to say is passed over`() {
        plugin(
            "quiet",
            """sidebar{ priority = 99, lines = function(p) return nil end }""",
        )
        sidebar("loud", 1, "LOUD")
        host.loadAll()
        assertEquals("loud", drawn()?.first)
    }

    @Test
    fun `unloading everything leaves the surface to the operator`() {
        sidebar("only", 10, "ONLY")
        host.loadAll()
        host.unload("only")
        assertNull(drawn())
    }

    @Test
    fun `every surface is claimable the same way`() {
        plugin(
            "all",
            """
            sidebar{ priority = 1, title = "S", lines = {} }
            boss_bar{ priority = 2, title = "B", lines = {} }
            tab_list{ priority = 3, title = "T", lines = {} }
            chat_format{ priority = 4, title = "C", lines = {} }
            name_tag{ priority = 5, title = "N", lines = {} }
            """.trimIndent(),
        )
        host.loadAll()
        assertEquals(5, host.surfaces.all().size)
        assertEquals(
            "B",
            host.surfaces
                .content(PluginSurface.BOSS_BAR, casey)
                ?.second
                ?.title,
        )
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
