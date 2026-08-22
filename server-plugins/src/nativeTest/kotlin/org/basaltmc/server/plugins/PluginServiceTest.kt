package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * One plugin using another, and the boundary between them.
 *
 * The interesting assertions are the ones about what does *not* cross: the provider's own table, a
 * live player object, or a failure inside the provider.
 */
class PluginServiceTest {
    private val root = "build/test-data/services-${nextId()}"
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
        offers: List<String> = emptyList(),
    ) {
        val folder = "${services.paths.packages}/$id"
        FileIO.createDirectories("$folder/src")
        val exports =
            if (offers.isEmpty()) {
                ""
            } else {
                "\n[exports]\nservices = [${offers.joinToString(", ") { "\"$it\"" }}]\n"
            }
        FileIO.writeText(
            "$folder/${PackageLayout.MANIFEST}",
            "id = \"$id\"\nversion = \"1.0.0\"\napi = 1\n$exports",
        )
        FileIO.writeText("$folder/src/main.lua", main)
    }

    @Test
    fun `one plugin calls another through the host`() {
        plugin(
            "bank",
            """
            service("money", {
                balance = function(p) return p.data.coins or 0 end,
                give = function(p, amount) p.data.coins = (p.data.coins or 0) + amount return true end,
            })
            """.trimIndent(),
            offers = listOf("money"),
        )
        plugin(
            "shop",
            """
            cmd("buy", function(src)
                local bank = service("money")
                local p = players.by_name("Casey")
                bank.give(p, 25)
                src:send(tostring(bank.balance(p)))
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("buy", emptyMap())
        assertEquals(listOf("25"), services.commands.replies)
        // The row belongs to the plugin that wrote it, not to the one that asked.
        assertEquals("25", services.storage.rows[Triple("bank", casey.uuid, "coins")])
    }

    @Test
    fun `a service nobody offers is nil which is the whole of a soft dependency`() {
        plugin(
            "lonely",
            """cmd("try", function(src) src:send(tostring(service("nothing"))) end)""",
        )
        host.loadAll()
        services.commands.run("try", emptyMap())
        assertEquals(listOf("nil"), services.commands.replies)
    }

    @Test
    fun `a failure inside the service is the provider's fault not the caller's`() {
        plugin("broken", """service("bad", { go = function() error("inside") end })""", offers = listOf("bad"))
        plugin(
            "caller",
            """
            cmd("go", function(src)
                local ok, err = pcall(function() service("bad").go() end)
                src:send(tostring(ok))
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("go", emptyMap())
        assertEquals(listOf("false"), services.commands.replies, "the caller should have caught it")
        val broken = host.statuses.first { it.id == "broken" }
        val caller = host.statuses.first { it.id == "caller" }
        assertEquals(1, broken.errors, "the strike belongs to whoever wrote the failing code")
        assertEquals(0, caller.errors)
    }

    @Test
    fun `the table a provider passed never reaches the other side`() {
        plugin(
            "secretive",
            """
            local private = { password = "hunter2" }
            service("thing", { get = function() return "public" end, private = private })
            """.trimIndent(),
            offers = listOf("thing"),
        )
        plugin(
            "nosy",
            """
            cmd("peek", function(src)
                local thing = service("thing")
                src:send(thing.get() .. "|" .. tostring(thing.private))
            end)
            """.trimIndent(),
        )
        host.loadAll()
        services.commands.run("peek", emptyMap())
        // Only the functions cross, and only as calls: the provider's own table is not in there.
        assertEquals(listOf("public|nil"), services.commands.replies)
    }

    @Test
    fun `unloading the provider takes the service with it`() {
        plugin("bank", """service("money", { balance = function(p) return 5 end })""", offers = listOf("money"))
        plugin("shop", """cmd("check", function(src) src:send(tostring(service("money"))) end)""")
        host.loadAll()
        services.commands.run("check", emptyMap())
        assertTrue(services.commands.replies.single() != "nil")
        host.unload("bank")
        services.reset()
        services.commands.run("check", emptyMap())
        assertEquals(listOf("nil"), services.commands.replies)
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
