package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.command.commands.DataCommand
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class DataConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 0.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

class DataCommandTest {
    private val storage = CommandStorage()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
            ),
        ).also { it.register(DataCommand(storage)) }

    private val source = DataConsole()
    private val id = Identifier.parse("basalt:test")

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun modifySetWritesIntoStorage() {
        run("""data modify storage basalt:test counter set value 7""")
        assertEquals(7, (storage[id]["counter"] as NbtInt).value)
    }

    /** parity: `/data modify ... from`, which copies a value out of another holder. */
    @Test
    fun modifyFromCopiesAnotherHoldersValue() {
        run("""data modify storage basalt:source name set value "Steve"""")
        run("""data modify storage basalt:test who set from storage basalt:source name""")

        assertEquals("Steve", (storage[id]["who"] as NbtString).value)
    }

    /** parity: `/data modify ... string`, whose slice indices count from the end when negative. */
    @Test
    fun modifyStringTakesASliceOfTheSourceValue() {
        run("""data modify storage basalt:source name set value "Steve"""")
        run("""data modify storage basalt:test short set string storage basalt:source name 0 3""")
        run("""data modify storage basalt:test tail set string storage basalt:source name -2""")

        assertEquals("Ste", (storage[id]["short"] as NbtString).value)
        assertEquals("ve", (storage[id]["tail"] as NbtString).value)
    }

    /** parity: `/data get <path> <scale>`, whose result is the value multiplied by the scale. */
    @Test
    fun getScalesTheNumberItReports() {
        run("""data modify storage basalt:test counter set value 7""")

        assertEquals(7, run("""data get storage basalt:test counter"""))
        assertEquals(70, run("""data get storage basalt:test counter 10"""))
    }

    /** `from` with no path at all copies the whole compound. */
    @Test
    fun modifyFromWithoutAPathCopiesEverything() {
        run("""data modify storage basalt:source a set value 1""")
        run("""data modify storage basalt:test copy set from storage basalt:source""")

        val copy = storage[id]["copy"] as NbtCompound
        assertEquals(1, (copy["a"] as NbtInt).value)
    }

    @Test
    fun nestedPathsAreCreatedOnTheWay() {
        run("""data modify storage basalt:test stats.deaths set value 3""")
        val stats = storage[id]["stats"] as NbtCompound
        assertEquals(3, (stats["deaths"] as NbtInt).value)
    }

    @Test
    fun mergeKeepsWhatWasThereAndAddsTheRest() {
        run("""data modify storage basalt:test a set value 1""")
        run("""data merge storage basalt:test {b:2}""")
        assertEquals(1, (storage[id]["a"] as NbtInt).value)
        assertEquals(2, (storage[id]["b"] as NbtInt).value)
    }

    @Test
    fun getReportsTheValueAndReturnsANumber() {
        run("""data modify storage basalt:test score set value 42""")
        assertEquals(42, run("data get storage basalt:test score"))
        assertTrue(source.messages.any { "commands.data.storage.query" in it })
    }

    @Test
    fun getOnAMissingPathIsRefused() {
        run("data get storage basalt:test nothing")
        assertTrue(source.messages.any { "arguments.nbtpath.nothing_found" in it })
    }

    @Test
    fun removeTakesTheKeyOut() {
        run("""data modify storage basalt:test gone set value 1""")
        run("data remove storage basalt:test gone")
        assertNull(storage[id]["gone"])
    }
}
