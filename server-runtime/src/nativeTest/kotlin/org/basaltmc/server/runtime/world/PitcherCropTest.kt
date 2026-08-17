package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
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
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `PitcherCropBlock`, the one crop that grows into a second block. */
class PitcherCropTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("pitcher", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/pitcher",
            Logger("PitcherCropTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private val ticks = RandomTicks()

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

    private fun propertiesAt(
        x: Int,
        y: Int,
        z: Int,
    ): Map<String, String> = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)

    /** Sky above and farmland below: what a crop needs before any of this is about the crop. */
    private fun plant(
        x: Int,
        z: Int,
        age: Int,
    ) {
        world.state.setBlockState(x, 69, z, state("farmland", mapOf("moisture" to "7")))
        world.state.setBlockState(x, 70, z, state("pitcher_crop", mapOf("age" to age.toString(), "half" to "lower")))
    }

    /** Ticks until the crop moves once, so a test sees one step and not the whole life. */
    private fun tickUntilGrown(
        x: Int,
        z: Int,
        turns: Int = 400,
    ) {
        val random = LegacyRandom(7L)
        val before = world.state.getBlockState(x, 70, z)
        repeat(turns) {
            ticks.tickBlock(world, x, 70, z, random)
            if (world.state.getBlockState(x, 70, z) != before) return
        }
    }

    @Test
    fun onlyTheLowerHalfIsPickedAtRandom() {
        assertTrue(BlockSupport.ticksRandomly(state("pitcher_crop", mapOf("age" to "0", "half" to "lower"))))
        assertFalse(BlockSupport.ticksRandomly(state("pitcher_crop", mapOf("age" to "0", "half" to "upper"))))
        assertFalse(
            BlockSupport.ticksRandomly(state("pitcher_crop", mapOf("age" to "4", "half" to "lower"))),
            "a grown crop has nothing left to do",
        )
    }

    @Test
    fun itGrowsOneAgeAtATime() {
        plant(1, 1, age = 0)

        tickUntilGrown(1, 1)

        assertEquals("1", propertiesAt(1, 70, 1)["age"])
    }

    /** parity: `PitcherCropBlock.grow`, which writes the upper half the moment the crop is tall. */
    @Test
    fun reachingAgeThreeWritesTheUpperHalf() {
        plant(3, 1, age = 2)

        tickUntilGrown(3, 1)

        assertEquals("3", propertiesAt(3, 70, 1)["age"], "the lower half grew")
        assertEquals("3", propertiesAt(3, 71, 1)["age"], "and the upper half arrived with it")
        assertEquals("upper", propertiesAt(3, 71, 1)["half"])
    }

    /** parity: `PitcherCropBlock.canGrow`'s `canGrowInto`, which will not push a block out of the way. */
    @Test
    fun itWillNotGrowTallIntoSomethingSolid() {
        plant(5, 1, age = 2)
        world.state.setBlockState(5, 71, 1, state("stone"))

        tickUntilGrown(5, 1)

        assertEquals("2", propertiesAt(5, 70, 1)["age"], "no room above, so it waits")
        assertEquals("stone", BlockStateDefinitions.blockOf(world.state.getBlockState(5, 71, 1) ?: 0)?.path)
    }

    /** The two halves go together, like a door's. */
    @Test
    fun breakingTheGroundTakesBothHalves() {
        world.state.setBlockState(7, 69, 1, state("farmland", mapOf("moisture" to "7")))
        world.state.setBlockState(7, 70, 1, state("pitcher_crop", mapOf("age" to "3", "half" to "lower")))
        world.state.setBlockState(7, 71, 1, state("pitcher_crop", mapOf("age" to "3", "half" to "upper")))

        world.state.setBlockState(7, 69, 1, org.basaltmc.server.registry.Blocks.AIR.stateId)
        NeighbourUpdates.afterChange(world, 7, 69, 1) { _, _, _, _ -> }

        assertEquals(org.basaltmc.server.registry.Blocks.AIR.stateId, world.state.getBlockState(7, 70, 1))
        assertEquals(org.basaltmc.server.registry.Blocks.AIR.stateId, world.state.getBlockState(7, 71, 1))
    }

    /** parity: `AttachedStemBlock.updateShape`: pick the melon and its stem goes back to a grown stem. */
    @Test
    fun pickingTheMelonDetachesItsStem() {
        world.state.setBlockState(8, 69, 1, state("farmland", mapOf("moisture" to "7")))
        world.state.setBlockState(9, 70, 1, state("melon"))
        world.state.setBlockState(8, 70, 1, state("attached_melon_stem", mapOf("facing" to "east")))

        world.state.setBlockState(9, 70, 1, org.basaltmc.server.registry.Blocks.AIR.stateId)
        NeighbourUpdates.afterChange(world, 9, 70, 1) { _, _, _, _ -> }

        assertEquals("melon_stem", BlockStateDefinitions.blockOf(world.state.getBlockState(8, 70, 1) ?: 0)?.path)
        assertEquals("7", propertiesAt(8, 70, 1)["age"], "it comes back fully grown, ready to fruit again")
    }

    /** parity: `BrushableBlock`, which is `Fallable` without extending `FallingBlock`. */
    @Test
    fun suspiciousSandFallsLikeOrdinarySand() {
        assertTrue(BlockConnections.isFalling(state("suspicious_sand")))
        assertTrue(BlockConnections.isFalling(state("suspicious_gravel")))
    }
}
