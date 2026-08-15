package org.basaltmc.server.world.portal

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** A handful of blocks in a map, which is all a frame search reads. */
private class Blocks3d : BlockStateView {
    private val blocks = mutableMapOf<Triple<Int, Int, Int>, Int>()

    operator fun set(
        position: Triple<Int, Int, Int>,
        state: Int,
    ) {
        blocks[position] = state
    }

    override fun stateAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int = blocks[Triple(x, y, z)] ?: Blocks.AIR.stateId
}

private val OBSIDIAN = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("obsidian")))

/**
 * Builds the usual frame: [width] by [height] of empty space with obsidian all round it, bottom
 * left of the *interior* at the given position.
 */
private fun frame(
    world: Blocks3d,
    axis: PortalAxis,
    x: Int,
    y: Int,
    z: Int,
    width: Int,
    height: Int,
) {
    fun at(
        column: Int,
        row: Int,
    ) = Triple(x + axis.rightX * column, y + row, z + axis.rightZ * column)
    for (column in -1..width) {
        world[at(column, -1)] = OBSIDIAN
        world[at(column, height)] = OBSIDIAN
    }
    for (row in 0 until height) {
        world[at(-1, row)] = OBSIDIAN
        world[at(width, row)] = OBSIDIAN
    }
}

class PortalShapeTest {
    @Test
    fun findsTheSmallestFrameVanillaAccepts() {
        val world = Blocks3d()
        frame(world, PortalAxis.X, x = 0, y = 64, z = 0, width = 2, height = 3)

        val shape = assertNotNull(PortalShape.findEmpty(world, 0, 64, 0, minY = -64))

        assertEquals(2, shape.width)
        assertEquals(3, shape.height)
        assertEquals(6, shape.interior().size)
        assertFalse(shape.isComplete)
    }

    /** The search falls to the floor of the empty column, so lighting the top finds the same frame. */
    @Test
    fun findsTheFrameFromAnyBlockInsideIt() {
        val world = Blocks3d()
        frame(world, PortalAxis.X, x = 0, y = 64, z = 0, width = 2, height = 3)

        val fromTop = assertNotNull(PortalShape.findEmpty(world, -1, 66, 0, minY = -64))

        assertEquals(64, fromTop.bottomLeftY)
        assertEquals(2, fromTop.width)
    }

    /** The preferred axis is tried first, but a Z frame is found by a search that started on X. */
    @Test
    fun findsAFrameOnTheOtherAxis() {
        val world = Blocks3d()
        frame(world, PortalAxis.Z, x = 0, y = 64, z = 0, width = 3, height = 4)

        val shape = assertNotNull(PortalShape.findEmpty(world, 0, 64, 1, minY = -64, preferred = PortalAxis.X))

        assertEquals(PortalAxis.Z, shape.axis)
        assertEquals(3, shape.width)
        assertEquals(4, shape.height)
    }

    @Test
    fun refusesAFrameThatIsTooNarrowOrTooShort() {
        val narrow = Blocks3d()
        frame(narrow, PortalAxis.X, 0, 64, 0, width = 1, height = 3)
        assertNull(PortalShape.findEmpty(narrow, 0, 64, 0, minY = -64))

        val short = Blocks3d()
        frame(short, PortalAxis.X, 0, 64, 0, width = 2, height = 2)
        assertNull(PortalShape.findEmpty(short, 0, 64, 0, minY = -64))
    }

    @Test
    fun refusesAFrameWiderThanTwentyOne() {
        val world = Blocks3d()
        frame(world, PortalAxis.X, 0, 64, 0, width = 22, height = 3)

        assertNull(PortalShape.findEmpty(world, 0, 64, 0, minY = -64))
    }

    @Test
    fun refusesAFrameWithAHoleInIt() {
        val world = Blocks3d()
        frame(world, PortalAxis.X, 0, 64, 0, width = 2, height = 3)
        world[Triple(0, 67, 0)] = Blocks.AIR.stateId

        assertNull(PortalShape.findEmpty(world, 0, 64, 0, minY = -64))
    }

    /** A lit portal is not an empty frame, but it is still the same shape, and a complete one. */
    @Test
    fun seesALitPortalAsCompleteRatherThanEmpty() {
        val world = Blocks3d()
        frame(world, PortalAxis.X, 0, 64, 0, width = 2, height = 3)
        val portal = PortalShape.portalState(PortalAxis.X)
        for (column in 0 until 2) {
            for (row in 0 until 3) world[Triple(-column, 64 + row, 0)] = portal
        }

        assertNull(PortalShape.findEmpty(world, 0, 64, 0, minY = -64))

        val shape = assertNotNull(PortalShape.find(world, 0, 64, 0, minY = -64) { it.isComplete })
        assertTrue(shape.isComplete)
        assertEquals(6, shape.interior().size)
    }

    @Test
    fun theAxisOfAPortalBlockComesBackOutOfItsState() {
        assertEquals(PortalAxis.X, PortalShape.axisOf(PortalShape.portalState(PortalAxis.X)))
        assertEquals(PortalAxis.Z, PortalShape.axisOf(PortalShape.portalState(PortalAxis.Z)))
        assertNull(PortalShape.axisOf(OBSIDIAN))
    }
}
