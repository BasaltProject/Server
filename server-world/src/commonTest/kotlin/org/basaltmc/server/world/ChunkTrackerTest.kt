package org.basaltmc.server.world

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ChunkTrackerTest {
    @Test
    fun firstUpdateLoadsTheWholeSquare() {
        val tracker = ChunkTracker()
        val delta = tracker.update(centerX = 0, centerZ = 0, radius = 1)

        val expected =
            (-1..1).flatMap { x -> (-1..1).map { z -> ChunkPos(x, z) } }.toSet()
        assertEquals(expected, delta.toLoad.toSet())
        assertTrue(delta.toUnload.isEmpty())
        assertEquals(expected, tracker.loaded)
    }

    @Test
    fun movingOneChunkOnlyLoadsAndUnloadsTheEdgeRows() {
        val tracker = ChunkTracker()
        tracker.update(centerX = 0, centerZ = 0, radius = 1)

        val delta = tracker.update(centerX = 1, centerZ = 0, radius = 1)

        // Moving +1 on X: the x=-1 column falls out of range, a new x=2 column comes in.
        assertEquals(setOf(ChunkPos(-1, -1), ChunkPos(-1, 0), ChunkPos(-1, 1)), delta.toUnload.toSet())
        assertEquals(setOf(ChunkPos(2, -1), ChunkPos(2, 0), ChunkPos(2, 1)), delta.toLoad.toSet())
    }

    @Test
    fun stayingInPlaceProducesAnEmptyDelta() {
        val tracker = ChunkTracker()
        tracker.update(centerX = 5, centerZ = -3, radius = 2)

        val delta = tracker.update(centerX = 5, centerZ = -3, radius = 2)

        assertTrue(delta.toLoad.isEmpty())
        assertTrue(delta.toUnload.isEmpty())
    }

    @Test
    fun shrinkingTheRadiusOnlyUnloads() {
        val tracker = ChunkTracker()
        tracker.update(centerX = 0, centerZ = 0, radius = 2)

        val delta = tracker.update(centerX = 0, centerZ = 0, radius = 1)

        assertTrue(delta.toLoad.isEmpty())
        val expectedUnload =
            (-2..2)
                .flatMap { x -> (-2..2).map { z -> ChunkPos(x, z) } }
                .filter { maxOf(kotlin.math.abs(it.x), kotlin.math.abs(it.z)) > 1 }
                .toSet()
        assertEquals(expectedUnload, delta.toUnload.toSet())
        assertEquals(9, tracker.loaded.size)
    }

    @Test
    fun teleportingFarAwayReplacesTheEntireSet() {
        val tracker = ChunkTracker()
        tracker.update(centerX = 0, centerZ = 0, radius = 1)

        val delta = tracker.update(centerX = 1000, centerZ = 1000, radius = 1)

        assertEquals(9, delta.toLoad.size)
        assertEquals(9, delta.toUnload.size)
        assertTrue(delta.toLoad.none { it in delta.toUnload })
    }
}
