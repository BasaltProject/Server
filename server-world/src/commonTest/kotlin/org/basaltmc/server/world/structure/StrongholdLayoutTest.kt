package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The maze is grown from one shared random, so a missing draw does not fail loudly: it builds a
 * different, perfectly plausible stronghold. These numbers come from the official server's own saved
 * pieces (seed 12345, the stronghold starting in chunk -35, -103).
 */
class StrongholdLayoutTest {
    private fun layoutFor(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): List<StrongholdPiece> {
        val random = WorldgenRandom(LegacyRandom(0L))
        return StrongholdLayout(seaLevel = 63, minY = -64).build(
            random,
            reseed = { attempt -> random.setLargeFeatureSeed(seed + attempt, chunkX, chunkZ) },
            chunkX = chunkX,
            chunkZ = chunkZ,
        )
    }

    private fun signature(piece: StrongholdPiece): String {
        val b = piece.box
        return "${piece.kind} ${b.minX},${b.minY},${b.minZ},${b.maxX},${b.maxY},${b.maxZ} " +
            "${piece.depth} ${piece.orientation} ${piece.entryDoor}"
    }

    @Test
    fun `the maze matches the one the official server saved`() {
        val pieces = layoutFor(-35, -103, 12345L)
        assertEquals(225, pieces.size)
        val expected =
            listOf(
                "START -558,12,-1646,-554,22,-1642 0 NORTH OPENING",
                "FIVE_CROSSING -561,10,-1657,-552,18,-1647 1 NORTH WOOD_DOOR",
                "STRAIGHT_STAIRS_DOWN -557,4,-1665,-553,14,-1658 2 NORTH OPENING",
                "STRAIGHT -568,12,-1651,-562,16,-1647 2 WEST WOOD_DOOR",
                "LEFT_TURN -551,12,-1651,-547,16,-1647 2 EAST GRATES",
                "STRAIGHT_STAIRS_DOWN -551,6,-1659,-547,16,-1652 3 NORTH OPENING",
            )
        assertEquals(expected, pieces.take(expected.size).map(::signature))
        assertEquals("FIVE_CROSSING -596,2,-1638,-587,10,-1628 16 SOUTH IRON_DOOR", signature(pieces.last()))
        // The caps hold: one portal room, at most two libraries, at most four five-way crossings.
        val counts = pieces.groupingBy { it.kind }.eachCount()
        assertEquals(1, counts[StrongholdPieceKind.PORTAL_ROOM])
        assertEquals(2, counts[StrongholdPieceKind.LIBRARY])
        assertEquals(5, counts[StrongholdPieceKind.FIVE_CROSSING])
        assertEquals(33, counts[StrongholdPieceKind.FILLER_CORRIDOR])
    }

    @Test
    fun `a stronghold always ends up with its portal room`() {
        for (chunkZ in -105..-101) {
            val pieces = layoutFor(-35, chunkZ, 987654321L)
            assertTrue(
                pieces.any { it.kind == StrongholdPieceKind.PORTAL_ROOM },
                "chunk (-35, $chunkZ) built a stronghold with no portal room",
            )
            assertTrue(pieces.all { it.box.minY > 0 - 64 }, "a piece sank below the world")
        }
    }
}
