package org.basaltmc.server.runtime.world

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

private val WATER = Blocks.WATER.stateId
private val LAVA = Blocks.LAVA.stateId
private val STONE = Blocks.STONE.stateId
private val AIR = Blocks.AIR.stateId
private val COBBLESTONE = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("cobblestone")))

private fun world(
    name: String,
    dimension: WorldDimension = WorldDimension.OVERWORLD,
): LoadedWorld =
    LoadedWorld(
        declaration = WorldDeclaration(name, dimension, WorldType.VOID, seed = 0L, isDefault = true),
        settings = WorldSettings.VANILLA,
        metadata = WorldMetadata(0L, 0, 64, 0),
        worldsRoot = "build/test-data/fluids-$name",
        logger = Logger("FluidTest").apply { minLevel = LogLevel.ERROR },
        generationWorkers = 1,
    )

class FluidFlowTest {
    private val level = world("flow")

    init {
        for (chunkX in -1..1) {
            for (chunkZ in -1..1) level.state.getOrGenerateChunk(chunkX, chunkZ)
        }
        // A stone floor at y = 63, so a flow has something to run along.
        for (x in -8..8) {
            for (z in -8..8) level.state.setBlockState(x, 63, z, STONE)
        }
    }

    @AfterTest
    fun stopGenerating() {
        level.stopGenerating(drainTimeoutMillis = 100)
    }

    /** Runs the world's fluid ticks until nothing is pending, the way the tick loop would. */
    private fun settle(maxTicks: Int = 200) {
        repeat(maxTicks) {
            for (position in level.state.dueFluidTicks()) FluidFlow.tick(level, position[0], position[1], position[2])
            if (level.state.chunkMemory().scheduledTicks == 0) return
        }
    }

    private fun stateAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int = checkNotNull(level.state.getBlockState(x, y, z))

    /** The bug from the screenshots: a hole dug in a pool stayed dry for ever. */
    @Test
    fun aHoleDugInAPoolFillsBackIn() {
        for (x in -4..4) {
            for (z in -4..4) level.state.setBlockState(x, 64, z, WATER)
        }
        level.state.setBlockState(0, 64, 0, AIR)
        FluidFlow.onBlockChanged(level, 0, 64, 0)

        settle()

        // Surrounded by four sources, so it comes back as a source and not as a puddle.
        assertEquals(WATER, stateAt(0, 64, 0))
    }

    @Test
    fun aSourceRunsAlongTheFloorAndFadesWithDistance() {
        level.state.setBlockState(0, 64, 0, WATER)
        FluidFlow.onBlockChanged(level, 0, 64, 0)

        settle()

        // parity: water loses one level per block, so it reaches seven and stops.
        assertEquals(WATER + 1, stateAt(1, 64, 0))
        assertEquals(WATER + 2, stateAt(2, 64, 0))
        assertEquals(WATER + 7, stateAt(7, 64, 0))
        assertEquals(AIR, stateAt(8, 64, 0))
    }

    @Test
    fun waterFallsBeforeItSpreads() {
        // A pit in the floor right next to the spring.
        level.state.setBlockState(1, 63, 0, AIR)
        level.state.setBlockState(0, 64, 0, WATER)
        FluidFlow.onBlockChanged(level, 0, 64, 0)

        settle()

        assertTrue(FluidFlow.kindOf(stateAt(1, 63, 0)) == FluidKind.WATER, "the pit is dry")
        // Falling water carries the full amount down. parity: `getNewLiquid`'s falling case.
        assertEquals(WATER + 8, stateAt(1, 63, 0))
    }

    /** parity: `water_source_conversion`, the rule behind an infinite water pool. */
    @Test
    fun twoSourcesFillTheGapBetweenThem() {
        level.state.setBlockState(-1, 64, 0, WATER)
        level.state.setBlockState(1, 64, 0, WATER)
        FluidFlow.onBlockChanged(level, 0, 64, 0)

        settle()

        assertEquals(WATER, stateAt(0, 64, 0))
    }

    @Test
    fun cuttingOffASpringDrainsWhatItLeftBehind() {
        level.state.setBlockState(0, 64, 0, WATER)
        FluidFlow.onBlockChanged(level, 0, 64, 0)
        settle()

        level.state.setBlockState(0, 64, 0, STONE)
        FluidFlow.onBlockChanged(level, 0, 64, 0)
        settle()

        for (x in 1..7) assertEquals(AIR, stateAt(x, 64, 0), "water left at x = $x")
    }

    /** parity: `LiquidBlock.shouldSpreadLiquid`: flowing lava beside water turns to cobblestone. */
    @Test
    fun lavaMeetingWaterTurnsToRock() {
        level.state.setBlockState(0, 64, 0, LAVA + 1)
        level.state.setBlockState(1, 64, 0, WATER)
        FluidFlow.onBlockChanged(level, 0, 64, 0)

        settle()

        assertEquals(COBBLESTONE, stateAt(0, 64, 0))
    }

    /** parity: `LavaFluid.getDropOff`, two per block outside the Nether: lava reaches three blocks. */
    @Test
    fun lavaSpreadsLessFarThanWater() {
        level.state.setBlockState(0, 64, 0, LAVA)
        FluidFlow.onBlockChanged(level, 0, 64, 0)

        settle(maxTicks = 2000)

        assertEquals(LAVA + 2, stateAt(1, 64, 0))
        assertEquals(LAVA + 6, stateAt(3, 64, 0))
        assertEquals(AIR, stateAt(4, 64, 0))
    }

    @Test
    fun asettledPoolSchedulesNothing() {
        for (x in -2..2) {
            for (z in -2..2) level.state.setBlockState(x, 64, z, WATER)
        }
        FluidFlow.onBlockChanged(level, 0, 64, 0)
        settle()

        assertEquals(0, level.state.chunkMemory().scheduledTicks, "a still pool is still ticking")
    }
}
