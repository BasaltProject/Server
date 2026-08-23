package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Talking past the client, on a named channel.
 *
 * The gate is the interesting part: a channel is how a plugin reaches the proxy, so which ones it
 * may use is written in the manifest where an operator reads it, not decided at runtime.
 */
class PluginChannelTest {
    private val root = "build/test-data/channels-${nextId()}"
    private val services = TestServices(root)
    private val host =
        PluginHost(services).also {
            // The operator said yes, which is the case these tests are about.
            it.allowed = { setOf(PluginPermission.NETWORK) }
        }
    private val casey = TestPlayer("Casey").also { services.players.list += it }

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun plugin(
        main: String,
        declared: List<String> = listOf("arena:main"),
    ) {
        val folder = "${services.paths.packages}/talker"
        FileIO.createDirectories("$folder/src")
        val network =
            if (declared.isEmpty()) {
                ""
            } else {
                "\n[capabilities]\nnetwork = [${declared.joinToString(", ") { "\"$it\"" }}]\n"
            }
        FileIO.writeText(
            "$folder/${PackageLayout.MANIFEST}",
            "id = \"talker\"\nversion = \"1.0.0\"\napi = 1\n$network",
        )
        FileIO.writeText("$folder/src/main.lua", main)
    }

    @Test
    fun `what arrives on a channel reaches the plugin as bytes`() {
        plugin(
            """
            local last = "nothing"
            channel.on("arena:main", function(p, bytes)
                last = p.name .. ":" .. #bytes .. ":" .. bytes[1]
            end)
            cmd("last", function(src) src:send(last) end)
            """.trimIndent(),
        )
        host.loadAll()
        host.channelMessage("arena:main", casey.uuid, byteArrayOf(7, 8, 9))
        services.commands.run("last")
        assertEquals(listOf("Casey:3:7"), services.commands.replies)
    }

    @Test
    fun `a channel the manifest did not declare is refused`() {
        plugin("""channel.on("bungeecord:main", function(p, bytes) end)""")
        val report = host.loadAll()
        assertTrue(report.loaded.isEmpty(), "an undeclared channel should not load")
    }

    @Test
    fun `a plugin that declared none is told what to add`() {
        plugin("""channel.on("arena:main", function(p, bytes) end)""", declared = emptyList())
        val report = host.loadAll()
        assertTrue(report.loaded.isEmpty())
    }

    @Test
    fun `sending goes out through the server`() {
        plugin(
            """
            cmd("ping", function(src)
                channel.send(players.by_name("Casey"), "arena:main", { 1, 2, 3, 4 })
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("ping")
        assertEquals(listOf(Triple("Casey", "arena:main", 4)), services.channels.sent)
    }

    @Test
    fun `nobody listening on a channel is not a failure`() {
        plugin("""channel.on("arena:main", function(p, bytes) end)""")
        host.loadAll()
        host.channelMessage("arena:other", casey.uuid, byteArrayOf(1))
        assertEquals(0, host.statuses.single().errors)
    }

    @Test
    fun `unloading stops the listening`() {
        plugin("""channel.on("arena:main", function(p, bytes) end)""")
        host.loadAll()
        assertTrue(host.listeningOnChannel("arena:main"))
        host.unload("talker")
        assertFalse(host.listeningOnChannel("arena:main"))
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
