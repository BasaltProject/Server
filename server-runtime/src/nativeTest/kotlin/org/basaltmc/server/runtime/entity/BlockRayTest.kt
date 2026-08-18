package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** parity: `BlockGetter.clip`, the walk a projectile's flight is tested against. */
class BlockRayTest {
    private fun world(): WorldState = WorldState(listOf(Chunk(0, 0)))

    @Test
    fun anEmptyLineHitsNothing() {
        assertNull(BlockRay.cast(world(), 1.0, 70.0, 1.0, 9.0, 70.0, 1.0))
    }

    /** The first block along the line is the one that stops it, not the one the line ends in. */
    @Test
    fun theNearestBlockWins() {
        val world = world()
        world.setBlockState(5, 70, 1, Blocks.STONE.stateId)
        world.setBlockState(8, 70, 1, Blocks.STONE.stateId)

        val hit = assertNotNull(BlockRay.cast(world, 1.5, 70.5, 1.5, 9.5, 70.5, 1.5))

        assertEquals(5, hit.blockX)
        assertEquals(5.0, hit.x, 1e-9, "it stops on the block's west face")
        assertEquals(BlockFace.WEST, hit.face)
    }

    /** A ray that stops short of the block never reaches it. */
    @Test
    fun aShortRayFallsShort() {
        val world = world()
        world.setBlockState(5, 70, 1, Blocks.STONE.stateId)

        assertNull(BlockRay.cast(world, 1.5, 70.5, 1.5, 4.5, 70.5, 1.5))
    }

    /** A shape that is not a full cube is missed where the block is empty. */
    @Test
    fun itGoesOverAHalfSlab() {
        val world = world()
        world.setBlockState(
            5,
            70,
            1,
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("stone_slab"),
                    mapOf("type" to "bottom", "waterlogged" to "false"),
                ),
            ),
        )

        assertNull(BlockRay.cast(world, 1.5, 70.9, 1.5, 9.5, 70.9, 1.5), "over the slab")
        assertNotNull(BlockRay.cast(world, 1.5, 70.2, 1.5, 9.5, 70.2, 1.5), "and through it")
    }

    /** Falling straight down onto a floor lands on its top face. */
    @Test
    fun aFallHitsTheTopFace() {
        val world = world()
        world.setBlockState(3, 64, 3, Blocks.STONE.stateId)

        val hit = assertNotNull(BlockRay.cast(world, 3.5, 68.0, 3.5, 3.5, 63.0, 3.5))

        assertEquals(65.0, hit.y, 1e-9)
        assertEquals(BlockFace.UP, hit.face)
        assertEquals(64, hit.blockY)
    }
}
