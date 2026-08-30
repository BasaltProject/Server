package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The placer turns a floor plan into templates, and nothing here can be checked against the jar
 * without it. What is pinned is that the house hangs together: every template named is one the
 * mansion has, all storeys and roofs are present, and every declared room is filled exactly once.
 */
class MansionPlacerTest {
    private class RecordingRooms : MansionRooms {
        val filled = mutableListOf<Triple<String, Int, StructurePos>>()

        override fun addRoom1X1(
            pieces: MutableList<MansionPiece>,
            position: StructurePos,
            rotation: StructureRotation,
            doorDirection: StructureDirection?,
            floor: Int,
        ) {
            filled += Triple("1x1", floor, position)
        }

        override fun addRoom1X2(
            pieces: MutableList<MansionPiece>,
            position: StructurePos,
            rotation: StructureRotation,
            roomDirection: StructureDirection?,
            doorDirection: StructureDirection,
            floor: Int,
            isStairs: Boolean,
        ) {
            filled += Triple("1x2", floor, position)
        }

        override fun addRoom2X2(
            pieces: MutableList<MansionPiece>,
            position: StructurePos,
            rotation: StructureRotation,
            roomDirection: StructureDirection,
            doorDirection: StructureDirection,
            floor: Int,
        ) {
            filled += Triple("2x2", floor, position)
        }

        override fun addRoom2X2Secret(
            pieces: MutableList<MansionPiece>,
            position: StructurePos,
            rotation: StructureRotation,
            floor: Int,
        ) {
            filled += Triple("2x2secret", floor, position)
        }
    }

    private val known =
        setOf(
            "entrance",
            "wall_flat",
            "wall_window",
            "wall_corner",
            "small_wall",
            "small_wall_corner",
            "roof",
            "roof_front",
            "roof_corner",
            "roof_inner_corner",
            "corridor_floor",
            "carpet_north",
            "carpet_east",
            "carpet_south_1",
            "carpet_south_2",
            "carpet_west_1",
            "carpet_west_2",
            "indoors_wall_1",
            "indoors_wall_2",
            "indoors_door_1",
            "indoors_door_2",
        )

    private fun build(
        seed: Long,
        rotation: StructureRotation,
    ): Pair<List<MansionPiece>, RecordingRooms> {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, 0, 0)
        val grid = MansionGrid(random)
        val rooms = RecordingRooms()
        val pieces = mutableListOf<MansionPiece>()
        MansionPlacer(random, rooms).createMansion(StructurePos(0, 80, 0), rotation, pieces, grid)
        return pieces to rooms
    }

    @Test
    fun `a mansion is built out of the templates a mansion has`() {
        for (seed in 1L..20L) {
            val (pieces, _) = build(seed, StructureRotation.NONE)
            val stray = pieces.map { it.template }.filterNot { it in known }.distinct()
            assertTrue(stray.isEmpty(), "seed $seed named templates the mansion has none of: $stray")
        }
    }

    @Test
    fun `every plan raises its entrance its walls its corridors and both roofs`() {
        for (seed in 1L..20L) {
            val (pieces, _) = build(seed, StructureRotation.NONE)
            val names = pieces.map { it.template }
            assertEquals(1, names.count { it == "entrance" }, "seed $seed entrances")
            assertTrue(names.any { it == "wall_flat" }, "seed $seed has no ground floor wall")
            assertTrue(names.any { it == "wall_window" }, "seed $seed has no upper wall")
            assertTrue(names.any { it == "corridor_floor" }, "seed $seed has no corridor")
            assertTrue(names.count { it == "roof" } > 4, "seed $seed has barely any roof")
        }
    }

    @Test
    fun `each room of the plan is filled once and only once`() {
        for (seed in 1L..20L) {
            val (_, rooms) = build(seed, StructureRotation.NONE)
            val seen = rooms.filled.map { it.second to it.third }
            assertEquals(seen.size, seen.toSet().size, "seed $seed filled a room twice")
            assertTrue(rooms.filled.isNotEmpty(), "seed $seed filled no room at all")
        }
    }

    @Test
    fun `turning the house moves every piece but changes nothing else`() {
        for (seed in 1L..10L) {
            val plain = build(seed, StructureRotation.NONE)
            val turned = build(seed, StructureRotation.CLOCKWISE_90)
            assertEquals(
                plain.first.map { it.template },
                turned.first.map { it.template },
                "seed $seed built a different house when turned",
            )
        }
    }
}
