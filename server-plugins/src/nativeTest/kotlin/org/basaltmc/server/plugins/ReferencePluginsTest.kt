package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The three reference plugins in `examples/`, loaded from their real files and played through.
 *
 * They are the acceptance of this stage, and they live in the repository rather than in a document
 * because a reference plugin nobody runs is a snippet, and a snippet rots: an API change that breaks
 * them breaks the build.
 */
class ReferencePluginsTest {
    private val root = "build/test-data/reference-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)
    private val casey = TestPlayer("Casey").also { services.players.list += it }
    private val robin = TestPlayer("Robin").also { services.players.list += it }

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun install(vararg names: String) {
        for (name in names) copyTree("$EXAMPLES/$name", "${services.paths.packages}/$name")
        val report = host.loadAll()
        assertEquals(names.toList().sorted(), report.loaded.sorted(), report.refused.toString())
    }

    @Test
    fun `the economy pays one player from another`() {
        install("economy")
        services.commands.run("balance", by = casey)
        assertEquals(listOf("<gold>You have 100 coins"), casey.received)

        services.commands.run("pay", mapOf("who" to "Robin", "amount" to 30L), by = casey)
        assertEquals("70", services.storage.rows[Triple("economy", casey.uuid, "balance")])
        assertEquals("130", services.storage.rows[Triple("economy", robin.uuid, "balance")])
        assertEquals(listOf("<green>Casey paid you 30"), robin.received)
    }

    @Test
    fun `the economy will not pay what is not there`() {
        install("economy")
        services.commands.run("pay", mapOf("who" to "Robin", "amount" to 1000L), by = casey)
        assertEquals(listOf("<red>You only have 100 coins"), casey.received)
        assertEquals(null, services.storage.rows[Triple("economy", robin.uuid, "balance")])
    }

    @Test
    fun `a balance is a placeholder the rest of the server can render`() {
        install("economy")
        assertEquals("100", services.placeholders.fill("%economy_balance%", casey))
        assertEquals("0", services.placeholders.fill("%economy_balance%", null))
    }

    @Test
    fun `claims stop somebody else breaking your blocks`() {
        install("claims")
        services.commands.run("claim", by = casey)
        assertEquals(listOf("<green>This chunk is yours"), casey.received)

        breakBlock(robin)
        assertTrue(broken.cancelled, "somebody else built inside a claim")
        // Delivering the refusal is the server's half of the bargain, so what the plugin owes is
        // the message, already looked up in its own lang file.
        assertEquals("<red>This land belongs to Casey", broken.cancelMessage)

        breakBlock(casey)
        assertTrue(!broken.cancelled, "the owner could not build in their own claim")
    }

    @Test
    fun `an unclaimed chunk belongs to nobody`() {
        install("claims")
        breakBlock(robin)
        assertTrue(!broken.cancelled, "an unclaimed chunk was protected")

        services.commands.run("claim", by = casey)
        services.commands.run("unclaim", by = casey)
        breakBlock(robin)
        assertTrue(!broken.cancelled, "the claim outlived being given up")
    }

    @Test
    fun `spleef pays its winner through the economy`() {
        install("economy", "spleef")
        services.commands.run("spleef", by = casey)
        services.commands.run("spleef", by = robin)

        // Robin digs a hole and falls through it.
        breakBlock(robin, y = 64)
        fall(robin)

        assertTrue(casey.received.contains("<gold>Casey wins!"), casey.received.toString())
        assertEquals("150", services.storage.rows[Triple("economy", casey.uuid, "balance")])
        // The arena is put back the way it was found.
        assertEquals(
            "minecraft:snow_block",
            services.worlds.list
                .first()
                .block(4, 64, 4),
        )
    }

    @Test
    fun `spleef runs without the economy because it only asked softly`() {
        install("spleef")
        services.commands.run("spleef", by = casey)
        services.commands.run("spleef", by = robin)
        breakBlock(robin, y = 64)
        fall(robin)
        assertEquals(0, host.statuses.single().errors, "a missing soft dependency should not be a failure")
        assertTrue(casey.received.contains("<gold>Casey wins!"))
    }

    @Test
    fun `all three say what they do in a third of the lines`() {
        // The comparison this stage is measured by. The Bukkit column is the same plugin written
        // the shortest honest way there, counted from the sources in docs: plugin class, listener,
        // command handling, the service class and the persistence by hand.
        val bukkit = mapOf("economy" to 208, "claims" to 111, "spleef" to 152)
        for ((name, theirs) in bukkit) {
            val ours = FileIO.readText("$EXAMPLES/$name/src/main.lua")?.lines()?.size ?: 0
            assertTrue(ours in 1..(theirs / 3), "$name is $ours lines against $theirs in Bukkit")
        }
    }

    @Test
    fun `every example in the folder loads`() {
        // Whatever is in there, not a list kept beside it: an example that stopped loading is a
        // broken promise, and one nobody runs is a snippet.
        val names = FileIO.list(EXAMPLES).filter { FileIO.directoryExists("$EXAMPLES/$it") }.sorted()
        assertTrue(names.size >= 8, "only found $names")
        install(*names.toTypedArray())
    }

    @Test
    fun `the menu example draws a warp per player`() {
        install("warps")
        services.commands.run("warps", by = casey)
        val open = services.menus.open.getValue(casey.uuid)
        assertEquals("<dark_gray>Warps", open.title)
        assertEquals(3, open.rows)
        assertEquals("minecraft:grass_block", open.items[10]?.id)

        services.menus.click(casey, 10)
        assertEquals("1", services.storage.rows[Triple("warps", casey.uuid, "visits_10")])
        assertTrue(casey.received.any { it.contains("Off to") }, casey.received.toString())
    }

    @Test
    fun `the npc example stands somebody up and answers a click`() {
        install("guide")
        host.events.fire(host.events.event(PluginEventTypes.SERVER_LOAD))
        val guides =
            services.worlds.list
                .first()
                .npcs
        assertEquals(2, guides.size)
        // By name and not by position: they come out of a Lua table, which has no order to promise.
        val welcome = guides.first { it.name == "<yellow>Guide" }
        assertEquals(180f, welcome.yaw)

        welcome.click(casey)
        assertEquals(listOf("<yellow>Welcome, Casey. Type /warps to get around."), casey.received)

        services.commands.run("guides", by = casey)
        assertEquals(emptyList(), welcome.audience, "hidden means shown to nobody")
    }

    @Test
    fun `the hologram example keeps one entity and rewrites it`() {
        install("leaderboard")
        host.events.fire(host.events.event(PluginEventTypes.SERVER_LOAD))
        val board =
            services.worlds.list
                .first()
                .holograms
                .single()
        assertTrue(board.text.contains("Nobody has mined"), board.text)

        breakBlock(casey)
        services.tasks.advance(1)
        services.tasks.advance(200)
        assertTrue(board.text.contains("Casey"), board.text)
        assertEquals(
            1,
            services.worlds.list
                .first()
                .holograms.size,
            "it made a second hologram",
        )
    }

    @Test
    fun `the shopkeeper sells through the economy and refuses without it`() {
        install("economy", "shopkeeper")
        host.events.fire(host.events.event(PluginEventTypes.SERVER_LOAD))
        assertEquals(
            1,
            services.worlds.list
                .first()
                .npcs.size,
        )

        services.commands.run("shop", by = casey)
        val shop = services.menus.open.getValue(casey.uuid)
        assertEquals("minecraft:golden_apple", shop.items[13]?.id)
        services.menus.click(casey, 13)
        // A hundred to start with, forty for the apple.
        assertEquals("60", services.storage.rows[Triple("economy", casey.uuid, "balance")])
        assertEquals(
            listOf("minecraft:golden_apple"),
            casey.inventory.slots
                .filterNotNull()
                .map { it.id },
        )
    }

    @Test
    fun `the countdown counts down without a state machine`() {
        install("countdown")
        services.commands.run("countdown", mapOf("seconds" to 3L), by = casey)
        // It is waiting, so the bar is up and the last seconds are titles.
        assertTrue(host.surfaces.anyClaimOn(PluginSurface.BOSS_BAR))
        services.tasks.advance(20)
        services.tasks.advance(20)
        services.tasks.advance(20)
        assertTrue(casey.titles.any { it.startsWith("<green><bold>GO") }, casey.titles.toString())
        assertEquals(0, host.statuses.single().errors)
    }

    @Test
    fun `the readme counts the lines it says it does`() {
        // A table of line counts nobody checks is a table of line counts that is wrong.
        val readme = FileIO.readText("$EXAMPLES/README.md").orEmpty()
        for (line in readme.lines().filter { it.startsWith("| [") }) {
            val name = line.substringAfter("| [").substringBefore("]")
            val claimed = line.split('|')[2].trim().toIntOrNull() ?: continue
            // Trimmed, so the trailing newline every file ends with is not counted as a line.
            val actual =
                FileIO
                    .readText("$EXAMPLES/$name/src/main.lua")
                    ?.trimEnd()
                    ?.lines()
                    ?.size ?: 0
            assertEquals(actual, claimed, "the readme says $name is $claimed lines and it is $actual")
        }
    }

    private val broken = host.events.event(PluginEventTypes.BLOCK_BREAK)

    private fun breakBlock(
        who: TestPlayer,
        y: Int = 70,
    ) {
        broken.set(0, who.name)
        broken.set(1, who.uuid)
        broken.set(2, "world")
        broken.set(3, 4)
        broken.set(4, y)
        broken.set(5, 4)
        broken.set(6, "minecraft:snow_block")
        host.events.fire(broken)
    }

    /** Down through the floor, which is how a round of spleef is lost. */
    private fun fall(who: TestPlayer) {
        val move = host.events.event(PluginEventTypes.PLAYER_MOVE)
        move.set(0, who.name)
        move.set(1, who.uuid)
        move.set(2, "world")
        move.set(3, 4.0)
        move.set(4, 64.0)
        move.set(5, 4.0)
        move.set(6, 4.0)
        move.set(7, 60.0)
        move.set(8, 4.0)
        host.events.fire(move)
        // watch() runs after the fact, on its own turn: the observer is a tick behind on purpose.
        services.tasks.advance(1)
    }

    private fun copyTree(
        from: String,
        to: String,
    ) {
        FileIO.createDirectories(to)
        for (entry in FileIO.list(from)) {
            val source = "$from/$entry"
            val target = "$to/$entry"
            if (FileIO.directoryExists(source)) {
                copyTree(source, target)
            } else {
                FileIO.readBytes(source)?.let { FileIO.writeBytes(target, it) }
            }
        }
    }

    private companion object {
        /** Tests run from the module folder, and the examples are one up from it. */
        const val EXAMPLES = "../examples"

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
