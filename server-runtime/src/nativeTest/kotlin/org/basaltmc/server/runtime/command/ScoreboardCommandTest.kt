package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.ScoreboardCommand
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.wire.protocol.play.DisplaySlot
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class ScoreboardConsole : CommandSource {
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

class ScoreboardCommandTest {
    private val scoreboard = Scoreboard()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
            ),
        ).also { it.register(ScoreboardCommand(scoreboard)) }

    private val source = ScoreboardConsole()

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun objectivesAreAddedOnceAndRefusedTwice() {
        assertEquals(1, run("scoreboard objectives add kills dummy"))
        run("scoreboard objectives add kills dummy")
        assertTrue(source.messages.any { "commands.scoreboard.objectives.add.duplicate" in it })
    }

    @Test
    fun scoresAreSetAddedAndRemoved() {
        run("scoreboard objectives add points dummy")
        assertEquals(10, run("scoreboard players set Alice points 10"))
        assertEquals(13, run("scoreboard players add Alice points 3"))
        assertEquals(8, run("scoreboard players remove Alice points 5"))
        assertEquals(8, scoreboard.score("Alice", "points"))
    }

    @Test
    fun aFakePlayerIsAValidScoreHolder() {
        run("scoreboard objectives add totals dummy")
        run("scoreboard players set #total totals 42")
        assertEquals(42, scoreboard.score("#total", "totals"))
    }

    @Test
    fun operationsFollowVanillasArithmetic() {
        run("scoreboard objectives add a dummy")
        run("scoreboard objectives add b dummy")
        run("scoreboard players set Alice a -7")
        run("scoreboard players set Bob b 2")
        run("scoreboard players operation Alice a /= Bob b")
        // Vanilla floors, so -7 / 2 is -4 and not -3.
        assertEquals(-4, scoreboard.score("Alice", "a"))

        run("scoreboard players set Alice a 5")
        run("scoreboard players operation Alice a >< Bob b")
        assertEquals(2, scoreboard.score("Alice", "a"))
        assertEquals(5, scoreboard.score("Bob", "b"))
    }

    @Test
    fun divisionByZeroIsRefused() {
        run("scoreboard objectives add a dummy")
        run("scoreboard objectives add b dummy")
        run("scoreboard players set Alice a 5")
        run("scoreboard players operation Alice a /= Bob b")
        assertTrue(source.messages.any { "arguments.operation.div0" in it })
    }

    @Test
    fun removingAnObjectiveForgetsItsScoresAndItsSlot() {
        run("scoreboard objectives add temp dummy")
        run("scoreboard players set Alice temp 3")
        run("scoreboard objectives setdisplay sidebar temp")
        run("scoreboard objectives remove temp")
        assertNull(scoreboard["temp"])
        assertNull(scoreboard.score("Alice", "temp"))
        assertNull(scoreboard.displayed(DisplaySlot.SIDEBAR))
    }

    @Test
    fun displayingTheSameObjectiveTwiceIsRefused() {
        run("scoreboard objectives add shown dummy")
        run("scoreboard objectives setdisplay sidebar shown")
        run("scoreboard objectives setdisplay sidebar shown")
        assertTrue(source.messages.any { "commands.scoreboard.objectives.display.alreadySet" in it })
    }

    @Test
    fun enableOnlyWorksOnATriggerObjective() {
        run("scoreboard objectives add plain dummy")
        run("scoreboard players enable Alice plain")
        assertTrue(source.messages.any { "commands.scoreboard.players.enable.invalid" in it })

        run("scoreboard objectives add menu trigger")
        run("scoreboard players enable Alice menu")
        assertTrue(scoreboard.isTriggerEnabled("Alice", "menu"))
        assertEquals(0, scoreboard.score("Alice", "menu"))
    }

    @Test
    fun resetWithoutAnObjectiveClearsEveryScore() {
        run("scoreboard objectives add one dummy")
        run("scoreboard objectives add two dummy")
        run("scoreboard players set Alice one 1")
        run("scoreboard players set Alice two 2")
        run("scoreboard players reset Alice")
        assertNull(scoreboard.score("Alice", "one"))
        assertNull(scoreboard.score("Alice", "two"))
    }
}
