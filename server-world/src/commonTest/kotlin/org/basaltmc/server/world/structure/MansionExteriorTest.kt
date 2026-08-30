package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The mansion's outer walk turns on `Rotation.rotate` and `Rotation.getRotated`, and a quarter turn
 * the wrong way puts every wall of the house on the wrong side without failing anywhere.
 */
class MansionExteriorTest {
    @Test
    fun `a quarter turn sends each face where the jar sends it`() {
        val north = StructureDirection.NORTH
        val east = StructureDirection.EAST
        val south = StructureDirection.SOUTH
        val west = StructureDirection.WEST
        assertEquals(
            listOf(north, east, south, west),
            listOf(north, east, south, west).map {
                StructureRotation.NONE.rotate(it)
            },
        )
        assertEquals(
            listOf(east, south, west, north),
            listOf(north, east, south, west).map {
                StructureRotation.CLOCKWISE_90.rotate(it)
            },
        )
        assertEquals(
            listOf(south, west, north, east),
            listOf(north, east, south, west).map {
                StructureRotation.CLOCKWISE_180.rotate(it)
            },
        )
        assertEquals(
            listOf(west, north, east, south),
            listOf(north, east, south, west).map { StructureRotation.COUNTERCLOCKWISE_90.rotate(it) },
        )
    }

    @Test
    fun `two turns compose into the one that adds up`() {
        val quarter = StructureRotation.CLOCKWISE_90
        assertEquals(StructureRotation.CLOCKWISE_180, quarter.rotated(quarter))
        assertEquals(StructureRotation.COUNTERCLOCKWISE_90, quarter.rotated(StructureRotation.CLOCKWISE_180))
        assertEquals(StructureRotation.NONE, quarter.rotated(StructureRotation.COUNTERCLOCKWISE_90))
        assertEquals(StructureRotation.NONE, StructureRotation.CLOCKWISE_180.rotated(StructureRotation.CLOCKWISE_180))
        for (rotation in StructureRotation.entries) {
            assertEquals(rotation, rotation.rotated(StructureRotation.NONE), "$rotation composed with none")
        }
    }

    @Test
    fun `the outer walk closes on every plan and lays only pieces the mansion has`() {
        val known = setOf("entrance", "wall_flat", "wall_window", "wall_corner")
        for (seed in 1L..20L) {
            val random = WorldgenRandom(LegacyRandom(0L))
            random.setLargeFeatureSeed(seed, 0, 0)
            val mansion = MansionGrid(random)
            val exterior = MansionExterior(MansionGrid.ENTRANCE_X + 1, MansionGrid.ENTRANCE_Y + 1)
            val pieces = mutableListOf<MansionPiece>()
            val data = MansionPlacement(StructurePos(0, 64, 0), StructureRotation.NONE, "wall_flat")
            exterior.entrance(pieces, data)
            exterior.traverseOuterWalls(
                pieces,
                data,
                mansion.baseGrid,
                StructureDirection.SOUTH,
                MansionGrid.ENTRANCE_X + 1,
                MansionGrid.ENTRANCE_Y + 1,
                MansionGrid.ENTRANCE_X + 1,
                MansionGrid.ENTRANCE_Y,
            )
            assertTrue(pieces.all { it.template in known }, "seed $seed laid something that is not a wall")
            val corners = pieces.count { it.template == "wall_corner" }
            assertTrue(corners >= 4, "seed $seed closed with only $corners corners")
        }
    }
}
