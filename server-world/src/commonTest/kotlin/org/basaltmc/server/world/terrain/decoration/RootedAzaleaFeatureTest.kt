package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class RootedAzaleaFeatureTest {
    @Test
    fun growsBendingAzaleaAboveTheSurfaceWithRootsThroughTheOverburden() {
        val chunk = overburdenChunk()

        assertTrue(
            RootedAzaleaFeature.place(
                chunk,
                WorldgenRandom(XoroshiroRandom(1234L)),
                DecorationPos(8, CAVE_ORIGIN_Y, 8),
            ),
        )

        assertTrue(countPath(chunk, "oak_log", SURFACE_Y + 1..SURFACE_Y + 12) > 0)
        assertTrue(
            countPath(chunk, "azalea_leaves", SURFACE_Y + 1..SURFACE_Y + 12) +
                countPath(chunk, "flowering_azalea_leaves", SURFACE_Y + 1..SURFACE_Y + 12) >
                0,
        )
        assertTrue(
            anyState(chunk, SURFACE_Y + 1..SURFACE_Y + 12) { stateId ->
                Blocks.leafDistance(stateId) in 1..6
            },
            "expected TreeFeature to update the natural leaves' log distance",
        )
        assertTrue(countPath(chunk, "rooted_dirt", CAVE_ORIGIN_Y until SURFACE_Y) > 0)
        assertTrue(countPath(chunk, "hanging_roots", CAVE_ORIGIN_Y - 2..CAVE_ORIGIN_Y + 1) > 0)
    }

    @Test
    fun failedNestedTreeDoesNotPlaceTheRootSystem() {
        val chunk = overburdenChunk()
        chunk.setBlockStateId(9, SURFACE_Y + 5, 8, Blocks.STONE.stateId)

        assertTrue(
            RootedAzaleaFeature.place(
                chunk,
                WorldgenRandom(XoroshiroRandom(1234L)),
                DecorationPos(8, CAVE_ORIGIN_Y, 8),
            ),
        )

        assertEquals(0, countPath(chunk, "oak_log", SURFACE_Y + 1..SURFACE_Y + 12))
        assertEquals(0, countPath(chunk, "rooted_dirt", CAVE_ORIGIN_Y until SURFACE_Y))
        assertEquals(0, countPath(chunk, "hanging_roots", CAVE_ORIGIN_Y - 2..CAVE_ORIGIN_Y + 1))
    }

    @Test
    fun sameSeedProducesTheSameRootSystemAndTree() {
        val first = overburdenChunk()
        val second = overburdenChunk()
        val origin = DecorationPos(8, CAVE_ORIGIN_Y, 8)

        RootedAzaleaFeature.place(first, WorldgenRandom(XoroshiroRandom(77L)), origin)
        RootedAzaleaFeature.place(second, WorldgenRandom(XoroshiroRandom(77L)), origin)

        for (x in 0 until 16) {
            for (y in CAVE_ORIGIN_Y - 2..SURFACE_Y + 12) {
                for (z in 0 until 16) {
                    assertEquals(first.getBlockStateId(x, y, z), second.getBlockStateId(x, y, z), "$x,$y,$z")
                }
            }
        }
        assertEquals(first.takeGeneratedBlockWrites(), second.takeGeneratedBlockWrites())
    }

    @Test
    fun edgeAzaleaKeepsCanopyAtExternalWorldCoordinates() {
        var externalTreeWrites = emptyList<org.basaltmc.server.world.ChunkGeneratedBlockWrite>()
        var selectedChunk: Chunk? = null
        for (seed in 0L..128L) {
            val chunk = overburdenChunk()
            RootedAzaleaFeature.place(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                DecorationPos(15, CAVE_ORIGIN_Y, 8),
            )
            externalTreeWrites =
                chunk.takeGeneratedBlockWrites().filter { write ->
                    Blocks.byStateId(write.stateId)?.block?.path in TREE_PATHS
                }
            if (externalTreeWrites.isNotEmpty()) {
                assertTrue(externalTreeWrites.all { it.x >= 16 })
                selectedChunk = chunk
                break
            }
        }
        assertTrue(externalTreeWrites.isNotEmpty(), "expected an azalea canopy to cross the eastern border")
        for (y in SURFACE_Y + 1..SURFACE_Y + 12) {
            assertTrue(Blocks.byStateId(selectedChunk!!.getBlockStateId(0, y, 8))?.block?.path !in TREE_PATHS)
        }
    }

    private fun overburdenChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in CAVE_ORIGIN_Y + 1 until SURFACE_Y) {
                        chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                    }
                    chunk.setBlockStateId(x, SURFACE_Y, z, Blocks.GRASS_BLOCK.stateId)
                }
            }
        }

    private fun countPath(
        chunk: Chunk,
        path: String,
        heights: IntRange,
    ): Int {
        val block = Identifier.vanilla(path)
        var count = 0
        for (x in 0 until 16) {
            for (y in heights) {
                for (z in 0 until 16) {
                    if (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block == block) count++
                }
            }
        }
        return count
    }

    private fun anyState(
        chunk: Chunk,
        heights: IntRange,
        predicate: (Int) -> Boolean,
    ): Boolean {
        for (x in 0 until 16) {
            for (y in heights) {
                for (z in 0 until 16) {
                    if (predicate(chunk.getBlockStateId(x, y, z))) return true
                }
            }
        }
        return false
    }

    private companion object {
        const val CAVE_ORIGIN_Y = 49
        const val SURFACE_Y = 64
        val TREE_PATHS = setOf("oak_log", "azalea_leaves", "flowering_azalea_leaves")
    }
}
