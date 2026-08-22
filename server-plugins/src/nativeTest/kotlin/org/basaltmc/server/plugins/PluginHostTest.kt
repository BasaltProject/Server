package org.basaltmc.server.plugins

import org.basaltmc.server.lua.LuaLimits
import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The acceptance of this stage, run as a test rather than by hand: a plugin that plugs in, reloads
 * and unplugs with the server up, survives its own mistakes, and cannot spin the tick loop.
 */
class PluginHostTest {
    private val root = "build/test-data/plugins-${nextId()}"
    private val services = TestServices(root)

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun host(
        lanes: Int = 1,
        limits: LuaLimits = LuaLimits(),
    ) = PluginHost(services, lanes = lanes, limits = limits)

    private fun writePlugin(
        id: String,
        main: String,
        manifest: String? = null,
        files: Map<String, String> = emptyMap(),
    ) {
        val folder = "${services.paths.packages}/$id"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText(
            "$folder/${PackageLayout.MANIFEST}",
            manifest ?: "id = \"$id\"\nname = \"$id\"\nversion = \"1.0.0\"\napi = 1\n",
        )
        FileIO.writeText("$folder/src/main.lua", main)
        for ((path, text) in files) {
            FileIO.createDirectories("$folder/$path".substringBeforeLast('/'))
            FileIO.writeText("$folder/$path", text)
        }
    }

    @Test
    fun `a plugin plugs in answers a command and unplugs without residue`() {
        writePlugin(
            "arena",
            """
            cmd("greet <who: word>", function(src, who)
                src:send("hello " .. who)
            end)
            """.trimIndent(),
        )
        val host = host()
        val report = host.loadAll()
        assertEquals(listOf("arena"), report.loaded, report.refused.toString())
        assertEquals(1, services.commands.registered.size)

        services.commands.run("greet", mapOf("who" to "world"))
        assertEquals(listOf("hello world"), services.commands.replies)

        val gone = host.unload("arena")
        assertEquals(listOf("arena"), gone)
        assertTrue(services.commands.registered.isEmpty(), "the command outlived the plugin")
        assertTrue(host.statuses.isEmpty())
    }

    @Test
    fun `a timer runs on its period and goes when the plugin does`() {
        writePlugin(
            "ticker",
            """
            local n = 0
            every(5, function()
                n = n + 1
                log.info("tick " .. n)
            end)
            """.trimIndent(),
        )
        val host = host()
        host.loadAll()
        assertEquals(0, services.tasks.ranTimers)
        services.tasks.advance(5)
        assertEquals(1, services.tasks.ranTimers)
        services.tasks.advance(10)
        assertEquals(3, services.tasks.ranTimers)

        host.unload("ticker")
        services.tasks.advance(20)
        assertEquals(3, services.tasks.ranTimers, "a timer of an unloaded plugin still fired")
    }

    @Test
    fun `wait ends the turn and the rest of it happens later`() {
        writePlugin(
            "countdown",
            """
            cmd("go", function(src)
                src:send("three")
                wait(20)
                src:send("two")
                wait(20)
                src:send("one")
            end)
            """.trimIndent(),
        )
        val host = host()
        host.loadAll()
        services.commands.run("go", emptyMap())
        assertEquals(listOf("three"), services.commands.replies)
        services.tasks.advance(20)
        assertEquals(listOf("three", "two"), services.commands.replies)
        services.tasks.advance(20)
        assertEquals(listOf("three", "two", "one"), services.commands.replies)
    }

    @Test
    fun `the folder is whatever shape the plugin shipped`() {
        writePlugin(
            "big",
            """
            local parts = {
                tostring(config.root),
                config.chat.prefix,
                config.en.welcome,
                files.read("data/worlds.json"),
                #files.list("modules/"),
            }
            cmd("check", function(src) src:send(table.concat(parts, "|")) end)
            """.trimIndent(),
            files =
                mapOf(
                    "resources/config.toml" to "root = true\n",
                    "resources/modules/chat.toml" to "prefix = \"[chat]\"\n",
                    "resources/modules/notes.toml" to "unused = 1\n",
                    "resources/lang/en.toml" to "welcome = \"hi\"\n",
                    "resources/data/worlds.json" to "[\"arena\"]",
                ),
        )
        val host = host()
        val report = host.loadAll()
        assertEquals(listOf("big"), report.loaded, report.refused.toString())

        services.commands.run("check", emptyMap())
        assertEquals(listOf("true|[chat]|hi|[\"arena\"]|2"), services.commands.replies)

        // A folder being worked on is its own working folder: its resources/ is used where it lies
        // rather than copied next to itself, so nothing is duplicated while developing.
        assertTrue(FileIO.exists("$root/plugins/big/resources/modules/chat.toml"))
        assertFalse(FileIO.exists("$root/plugins/big/modules/chat.toml"), "the folder was copied onto itself")
        assertFalse(FileIO.directoryExists("$root/plugins/big/config"), "nothing should be invented")
    }

    @Test
    fun `a packaged plugin gets its resources put down as its folder`() {
        val bytes =
            TestZip.zipOf(
                PackageLayout.MANIFEST to "id = \"packed\"\nversion = \"1.0.0\"\napi = 1\n",
                "src/main.lua" to "cmd('packed', function(src) src:send(config.chat.prefix) end)",
                "resources/config.toml" to "root = true\n",
                "resources/modules/chat.toml" to "prefix = \"[chat]\"\n",
                "resources/lang/en.toml" to "welcome = \"hi\"\n",
            )
        FileIO.createDirectories(services.paths.packages)
        FileIO.writeBytes("${services.paths.packages}/packed${PackageLayout.EXTENSION}", bytes)

        val host = host()
        assertEquals(listOf("packed"), host.loadAll().loaded)
        assertTrue(FileIO.exists("$root/plugins/packed/config.toml"))
        assertTrue(FileIO.exists("$root/plugins/packed/modules/chat.toml"))
        assertTrue(FileIO.exists("$root/plugins/packed/lang/en.toml"))
        services.commands.run("packed", emptyMap())
        assertEquals(listOf("[chat]"), services.commands.replies)

        // A second load leaves the operator's edit alone.
        FileIO.writeText("$root/plugins/packed/modules/chat.toml", "prefix = \"[edited]\"\n")
        host.reload("packed")
        services.reset()
        services.commands.run("packed", emptyMap())
        assertEquals(listOf("[edited]"), services.commands.replies)
    }

    @Test
    fun `two files with the same name are refused`() {
        writePlugin(
            "twins",
            "log.info('twins')",
            files =
                mapOf(
                    "resources/modules/chat.toml" to "a = 1\n",
                    "resources/other/chat.toml" to "a = 2\n",
                ),
        )
        val report = host().loadAll()
        assertTrue(report.loaded.isEmpty())
        assertTrue(report.refused.isNotEmpty())
    }

    @Test
    fun `what the package keeps to itself is not copied out but is still readable`() {
        writePlugin(
            "keeper",
            """
            cmd("tpl", function(src) src:send(files.read("templates/arena.json")) end)
            """.trimIndent(),
            manifest =
                "id = \"keeper\"\nversion = \"1.0.0\"\napi = 1\n\n[resources]\nprivate = [\"templates/\"]\n",
            files = mapOf("resources/templates/arena.json" to "{}"),
        )
        val host = host()
        assertEquals(listOf("keeper"), host.loadAll().loaded)
        assertFalse(FileIO.exists("$root/plugins/keeper/templates/arena.json"), "a private resource was copied out")
        services.commands.run("tpl", emptyMap())
        assertEquals(listOf("{}"), services.commands.replies)
    }

    @Test
    fun `what the operator edited is what the plugin reads`() {
        writePlugin(
            "settings",
            """
            cmd("show", function(src) src:send(tostring(config.size)) end)
            """.trimIndent(),
            files = mapOf("resources/config.toml" to "size = 10\n"),
        )
        val first = host()
        first.loadAll()
        services.commands.run("show", emptyMap())
        assertEquals(listOf("10"), services.commands.replies)

        first.unloadAll()
        FileIO.writeText("$root/plugins/settings/resources/config.toml", "size = 42\n")
        val second = host()
        services.reset()
        second.loadAll()
        services.commands.run("show", emptyMap())
        assertEquals(listOf("42"), services.commands.replies, "the default overwrote the operator's file")
    }

    @Test
    fun `a plugin that fails keeps failing on its own not on the server`() {
        writePlugin(
            "broken",
            """
            cmd("boom", function(src)
                local x = nil
                return x.field
            end)
            """.trimIndent(),
        )
        val host = host()
        host.loadAll()
        repeat(2) { services.commands.run("boom", emptyMap()) }
        assertFalse(host.statuses.single().disabled, "two mistakes are not a pattern")
        assertEquals(2, host.statuses.single().errorStreak)
        services.commands.run("boom", emptyMap())
        assertTrue(host.statuses.single().disabled, "three in a row should turn it off")
        assertEquals(3, host.statuses.single().errors)
    }

    @Test
    fun `an endless loop costs the plugin its turn and nothing else`() {
        writePlugin(
            "spinner",
            """
            cmd("spin", function(src) while true do end end)
            cmd("fine", function(src) src:send("still here") end)
            """.trimIndent(),
        )
        val host = host(limits = LuaLimits(instructionsPerTurn = 200_000))
        host.loadAll()
        services.commands.run("spin", emptyMap())
        assertTrue(host.statuses.single().errors == 1)
        services.commands.run("fine", emptyMap())
        assertEquals(listOf("still here"), services.commands.replies, "the server stopped taking commands")
    }

    @Test
    fun `a package dropped in later comes up without a restart`() {
        writePlugin("first", "log.info('one')")
        val host = host()
        assertEquals(listOf("first"), host.loadAll().loaded)

        writePlugin("second", "cmd('two', function(src) src:send('from two') end)")
        assertEquals(listOf("second"), host.loadAll().loaded, "the second one should come up on its own")
        services.commands.run("two", emptyMap())
        assertEquals(listOf("from two"), services.commands.replies)
        assertEquals(2, host.statuses.size)
    }

    @Test
    fun `reloading picks up the file as it is now`() {
        writePlugin("live", "cmd('say', function(src) src:send('before') end)")
        val host = host()
        host.loadAll()
        services.commands.run("say", emptyMap())
        assertEquals(listOf("before"), services.commands.replies)

        writePlugin("live", "cmd('say', function(src) src:send('after') end)")
        host.reload("live")
        services.reset()
        services.commands.run("say", emptyMap())
        assertEquals(listOf("after"), services.commands.replies)
        assertEquals(1, services.commands.registered.size, "reloading registered the command twice")
    }

    @Test
    fun `unloading one takes out what depended on it and says so first`() {
        writePlugin("economy", "log.info('money')")
        writePlugin(
            "shop",
            "log.info('shop')",
            manifest = "id = \"shop\"\nversion = \"1.0.0\"\napi = 1\n\n[depends]\neconomy = \"*\"\n",
        )
        val host = host()
        assertEquals(listOf("economy", "shop"), host.loadAll().loaded)
        assertEquals(listOf("shop", "economy"), host.unload("economy"))
    }

    @Test
    fun `a plugin whose dependency is missing is refused with the reason`() {
        writePlugin(
            "shop",
            "log.info('shop')",
            manifest = "id = \"shop\"\nversion = \"1.0.0\"\napi = 1\n\n[depends]\neconomy = \"*\"\n",
        )
        val report = host().loadAll()
        assertTrue(report.loaded.isEmpty())
        assertTrue(
            report.refused
                .single()
                .second
                .contains("economy"),
            report.refused.toString(),
        )
    }

    @Test
    fun `a plugin that does not compile is named and the rest still load`() {
        writePlugin("bad", "this is not lua")
        writePlugin("good", "log.info('fine')")
        val report = host().loadAll()
        assertEquals(listOf("good"), report.loaded)
        assertEquals("bad", report.refused.single().first)
    }

    @Test
    fun `require reaches the plugin's own src and nowhere else`() {
        writePlugin(
            "modular",
            """
            local kits = require("kits")
            cmd("kit", function(src) src:send(kits.starter) end)
            """.trimIndent(),
            files = mapOf("src/kits.lua" to "return { starter = 'stone' }"),
        )
        val host = host()
        assertEquals(listOf("modular"), host.loadAll().loaded)
        services.commands.run("kit", emptyMap())
        assertEquals(listOf("stone"), services.commands.replies)
    }

    @Test
    fun `with two lanes a plugin has two of everything which is the point`() {
        writePlugin(
            "laney",
            """
            local seen = 0
            cmd("count", function(src)
                seen = seen + 1
                src:send(plugin.lane .. ":" .. seen .. "/" .. plugin.lanes)
            end)
            """.trimIndent(),
        )
        val host = host(lanes = 2)
        host.loadAll()
        repeat(4) { services.commands.run("count", emptyMap()) }
        assertEquals(listOf("0:1/2", "1:1/2", "0:2/2", "1:2/2"), services.commands.replies)
        assertEquals(1, services.commands.registered.size, "the command should be registered once, not per lane")
    }

    @Test
    fun `the console is told what a plugin is and it goes quiet when unloaded`() {
        writePlugin("status", "cmd('a', function() end) every(20, function() end)")
        val host = host()
        host.loadAll()
        val status = host.statuses.single()
        assertEquals("status", status.id)
        assertEquals("1.0.0", status.version)
        assertTrue(status.editable, "a folder is the editable form")
        assertEquals(1, status.lanes)
        assertTrue(status.registrations >= 2, "a command and a timer should both be registered")
        assertNotNull(status.commands.firstOrNull { it.startsWith("a") })
        host.unload("status")
        assertNull(host.statuses.firstOrNull())
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
