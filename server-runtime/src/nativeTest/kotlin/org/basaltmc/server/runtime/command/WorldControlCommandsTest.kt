package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.TickLoop
import org.basaltmc.server.runtime.command.commands.ForceLoadCommand
import org.basaltmc.server.runtime.command.commands.TickCommand
import org.basaltmc.server.runtime.command.commands.WorldBorderCommand
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.timings.TickDurations
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

private class ControlConsole : CommandSource {
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

/** `/worldborder`, `/forceload` and `/tick`: the three that change how a world runs. */
class WorldControlCommandsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/world-control-commands",
            logger = Logger("WorldControlCommandsTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val tickLoop = TickLoop(saveIntervalSeconds = 0)
    private val durations = TickDurations()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
                worlds = mapOf("world" to world),
            ),
        ).also { it.register(WorldBorderCommand()) }
            .also { it.register(ForceLoadCommand()) }
            .also { it.register(TickCommand(tickLoop, durations) { 0L }) }

    private val source = ControlConsole()

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun theBorderIsSetAndRefusedWhenUnchanged() {
        run("worldborder set 200")
        assertEquals(200.0, world.border.targetSize)
        run("worldborder set 200")
        assertTrue(source.messages.any { "commands.worldborder.set.failed.nochange" in it })
    }

    @Test
    fun addChangesTheSizeRelativeToTheCurrentOne() {
        run("worldborder set 100")
        run("worldborder add 50")
        assertEquals(150.0, world.border.targetSize)
    }

    @Test
    fun aTimedChangeKeepsTheOldSizeUntilItSettles() {
        run("worldborder set 100")
        run("worldborder set 50 10")
        assertEquals(100.0, world.border.size)
        assertEquals(50.0, world.border.targetSize)
        assertEquals(500L, world.border.lerpTimeMillis)
    }

    @Test
    fun aBorderSmallerThanOneBlockIsRefused() {
        run("worldborder set 0.5")
        assertTrue(source.messages.any { "commands.worldborder.set.failed.small" in it })
    }

    @Test
    fun centerAndWarningsAreStored() {
        run("worldborder center 10 -20")
        assertEquals(10.0, world.border.centerX)
        assertEquals(-20.0, world.border.centerZ)
        run("worldborder warning distance 8")
        assertEquals(8, world.border.warningBlocks)
        run("worldborder warning time 30")
        assertEquals(30, world.border.warningSeconds)
    }

    @Test
    fun forceLoadTakesBlockCoordinatesAndKeepsTheChunk() {
        assertEquals(1, run("forceload add 16 16"))
        assertTrue(ChunkGenerationQueue.pack(1, 1) in world.forcedChunks)
        run("forceload add 16 16")
        assertTrue(source.messages.any { "commands.forceload.added.failure" in it })
    }

    @Test
    fun forceLoadRemoveAllForgetsEverything() {
        run("forceload add 0 0")
        run("forceload add 32 32")
        run("forceload remove all")
        assertTrue(world.forcedChunks.isEmpty())
    }

    @Test
    fun forceLoadRefusesMoreChunksThanVanillaAllows() {
        run("forceload add 0 0 4000 4000")
        assertTrue(source.messages.any { "commands.forceload.toobig" in it })
    }

    @Test
    fun tickRateAndFreezeAreRemembered() {
        run("tick rate 10")
        assertEquals(10f, tickLoop.tickRate)
        run("tick freeze")
        assertTrue(tickLoop.isFrozen)
        run("tick unfreeze")
        assertFalse(tickLoop.isFrozen)
    }

    @Test
    fun steppingOnlyWorksWhileFrozen() {
        run("tick step 5")
        assertTrue(source.messages.any { "commands.tick.step.fail" in it })
        run("tick freeze")
        assertEquals(5, run("tick step 5"))
        run("tick unfreeze")
    }

    @Test
    fun sprintStopsOnlyWhileSprinting() {
        run("tick sprint stop")
        assertTrue(source.messages.any { "commands.tick.sprint.stop.fail" in it })
        run("tick sprint 100")
        assertTrue(tickLoop.isSprinting)
        assertEquals(1, run("tick sprint stop"))
    }
}
