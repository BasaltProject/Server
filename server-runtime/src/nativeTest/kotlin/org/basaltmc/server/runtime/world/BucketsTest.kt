package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.protocol.play.Direction
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class BucketsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration(
                    "buckets",
                    WorldDimension.OVERWORLD,
                    WorldType.VOID,
                    seed = 0L,
                    isDefault = true,
                ),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/buckets",
            logger = Logger("BucketsTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    init {
        for (chunkX in -1..1) {
            for (chunkZ in -1..1) world.state.getOrGenerateChunk(chunkX, chunkZ)
        }
        // A floor at y = 63 to pour onto, with the player standing on it at y = 64.
        for (x in -4..4) {
            for (z in -4..4) world.state.setBlockState(x, 63, z, Blocks.STONE.stateId)
        }
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    /** Looking straight down at the floor from eye height: the water lands on top of it. */
    @Test
    fun aFullBucketPoursOntoTheBlockItIsAimedAt() {
        val result =
            assertNotNull(
                Buckets.use(world, Buckets.WATER, 0.5, 65.62, 0.5, yaw = 0f, pitch = 90f, creative = false),
            )

        assertEquals(Blocks.WATER.stateId, world.state.getBlockState(0, 64, 0))
        assertEquals(Buckets.EMPTY, result.replacement)
        assertEquals(true, result.changes.any { it[1] == 64 })
    }

    /**
     * The bug from the report: aiming at water with a wall behind it makes the client send
     * `use_item_on` rather than `use_item`, and nothing was listening for it, so the bucket did
     * nothing at all. parity: `BucketItem.use`, whose ray goes straight through fluids.
     */
    @Test
    fun aFullBucketEmptiesIntoWaterItWasAimedAt() {
        world.state.setBlockState(0, 64, 0, Blocks.WATER.stateId + 3)

        val result =
            assertNotNull(
                Buckets.useOn(world, Buckets.WATER, x = 0, y = 64, z = 0, face = Direction.UP, creative = false),
            )

        assertEquals(Blocks.WATER.stateId, world.state.getBlockState(0, 64, 0), "the flowing water became a source")
        assertEquals(Buckets.EMPTY, result.replacement)
    }

    /** parity: `canBeReplaced(Fluid)`: lava is not solid either, so water goes straight into it. */
    @Test
    fun aFullBucketEmptiesIntoLava() {
        world.state.setBlockState(0, 64, 0, Blocks.LAVA.stateId)

        assertNotNull(Buckets.useOn(world, Buckets.WATER, x = 0, y = 64, z = 0, face = Direction.UP, creative = false))

        assertEquals(Blocks.WATER.stateId, world.state.getBlockState(0, 64, 0))
    }

    /** A solid block is not replaced: the water goes onto the face that was clicked. */
    @Test
    fun aFullBucketOnAStoneFacePoursNextToIt() {
        world.state.setBlockState(0, 64, 0, Blocks.STONE.stateId)

        assertNotNull(Buckets.useOn(world, Buckets.WATER, x = 0, y = 64, z = 0, face = Direction.UP, creative = false))

        assertEquals(Blocks.STONE.stateId, world.state.getBlockState(0, 64, 0))
        assertEquals(Blocks.WATER.stateId, world.state.getBlockState(0, 65, 0))
    }

    /** parity: creative keeps the bucket it poured. */
    @Test
    fun creativeKeepsTheBucket() {
        val result =
            assertNotNull(
                Buckets.use(world, Buckets.WATER, 0.5, 65.62, 0.5, yaw = 0f, pitch = 90f, creative = true),
            )

        assertNull(result.replacement)
    }

    @Test
    fun anEmptyBucketTakesASourceAndFillsItself() {
        world.state.setBlockState(0, 64, 0, Blocks.WATER.stateId)

        val result =
            assertNotNull(
                Buckets.use(world, Buckets.EMPTY, 0.5, 65.62, 0.5, yaw = 0f, pitch = 90f, creative = false),
            )

        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(0, 64, 0))
        assertEquals(Buckets.WATER, result.replacement)
    }

    /** parity: `BucketPickup` only hands over a source; flowing water stays where it is. */
    @Test
    fun anEmptyBucketLeavesFlowingWaterAlone() {
        world.state.setBlockState(0, 64, 0, Blocks.WATER.stateId + 3)

        assertNull(Buckets.use(world, Buckets.EMPTY, 0.5, 65.62, 0.5, yaw = 0f, pitch = 90f, creative = false))
        assertEquals(Blocks.WATER.stateId + 3, world.state.getBlockState(0, 64, 0))
    }

    /** Poured water is a source, and a source spreads: the pour is what wakes the engine. */
    @Test
    fun pouredWaterStartsFlowing() {
        Buckets.use(world, Buckets.WATER, 0.5, 65.62, 0.5, yaw = 0f, pitch = 90f, creative = true)

        repeat(60) {
            for (position in world.state.dueFluidTicks()) FluidFlow.tick(world, position[0], position[1], position[2])
        }

        assertEquals(Blocks.WATER.stateId + 1, world.state.getBlockState(1, 64, 0))
    }
}
