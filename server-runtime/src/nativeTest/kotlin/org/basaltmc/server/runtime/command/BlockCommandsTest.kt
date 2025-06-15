package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals

private class TestSource : CommandSource {
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

/**
 * The block-editing commands against a void world, whose chunks are empty and cheap: what is
 * asserted is which states ended up where, which does not depend on terrain existing.
 */
class BlockCommandsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("world", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/block-commands",
            logger = Logger("BlockCommandsTest").apply { minLevel = LogLevel.ERROR },
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

    private val source = TestSource()

    init {
        // /setblock refuses an unloaded chunk, exactly as vanilla does, so the area under test has
        // to exist first.
        for (chunkX in -1..1) for (chunkZ in -1..1) world.state.getOrGenerateChunk(chunkX, chunkZ)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun setBlockWritesTheStateItWasGiven() {
        assertEquals(1, run("setblock 1 64 1 stone"))
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(1, 64, 1))
    }

    @Test
    fun setBlockKeepRefusesAnOccupiedPosition() {
        run("setblock 2 64 2 stone")
        run("setblock 2 64 2 dirt keep")
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(2, 64, 2))
        assertContains(source.messages.last(), "commands.setblock.failed")
    }

    @Test
    fun setBlockRefusesAnUnknownBlock() {
        run("setblock 3 64 3 not_a_block")
        assertContains(source.messages.first(), "argument.block.id.invalid")
    }

    @Test
    fun setBlockRefusesAnUnloadedChunk() {
        run("setblock 4000 64 4000 stone")
        assertContains(source.messages.first(), "argument.pos.unloaded")
    }

    @Test
    fun fillCountsEveryBlockItChanged() {
        assertEquals(27, run("fill 0 64 0 2 66 2 stone"))
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(1, 65, 1))
    }

    @Test
    fun fillHollowLeavesTheCoreEmpty() {
        run("fill 0 70 0 2 72 2 stone")
        run("fill 0 70 0 2 72 2 dirt hollow")
        assertEquals(Blocks.DIRT.stateId, world.state.getBlockState(0, 70, 0))
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(1, 71, 1))
    }

    @Test
    fun fillOutlineLeavesTheCoreAsItWas() {
        run("fill 10 70 10 12 72 12 stone")
        run("fill 10 70 10 12 72 12 dirt outline")
        assertEquals(Blocks.DIRT.stateId, world.state.getBlockState(10, 70, 10))
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(11, 71, 11))
    }

    @Test
    fun fillReplaceOnlyTouchesTheFilteredBlock() {
        run("fill 0 80 0 3 80 0 stone")
        run("setblock 1 80 0 dirt")
        assertEquals(3, run("fill 0 80 0 3 80 0 bedrock replace stone"))
        assertEquals(Blocks.DIRT.stateId, world.state.getBlockState(1, 80, 0))
        assertEquals(Blocks.BEDROCK.stateId, world.state.getBlockState(0, 80, 0))
    }

    @Test
    fun fillRefusesAVolumeOverTheGameRuleLimit() {
        run("fill 0 -64 0 63 255 63 stone")
        assertContains(source.messages.first(), "commands.fill.toobig")
    }

    @Test
    fun cloneCopiesTheRegionAndRefusesAnOverlapWithoutForce() {
        run("fill 0 90 0 1 90 1 stone")
        assertEquals(4, run("clone 0 90 0 1 90 1 4 90 4"))
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(5, 90, 5))

        run("clone 0 90 0 1 90 1 1 90 1")
        assertContains(source.messages.last(), "commands.clone.overlap")
    }

    @Test
    fun cloneMoveClearsTheSource() {
        run("fill 0 95 0 1 95 1 stone")
        run("clone 0 95 0 1 95 1 8 95 8 move")
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(0, 95, 0))
        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(8, 95, 8))
    }

    /** parity: `/clone from <dimension>`, which reads the region out of another world. */
    @Test
    fun cloneFromNamesTheWorldItReadsFrom() {
        run("fill 0 96 0 1 96 1 stone")
        run("clone from world 0 96 0 1 96 1 20 96 20 replace")

        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(20, 96, 20))
    }

    /** parity: `/clone ... to <dimension> <destination>`, the writing half of the same pair. */
    @Test
    fun cloneToNamesTheWorldItWritesInto() {
        run("fill 0 97 0 1 97 1 stone")
        run("clone 0 97 0 1 97 1 to world 22 97 22 replace")

        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(22, 97, 22))
    }

    /** `strict` is accepted and copies the same states, which is all it can mean here. */
    @Test
    fun cloneStrictCopiesTheSameBlocks() {
        run("fill 0 98 0 1 98 1 stone")
        run("clone 0 98 0 1 98 1 24 98 24 strict replace")

        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(24, 98, 24))
    }

    @Test
    fun cloneMaskedKeepsWhatIsUnderTheAirItSkipped() {
        run("fill 0 100 0 1 100 1 stone")
        run("setblock 0 100 0 air")
        run("fill 8 100 8 9 100 9 bedrock")
        run("clone 0 100 0 1 100 1 8 100 8 masked")
        assertEquals(Blocks.BEDROCK.stateId, world.state.getBlockState(8, 100, 8))
        assertNotEquals(Blocks.BEDROCK.stateId, world.state.getBlockState(9, 100, 9))
    }
}
