package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.TeamCommand
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.wire.protocol.play.CollisionRule
import org.basaltmc.wire.protocol.play.NameTagVisibility
import org.basaltmc.wire.protocol.play.TeamColor
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class TeamConsole : CommandSource {
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

class TeamCommandTest {
    private val teams = Teams()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
            ),
        ).also { it.register(TeamCommand(teams)) }

    private val source = TeamConsole()

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun teamsAreAddedOnceAndRefusedTwice() {
        assertEquals(1, run("team add red"))
        run("team add red")
        assertTrue(source.messages.any { "commands.team.add.duplicate" in it })
    }

    @Test
    fun joiningAnotherTeamLeavesTheFirst() {
        run("team add red")
        run("team add blue")
        run("team join red Alice")
        run("team join blue Alice")
        assertEquals("blue", teams.teamOf("Alice")?.name)
        assertFalse("Alice" in checkNotNull(teams["red"]).members)
    }

    @Test
    fun optionsTakeTheirVanillaNames() {
        run("team add ops")
        run("team modify ops color dark_purple")
        run("team modify ops nametagVisibility hideForOtherTeams")
        run("team modify ops collisionRule pushOwnTeam")
        run("team modify ops friendlyFire false")
        val team = checkNotNull(teams["ops"])
        assertEquals(TeamColor.DARK_PURPLE, team.color)
        assertEquals(NameTagVisibility.HIDE_FOR_OTHER_TEAMS, team.nameTagVisibility)
        assertEquals(CollisionRule.PUSH_OWN_TEAM, team.collisionRule)
        assertFalse(team.friendlyFire)
    }

    @Test
    fun settingAnOptionToWhatItAlreadyIsIsRefused() {
        run("team add same")
        run("team modify same friendlyFire true")
        assertTrue(source.messages.any { "commands.team.option.friendlyfire.alreadyEnabled" in it })
    }

    @Test
    fun emptyingClearsTheMembersAndRefusesWhenAlreadyEmpty() {
        run("team add crew")
        run("team join crew Alice")
        assertEquals(1, run("team empty crew"))
        assertTrue(checkNotNull(teams["crew"]).members.isEmpty())
        run("team empty crew")
        assertTrue(source.messages.any { "commands.team.empty.unchanged" in it })
    }

    @Test
    fun removingATeamForgetsIt() {
        run("team add gone")
        run("team remove gone")
        assertNull(teams["gone"])
    }

    @Test
    fun colorResetClearsIt() {
        run("team add plain")
        run("team modify plain color red")
        run("team modify plain color reset")
        assertNull(checkNotNull(teams["plain"]).color)
    }
}
