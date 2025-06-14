package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.commands.ExecuteCommand
import org.basaltmc.server.runtime.command.commands.ScoreboardCommand
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class ExecuteConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.5, 64.0, 0.5)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

class ExecuteCommandTest {
    private val scoreboard = Scoreboard()

    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/execute-command",
            logger = Logger("ExecuteCommandTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
                worlds = mapOf("world" to world),
            ),
        ).also(::registerVanillaCommands)
            .also { it.register(ScoreboardCommand(scoreboard)) }
            .also { it.register(ExecuteCommand(scoreboard)) }

    private val source = ExecuteConsole()

    init {
        for (chunkX in -1..1) for (chunkZ in -1..1) world.state.getOrGenerateChunk(chunkX, chunkZ)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun runExecutesTheInnerCommand() {
        assertEquals(1, run("execute run setblock 1 64 1 stone"))
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(1, 64, 1))
    }

    @Test
    fun positionedMovesWhereRelativeCoordinatesResolve() {
        run("execute positioned 5 70 5 run setblock ~ ~ ~ stone")
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(5, 70, 5))
    }

    @Test
    fun alignFloorsTheAxesItNames() {
        run("execute positioned 5.7 70.2 5.9 align xz run setblock ~ ~ ~ dirt")
        assertEquals(Blocks.DIRT.stateId, world.state.getBlockState(5, 70, 5))
    }

    @Test
    fun ifBlockGatesTheRestOfTheChain() {
        run("setblock 2 64 2 stone")
        run("execute if block 2 64 2 stone run setblock 2 65 2 dirt")
        assertEquals(Blocks.DIRT.stateId, world.state.getBlockState(2, 65, 2))

        run("execute if block 2 64 2 dirt run setblock 2 66 2 dirt")
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(2, 66, 2))
    }

    @Test
    fun unlessIsTheOppositeOfIf() {
        run("setblock 3 64 3 stone")
        run("execute unless block 3 64 3 dirt run setblock 3 65 3 stone")
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(3, 65, 3))
    }

    @Test
    fun aConditionOnItsOwnReportsRatherThanGates() {
        run("setblock 4 64 4 stone")
        assertEquals(1, run("execute if block 4 64 4 stone"))
        assertEquals(0, run("execute if block 4 64 4 dirt"))
        assertTrue(source.messages.any { "commands.execute.conditional.fail" in it })
    }

    @Test
    fun ifScoreComparesAgainstARangeAndAnotherScore() {
        run("scoreboard objectives add points dummy")
        run("scoreboard players set Alice points 7")
        run("scoreboard players set Bob points 3")
        assertEquals(1, run("execute if score Alice points matches 5..10"))
        assertEquals(0, run("execute if score Alice points matches ..5"))
        assertEquals(1, run("execute if score Alice points > Bob points"))
        assertEquals(0, run("execute if score Bob points > Alice points"))
    }

    @Test
    fun storeResultWritesTheInnerCommandsResult() {
        run("scoreboard objectives add out dummy")
        run("fill 0 80 0 2 80 0 stone")
        run("execute store result score #filled out run fill 0 80 0 2 80 0 dirt")
        assertEquals(3, scoreboard.score("#filled", "out"))
    }

    @Test
    fun storeSuccessWritesOneOrZero() {
        run("scoreboard objectives add ok dummy")
        run("execute store success score #ok ok run setblock 6 64 6 stone")
        assertEquals(1, scoreboard.score("#ok", "ok"))
        run("execute store success score #ok ok run setblock 6 64 6 stone keep")
        assertEquals(0, scoreboard.score("#ok", "ok"))
    }

    @Test
    fun ifLoadedAnswersForChunksThatAreNotThere() {
        assertEquals(1, run("execute if loaded 0 64 0"))
        assertEquals(0, run("execute if loaded 5000 64 5000"))
    }

    @Test
    fun anUnknownSubcommandIsRefusedByName() {
        run("execute nonsense run seed")
        assertTrue(source.messages.any { "Unknown subcommand" in it })
    }

    @Test
    fun inSwitchesTheWorldTheChainActsOn() {
        // One world here, so `in world` is the identity: what it proves is that the world name is
        // resolved and the chain carries on rather than failing.
        assertEquals(1, run("execute in world run setblock 7 64 7 stone"))
        assertNull(world.state.getBlockState(7, 64, 7)?.takeIf { it == Blocks.AIR.stateId })
    }
}
