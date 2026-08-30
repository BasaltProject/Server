package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The monument shuts doorways at random and only keeps a door shut when the entrance is still
 * reachable from both sides of it. That is the whole point of the walk, so a monument with a room
 * cut off from its entrance means the port lost it.
 */
class MonumentRoomGraphTest {
    private fun graphFor(seed: Long): MonumentRoomGraph {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, 0, 0)
        return generateMonumentRoomGraph(random)
    }

    @Test
    fun `the lattice has its forty six cells plus the roof and the two wings`() {
        for (seed in 1L..30L) {
            val graph = graphFor(seed)
            // Twenty on each of the two full floors, six on the half floor, plus roof and wings.
            assertEquals(20 + 20 + 6 + 3, graph.rooms.size, "seed $seed room count")
            assertEquals(3, graph.rooms.count { it.isSpecial }, "seed $seed roof and wings")
            assertTrue(graph.sourceRoom.isSource, "seed $seed lost its entrance")
            assertTrue(graph.coreRoom.claimed, "seed $seed did not claim its core")
        }
    }

    @Test
    fun `every room still reaches the entrance once the doors are shut`() {
        for (seed in 1L..30L) {
            val graph = graphFor(seed)
            var scan = 1_000_000
            for (room in graph.rooms) {
                if (room.isSpecial) continue
                assertTrue(room.findSource(scan++), "seed $seed walled a room off from the entrance")
            }
        }
    }

    @Test
    fun `the core room claims the eight cells it fills`() {
        for (seed in 1L..30L) {
            val graph = graphFor(seed)
            val core = graph.coreRoom
            val east = StructureDirection.EAST.ordinal
            val north = StructureDirection.NORTH.ordinal
            val up = StructureDirection.UP.ordinal
            val corners =
                listOf(
                    core,
                    core.connections[east],
                    core.connections[north],
                    core.connections[east]?.connections?.get(north),
                    core.connections[up],
                    core.connections[east]?.connections?.get(up),
                    core.connections[north]?.connections?.get(up),
                    core.connections[east]
                        ?.connections
                        ?.get(north)
                        ?.connections
                        ?.get(up),
                )
            assertEquals(8, corners.size)
            for (corner in corners) {
                assertTrue(corner == null || corner.claimed, "seed $seed left part of the core unclaimed")
            }
        }
    }
}
