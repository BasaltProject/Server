package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldClock
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

private class RecordingConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Tester"
    override val displayName: Component = Component.text("Tester")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 0.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

/** `/time`, `/weather`, `/gamerule`, `/difficulty` and `/defaultgamemode` against a void world. */
class WorldStateCommandsTest {
    private fun world(
        name: String,
        dimension: WorldDimension = WorldDimension.OVERWORLD,
    ) = LoadedWorld(
        declaration = WorldDeclaration(name, dimension, WorldType.VOID, seed = 0L, isDefault = true),
        settings = WorldSettings.VANILLA,
        metadata = WorldMetadata(0L, 0, 64, 0),
        worldsRoot = "build/test-data/world-state-commands",
        logger = Logger("WorldStateCommandsTest").apply { minLevel = LogLevel.ERROR },
        generationWorkers = 1,
    )

    private val overworld = world("world")
    private val nether = world("nether", WorldDimension.NETHER)

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
                worlds = mapOf("world" to overworld, "nether" to nether),
            ),
        ).also(::registerVanillaCommands)

    private val source = RecordingConsole()

    @AfterTest
    fun stopGenerating() {
        overworld.stopGenerating(drainTimeoutMillis = 100)
        nether.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun timeSetMovesTheClockToAnAbsoluteTick() {
        assertEquals(1000, run("time set 1000"))
        assertEquals(1000L, overworld.clock.totalTicks)
    }

    @Test
    fun timeSetAcceptsAUnitSuffix() {
        run("time set 2d")
        assertEquals(48000L, overworld.clock.totalTicks)
    }

    @Test
    fun timeMarkersMoveForwardToTheNextOccurrence() {
        // parity: 26.2 turned day/noon/night/midnight into markers on a 24000-tick timeline, so
        // asking for noon at 7000 lands on the next day's noon, not on the past one.
        overworld.clock.setTotalTicks(7000)
        run("time set noon")
        assertEquals(30000L, overworld.clock.totalTicks)
    }

    @Test
    fun timeAddNeverGoesBelowZero() {
        overworld.clock.setTotalTicks(100)
        run("time add -500")
        assertEquals(0L, overworld.clock.totalTicks)
    }

    @Test
    fun timeIsRefusedInADimensionWithNoClock() {
        val player = RecordingConsole()
        // The console stands in the default world, so the Nether is reached by its own dispatcher.
        val netherDispatcher =
            CommandDispatcher(
                CommandEnvironment(
                    players = PlayerRegistry(),
                    groupsProvider = { testGroups() },
                    worldSeed = 0L,
                    maxPlayers = 20,
                    defaultWorldName = "nether",
                    worlds = mapOf("nether" to nether),
                ),
            ).also(::registerVanillaCommands)
        netherDispatcher.execute(player, "time set 1000")
        assertContains(player.messages.first(), "commands.time.no_default_clock")
    }

    @Test
    fun theClockOnlyAdvancesWhileAdvanceTimeIsOn() {
        overworld.clock.setTotalTicks(0)
        overworld.tickTime()
        assertEquals(1L, overworld.clock.totalTicks)

        run("gamerule advance_time false")
        overworld.tickTime()
        assertEquals(1L, overworld.clock.totalTicks)
        assertFalse(overworld.gameRules[GameRuleRegistry.ADVANCE_TIME])
        run("gamerule advance_time true")
    }

    @Test
    fun aPausedClockStopsButGameTimeKeepsCounting() {
        run("time pause")
        val before = overworld.clock.totalTicks
        val gameTimeBefore = overworld.gameTime
        overworld.tickTime()
        assertEquals(before, overworld.clock.totalTicks)
        assertEquals(gameTimeBefore + 1, overworld.gameTime)
        run("time resume")
    }

    @Test
    fun aHalfRateClockAdvancesEveryOtherTick() {
        run("time rate 0.5")
        overworld.clock.setTotalTicks(0)
        overworld.tickTime()
        val afterOne = overworld.clock.totalTicks
        overworld.tickTime()
        assertEquals(afterOne + 1, overworld.clock.totalTicks)
        run("time rate 1")
    }

    @Test
    fun weatherSetsRainAndThunderTogether() {
        run("weather thunder 600")
        assertTrue(overworld.weather.raining)
        assertTrue(overworld.weather.thundering)
        assertEquals(600, overworld.weather.rainTime)

        run("weather clear")
        assertFalse(overworld.weather.raining)
        assertFalse(overworld.weather.thundering)
    }

    @Test
    fun gameRuleQueryAnswersWithTheVanillaDefault() {
        // The value travels as a translation argument, which plain text does not render, so the
        // result integer is what states it: 0 for false.
        assertEquals(0, run("gamerule keep_inventory"))
        assertEquals("false", overworld.gameRules.serialized(GameRuleRegistry.KEEP_INVENTORY))
    }

    @Test
    fun gameRuleSetIsRememberedAndBounded() {
        assertEquals(7, run("gamerule random_tick_speed 7"))
        assertEquals(7, overworld.gameRules[GameRuleRegistry.RANDOM_TICK_SPEED])
        run("gamerule random_tick_speed -1")
        assertTrue(source.messages.any { "Integer must not be less than 0" in it })
    }

    @Test
    fun difficultyRefusesTheOneAlreadySet() {
        run("difficulty hard")
        assertEquals("hard", overworld.difficulty)
        run("difficulty hard")
        assertContains(source.messages.last(), "commands.difficulty.failure")
    }

    @Test
    fun defaultGameModeIsRecordedOnTheWorld() {
        run("defaultgamemode creative")
        assertEquals("creative", overworld.defaultGamemode)
    }

    @Test
    fun clockStateSurvivesAWriteAndReadOfLevelNbt() {
        run("time set 12345")
        run("weather rain 900")
        run("gamerule keep_inventory true")
        overworld.saveMetadata()
        val reloaded = WorldMetadata.read("build/test-data/world-state-commands/world")
        checkNotNull(reloaded)
        assertEquals(12345L, reloaded.timeOfDay)
        assertTrue(reloaded.raining)
        assertEquals("true", reloaded.gameRules.serialized(GameRuleRegistry.KEEP_INVENTORY))
    }

    @Test
    fun theNextMarkerIsAWholeDayAheadWhenItHasJustPassed() {
        assertEquals(25000L, WorldClock.timeOfNextMarker(1000, 1000))
    }
}
