package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `Entity.collide` and `Shapes.collide`. */
class EntityCollisionTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("collision", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/collision",
            Logger("EntityCollisionTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private val moved = DoubleArray(3)

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun floorAt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        world.state.setBlockState(x, y, z, Blocks.STONE.stateId)
    }

    private fun slabAt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        world.state.setBlockState(
            x,
            y,
            z,
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("stone_slab"),
                    mapOf("type" to "bottom", "waterlogged" to "false"),
                ),
            ),
        )
    }

    private fun itemBox(
        x: Double,
        y: Double,
        z: Double,
    ) = Aabb.of(x, y, z, width = 0.25, height = 0.25)

    /** Nothing in the way means the whole movement happens. */
    @Test
    fun anEmptySkyDoesNotStopAnything() {
        EntityCollision.collide(world.state, itemBox(4.5, 80.0, 4.5), 0.3, -0.5, -0.2, moved)

        assertEquals(0.3, moved[0])
        assertEquals(-0.5, moved[1])
        assertEquals(-0.2, moved[2])
    }

    /** A floor stops a fall exactly on top of it, not a fraction into it. */
    @Test
    fun theFloorStopsAFall() {
        floorAt(4, 70, 4)

        EntityCollision.collide(world.state, itemBox(4.5, 71.4, 4.5), 0.0, -0.6, 0.0, moved)

        assertEquals(-0.4, moved[1], 1e-9, "it lands on the block's top face")
    }

    /** A wall stops a sideways move and leaves the rest of the movement alone. */
    @Test
    fun aWallStopsASidewaysMove() {
        floorAt(6, 70, 4)

        EntityCollision.collide(world.state, itemBox(5.0, 70.0, 4.5), 1.5, -0.1, 0.0, moved)

        assertEquals(0.875, moved[0], 1e-9, "up to the wall's face, not through it")
        assertEquals(-0.1, moved[1], "and the fall is untouched")
    }

    /**
     * parity: `Direction.axisStepOrder`, Y first. Landing on a slab has to happen before the sideways
     * move, or the box is still low enough to catch the slab's side and stops short of it.
     */
    @Test
    fun itLandsBeforeItSlides() {
        slabAt(4, 70, 4)
        slabAt(5, 70, 4)

        EntityCollision.collide(world.state, itemBox(4.5, 70.8, 4.5), 0.6, -0.5, 0.0, moved)

        assertEquals(-0.3, moved[1], 1e-9, "the half slab is half a block tall")
        assertEquals(0.6, moved[0], 1e-9, "and from there the slide is free")
    }

    @Test
    fun aBoxKnowsWhenItIsInsideSomething() {
        floorAt(4, 70, 4)

        assertTrue(EntityCollision.collides(world.state, Aabb.of(4.5, 70.2, 4.5, 0.25, 0.25)))
        assertFalse(EntityCollision.collides(world.state, Aabb.of(4.5, 71.2, 4.5, 0.25, 0.25)))
    }

    /** parity: the epsilon, which is what lets a box slide along a wall it is flush against. */
    @Test
    fun aFlushWallIsNotACollision() {
        floorAt(5, 70, 4)

        // The box's east face sits exactly on the block's west face.
        EntityCollision.collide(world.state, Aabb(4.75, 70.0, 4.4, 5.0, 70.25, 4.6), 0.0, 0.0, 0.3, moved)

        assertEquals(0.3, moved[2], 1e-9, "it slides rather than sticking")
    }
}
