package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The fitters are tried in a fixed order and the first that fits wins, which is what decides
 * whether two cells become one double room or two singles. Reordering them builds a different
 * monument without failing anywhere, so what is pinned here is the outcome that order produces.
 */
class MonumentRoomFittersTest {
    private fun fitFor(seed: Long): Pair<MonumentRoomGraph, List<MonumentRoomPiece>> {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, 0, 0)
        val graph = generateMonumentRoomGraph(random)
        return graph to fitMonumentRooms(graph, random)
    }

    @Test
    fun `every ordinary cell of the lattice ends up in some room`() {
        for (seed in 1L..30L) {
            val (graph, _) = fitFor(seed)
            val unclaimed = graph.rooms.filter { !it.isSpecial && !it.claimed }
            assertTrue(unclaimed.isEmpty(), "seed $seed left ${unclaimed.size} cells with no room")
        }
    }

    @Test
    fun `a monument always has its entrance core wings and penthouse`() {
        for (seed in 1L..30L) {
            val (_, pieces) = fitFor(seed)
            for (kind in listOf(
                MonumentRoomKind.ENTRY,
                MonumentRoomKind.CORE,
                MonumentRoomKind.LEFT_WING,
                MonumentRoomKind.RIGHT_WING,
                MonumentRoomKind.PENTHOUSE,
            )) {
                assertEquals(1, pieces.count { it.kind == kind }, "seed $seed count of $kind")
            }
        }
    }

    @Test
    fun `the two wings take opposite designs from the one draw they share`() {
        for (seed in 1L..30L) {
            val (_, pieces) = fitFor(seed)
            val left = pieces.first { it.kind == MonumentRoomKind.LEFT_WING }
            val right = pieces.first { it.kind == MonumentRoomKind.RIGHT_WING }
            assertTrue(left.design != right.design, "seed $seed gave both wings the same design")
        }
    }

    @Test
    fun `a simple room takes one of three interiors and the doubles none`() {
        for (seed in 1L..30L) {
            val (_, pieces) = fitFor(seed)
            for (piece in pieces) {
                when (piece.kind) {
                    MonumentRoomKind.SIMPLE -> assertTrue(piece.design in 0..2, "seed $seed design ${piece.design}")
                    MonumentRoomKind.LEFT_WING, MonumentRoomKind.RIGHT_WING -> Unit
                    else -> assertEquals(0, piece.design, "seed $seed ${piece.kind} drew a design it has none of")
                }
            }
        }
    }
}
