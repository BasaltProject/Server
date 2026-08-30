package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The mansion floor plan comes out of nothing but the random, so there is no vector to compare it
 * against without the jar. What can be checked is that every plan it produces is a legal one: the
 * rest of the port reads these flags and would build a broken house from a grid that breaks them.
 */
class MansionGridTest {
    private fun gridFor(seed: Long): MansionGrid {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, 0, 0)
        return MansionGrid(random)
    }

    @Test
    fun `the entrance and the closed border are where the layout expects them`() {
        for (seed in 1L..40L) {
            val grid = gridFor(seed).baseGrid
            for (x in MansionGrid.ENTRANCE_X..MansionGrid.ENTRANCE_X + 1) {
                for (y in MansionGrid.ENTRANCE_Y..MansionGrid.ENTRANCE_Y + 1) {
                    assertEquals(MansionGrid.START_ROOM, grid[x, y], "seed $seed, entrance ($x, $y)")
                }
            }
            for (x in 0 until MansionGrid.SIZE) {
                assertEquals(MansionGrid.BLOCKED, grid[x, 0], "seed $seed, north border at $x")
                assertEquals(MansionGrid.BLOCKED, grid[x, MansionGrid.SIZE - 1], "seed $seed, south border at $x")
            }
        }
    }

    @Test
    fun `every room is one cell two cells or a square of four`() {
        for (seed in 1L..40L) {
            val mansion = gridFor(seed)
            for (floor in 0..2) {
                val cells = mutableMapOf<Int, MutableList<Pair<Int, Int>>>()
                for (y in 0 until MansionGrid.SIZE) {
                    for (x in 0 until MansionGrid.SIZE) {
                        val data = mansion.floorRooms[floor][x, y]
                        val id = data and MansionGrid.ROOM_ID_MASK
                        if (id < 10) continue
                        cells.getOrPut(id) { mutableListOf() } += x to y
                    }
                }
                for ((id, positions) in cells) {
                    val data = mansion.floorRooms[floor][positions[0].first, positions[0].second]
                    val expected =
                        when (data and MansionGrid.ROOM_TYPE_MASK) {
                            MansionGrid.ROOM_1X1 -> 1
                            MansionGrid.ROOM_1X2 -> 2
                            MansionGrid.ROOM_2X2 -> 4
                            else -> error("seed $seed floor $floor room $id has no size flag")
                        }
                    assertEquals(expected, positions.size, "seed $seed, floor $floor, room $id")
                }
            }
        }
    }

    @Test
    fun `a room has one origin cell and only it may carry the door`() {
        for (seed in 1L..40L) {
            val mansion = gridFor(seed)
            for (floor in 0..2) {
                val origins = mutableMapOf<Int, Int>()
                for (y in 0 until MansionGrid.SIZE) {
                    for (x in 0 until MansionGrid.SIZE) {
                        val data = mansion.floorRooms[floor][x, y]
                        val id = data and MansionGrid.ROOM_ID_MASK
                        if (id < 10) continue
                        if (data and MansionGrid.ROOM_ORIGIN_FLAG != 0) {
                            origins[id] = origins.getOrElse(id) { 0 } + 1
                        } else {
                            assertTrue(
                                data and MansionGrid.ROOM_DOOR_FLAG == 0,
                                "seed $seed, floor $floor, room $id has a door away from its origin",
                            )
                        }
                    }
                }
                for ((id, count) in origins) assertEquals(1, count, "seed $seed, floor $floor, room $id origins")
            }
        }
    }
}
