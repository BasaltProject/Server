package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ScheduledBlocksTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("scheduled", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/scheduled",
            Logger("ScheduledTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun pathAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path

    /** parity: `FarmlandBlock.turnToDirt`, which converts rather than breaks. */
    @Test
    fun coveredFarmlandBecomesDirtRatherThanBreaking() {
        world.state.setBlockState(1, 70, 1, state("farmland", mapOf("moisture" to "3")))
        world.state.setBlockState(1, 71, 1, Blocks.STONE.stateId)

        NeighbourUpdates.afterChange(world, 1, 71, 1) { _, _, _, _ -> }

        assertEquals(Blocks.DIRT.stateId, world.state.getBlockState(1, 70, 1))
    }

    @Test
    fun aCoveredPathBecomesDirtToo() {
        world.state.setBlockState(3, 70, 1, state("dirt_path"))
        world.state.setBlockState(3, 71, 1, Blocks.STONE.stateId)

        NeighbourUpdates.afterChange(world, 3, 71, 1) { _, _, _, _ -> }

        assertEquals(Blocks.DIRT.stateId, world.state.getBlockState(3, 70, 1))
    }

    /** parity: `CoralBlock.tick`: out of water it dies rather than disappearing. */
    @Test
    fun coralOutOfWaterDies() {
        world.state.setBlockState(5, 70, 1, state("tube_coral_block"))

        assertEquals(
            "dead_tube_coral_block",
            BlockStateDefinitions.blockOf(checkNotNull(ScheduledBlocks.coral(world, 5, 70, 1)))?.path,
        )
    }

    @Test
    fun coralInWaterLives() {
        world.state.setBlockState(7, 70, 1, state("tube_coral_block"))
        world.state.setBlockState(7, 71, 1, Blocks.WATER.stateId)

        assertEquals(null, ScheduledBlocks.coral(world, 7, 70, 1))
    }

    /** parity: `ScaffoldingBlock.getDistance`, which counts out from what holds it. */
    @Test
    fun scaffoldingCountsItsDistanceFromTheGround() {
        world.state.setBlockState(9, 69, 1, Blocks.STONE.stateId)
        for (dx in 0..3) {
            world.state.setBlockState(
                9 + dx,
                70,
                1,
                state("scaffolding", mapOf("distance" to "7", "waterlogged" to "false", "bottom" to "false")),
            )
        }

        // Through the neighbour update, not one pass by hand: each piece that changes re-examines
        // the ones beside it, which is how a row converges instead of settling at whatever the
        // first sweep happened to see.
        NeighbourUpdates.afterChange(world, 9, 69, 1) { _, _, _, _ -> }

        assertEquals(
            "0",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(9, 70, 1)))["distance"],
        )
        assertEquals(
            "1",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(10, 70, 1)))["distance"],
        )
        assertEquals(
            "3",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(12, 70, 1)))["distance"],
        )
    }

    /** parity: `BubbleColumnBlock.updateColumn`: soul sand pushes a column up through the water. */
    @Test
    fun soulSandUnderWaterMakesABubbleColumn() {
        world.state.setBlockState(12, 75, 5, state("soul_sand"))
        for (dy in 1..3) world.state.setBlockState(12, 75 + dy, 5, Blocks.WATER.stateId)

        val changes = mutableListOf<BlockChange>()
        ScheduledBlocks.bubbleColumn(world, 12, 76, 5, changes)

        for (dy in 1..3) {
            assertEquals("bubble_column", pathAt(12, 75 + dy, 5), "the column should reach ${75 + dy}")
        }
        assertEquals(
            "false",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(12, 76, 5)))["drag"],
            "soul sand pushes up",
        )
    }

    @Test
    fun magmaDragsTheColumnDown() {
        world.state.setBlockState(14, 75, 5, state("magma_block"))
        world.state.setBlockState(14, 76, 5, Blocks.WATER.stateId)

        ScheduledBlocks.bubbleColumn(world, 14, 76, 5, mutableListOf())

        assertEquals(
            "true",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(14, 76, 5)))["drag"],
        )
    }

    /** parity: `FrostedIceBlock.tick`, which thins a step at a time and then turns to water. */
    @Test
    fun frostedIceThinsAndThenMelts() {
        world.state.setBlockState(1, 75, 5, state("frosted_ice", mapOf("age" to "0")))
        val random = LegacyRandom(2L)
        val changes = mutableListOf<BlockChange>()

        repeat(200) { ScheduledBlocks.frostedIce(world, 1, 75, 5, random, changes) }

        assertEquals(Blocks.WATER.stateId, world.state.getBlockState(1, 75, 5), "it should have melted away")
    }

    /** parity: `DriedGhastBlock.tick`, and the tick it asks for while it still has work. */
    @Test
    fun aSoakedDriedGhastFillsUp() {
        world.state.setBlockState(4, 75, 5, state("dried_ghast", mapOf("hydration" to "0", "waterlogged" to "true")))
        val changes = mutableListOf<BlockChange>()

        repeat(10) { ScheduledBlocks.driedGhast(world, 4, 75, 5, changes) }

        assertEquals(
            "3",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(4, 75, 5)))["hydration"],
        )
        assertTrue(changes.isNotEmpty())
    }

    @Test
    fun aDryOneDriesOut() {
        world.state.setBlockState(6, 75, 5, state("dried_ghast", mapOf("hydration" to "3", "waterlogged" to "false")))
        val changes = mutableListOf<BlockChange>()

        repeat(10) { ScheduledBlocks.driedGhast(world, 6, 75, 5, changes) }

        assertEquals(
            "0",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(6, 75, 5)))["hydration"],
        )
    }
}
