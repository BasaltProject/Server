package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.StopwatchCommand
import org.basaltmc.server.runtime.command.commands.Stopwatches
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class StopwatchConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 64.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

class StopwatchCommandTest {
    private var now = 0L

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
            ),
        ).also { it.register(StopwatchCommand(Stopwatches { now })) }

    private val source = StopwatchConsole()

    private fun run(command: String): Int = dispatcher.execute(source, command)

    /** parity: it counts wall-clock time, and `query` returns the seconds scaled. */
    @Test
    fun aStopwatchCountsFromWhenItWasCreated() {
        assertEquals(1, run("stopwatch create basalt:run"))
        now = 5_000

        assertEquals(5, run("stopwatch query basalt:run"))
        assertEquals(50, run("stopwatch query basalt:run 10"))
    }

    @Test
    fun restartingItStartsTheCountAgain() {
        run("stopwatch create basalt:run")
        now = 5_000
        run("stopwatch restart basalt:run")
        now = 7_000

        assertEquals(2, run("stopwatch query basalt:run"))
    }

    @Test
    fun creatingTheSameOneTwiceIsRefused() {
        run("stopwatch create basalt:run")

        assertEquals(0, run("stopwatch create basalt:run"))
        assertTrue(source.messages.any { "already_exists" in it })
    }

    @Test
    fun queryingOneNobodyMadeIsRefused() {
        assertEquals(0, run("stopwatch query basalt:missing"))
        assertTrue(source.messages.any { "does_not_exist" in it })
    }

    @Test
    fun removingItLeavesNothingToQuery() {
        run("stopwatch create basalt:run")

        assertEquals(1, run("stopwatch remove basalt:run"))
        assertEquals(0, run("stopwatch query basalt:run"))
    }
}
