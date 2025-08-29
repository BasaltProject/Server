package org.basaltmc.server.world.structure

import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * A mineshaft is one chain of rolls off a single stream, so counting pieces by kind is a sharp test
 * even though it looks coarse. The expectations were read back from a real 26.2 server's own
 * `structures.starts` on seed 12345.
 */
class MineshaftLayoutTest {
    private fun layout(
        chunkX: Int,
        chunkZ: Int,
    ) = MineshaftLayout.assemble(SEED, chunkX, chunkZ, MineshaftType.NORMAL, SEA_LEVEL, MIN_Y)

    private fun histogram(pieces: List<MineshaftPiece>): Map<MineshaftPiece.Kind, Int> =
        MineshaftPiece.Kind.entries.associateWith { kind -> pieces.count { it.kind == kind } }

    @Test
    fun `a mineshaft has the pieces a real server gave it`() {
        for (expected in VANILLA) {
            val pieces = layout(expected.chunkX, expected.chunkZ)
            assertEquals(
                expected.byKind,
                histogram(pieces),
                "wrong pieces for the mineshaft at (${expected.chunkX}, ${expected.chunkZ})",
            )
        }
    }

    @Test
    fun `a mineshaft reaches exactly as far as a real server's does`() {
        for (expected in VANILLA) {
            val pieces = layout(expected.chunkX, expected.chunkZ)
            val union =
                StructureBoundingBox(
                    pieces.minOf { it.box.minX },
                    pieces.minOf { it.box.minY },
                    pieces.minOf { it.box.minZ },
                    pieces.maxOf { it.box.maxX },
                    pieces.maxOf { it.box.maxY },
                    pieces.maxOf { it.box.maxZ },
                )
            assertEquals(
                expected.union,
                union,
                "wrong extent for the mineshaft at (${expected.chunkX}, ${expected.chunkZ})",
            )
        }
    }

    @Test
    fun `the starting room sits where a real server put it`() {
        for (expected in VANILLA) {
            val room = layout(expected.chunkX, expected.chunkZ).single { it.kind == MineshaftPiece.Kind.ROOM }
            assertEquals(
                expected.room,
                room.box,
                "wrong starting room for the mineshaft at (${expected.chunkX}, ${expected.chunkZ})",
            )
        }
    }

    private class Expected(
        val chunkX: Int,
        val chunkZ: Int,
        val room: StructureBoundingBox,
        val union: StructureBoundingBox,
        corridor: Int,
        crossing: Int,
        stairs: Int,
    ) {
        val byKind =
            mapOf(
                MineshaftPiece.Kind.CORRIDOR to corridor,
                MineshaftPiece.Kind.CROSSING to crossing,
                MineshaftPiece.Kind.STAIRS to stairs,
                MineshaftPiece.Kind.ROOM to 1,
            )
    }

    private companion object {
        const val SEED = 12345L
        const val SEA_LEVEL = 63
        const val MIN_Y = -64

        val VANILLA =
            listOf(
                Expected(
                    -4,
                    1,
                    StructureBoundingBox(-62, 40, 18, -52, 48, 30),
                    StructureBoundingBox(-127, 33, -32, 28, 52, 109),
                    100,
                    32,
                    9,
                ),
                Expected(
                    4,
                    -1,
                    StructureBoundingBox(66, -44, -14, 75, -39, -5),
                    StructureBoundingBox(-9, -57, -84, 149, -37, 73),
                    81,
                    23,
                    16,
                ),
                Expected(
                    -1,
                    14,
                    StructureBoundingBox(-14, 16, 226, -5, 20, 238),
                    StructureBoundingBox(-103, 5, 162, 65, 24, 276),
                    78,
                    26,
                    9,
                ),
            )
    }
}
