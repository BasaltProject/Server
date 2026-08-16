package org.basaltmc.server.world.light

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private val STONE = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone")))
private val GLASS = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("glass")))
private val WATER = Blocks.WATER.stateId

/** A flat floor at y = 64, which is what most of these start from. */
private fun flatChunk(
    chunkX: Int = 0,
    chunkZ: Int = 0,
): Chunk {
    val chunk = Chunk(chunkX, chunkZ, ChunkHeight.OVERWORLD)
    for (x in 0 until 16) {
        for (z in 0 until 16) {
            for (y in ChunkHeight.OVERWORLD.minY until 64) chunk.setBlockStateId(x, y, z, STONE)
        }
    }
    return chunk
}

private fun lightAt(
    sections: Array<IntArray>,
    bounds: ChunkHeight,
    x: Int,
    y: Int,
    z: Int,
): Int {
    val index = (y - bounds.minY) / 16
    val localY = (y - bounds.minY) % 16
    return sections[index][(localY * 16 + z) * 16 + x]
}

/** The chunk on its own: every neighbour missing, which reads as open sky. */
private fun window(chunk: Chunk): LightWindow = LightWindow(chunk) { _, _ -> null }

/** A lid at [y] over the whole chunk, with at most one hole in it. */
private fun roof(
    chunk: Chunk,
    y: Int,
    holeX: Int = -1,
    holeZ: Int = -1,
) {
    for (x in 0 until 16) {
        for (z in 0 until 16) {
            if (x == holeX && z == holeZ) continue
            chunk.setBlockStateId(x, y, z, STONE)
        }
    }
}

/** The chunk, with eight neighbours built the same way: a roof that does not stop at the border. */
private fun roofedWindow(
    chunk: Chunk,
    y: Int,
): LightWindow {
    val neighbours = mutableMapOf<Pair<Int, Int>, Chunk>()
    return LightWindow(chunk) { chunkX, chunkZ ->
        neighbours.getOrPut(chunkX to chunkZ) { flatChunk(chunkX, chunkZ).also { roof(it, y) } }
    }
}

class SkyLightTest {
    private val bounds = ChunkHeight.OVERWORLD

    @Test
    fun openSkyIsFullyLitAndSolidGroundIsNot() {
        val sections = SkyLight.compute(window(flatChunk()))

        assertEquals(15, lightAt(sections, bounds, 8, 64, 8))
        assertEquals(15, lightAt(sections, bounds, 8, 200, 8))
        assertEquals(0, lightAt(sections, bounds, 8, 63, 8))
    }

    /**
     * The bug this engine replaced: light was uniform per section, so one tall column left every
     * section under it black across the whole chunk. An End pillar did exactly this.
     */
    @Test
    fun aTowerOnlyShadowsWhatIsUnderIt() {
        val chunk = flatChunk()
        for (y in 64 until 120) chunk.setBlockStateId(8, y, 8, STONE)

        val sections = SkyLight.compute(window(chunk))

        assertEquals(0, lightAt(sections, bounds, 8, 100, 8))
        assertEquals(15, lightAt(sections, bounds, 2, 100, 2))
        assertEquals(15, lightAt(sections, bounds, 15, 70, 15))
    }

    /** parity: `LayerLightEngine`, one level per block away from the light. */
    @Test
    fun lightCreepsUnderAnOverhangAndFadesWithDistance() {
        val chunk = flatChunk()
        // A roof at y = 66 over the whole chunk but the last column, so light comes in sideways.
        for (x in 0 until 15) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 66, z, STONE)
        }

        // The roof carries on into the neighbours, so that column is the only way in.
        val sections = SkyLight.compute(roofedWindow(chunk, y = 66))

        assertEquals(15, lightAt(sections, bounds, 15, 65, 8))
        assertEquals(14, lightAt(sections, bounds, 14, 65, 8))
        assertEquals(13, lightAt(sections, bounds, 13, 65, 8))
        assertTrue(lightAt(sections, bounds, 0, 65, 8) < 5)
    }

    @Test
    fun glassLetsTheSkyStraightThrough() {
        val chunk = flatChunk()
        for (y in 64 until 70) chunk.setBlockStateId(1, y, 1, GLASS)
        roof(chunk, y = 70, holeX = 1, holeZ = 1)

        val sections = SkyLight.compute(roofedWindow(chunk, y = 70))

        assertEquals(15, lightAt(sections, bounds, 1, 64, 1))
    }

    /** parity: water's own `lightBlock` of 1, so a column of it dims the sky one level per block. */
    @Test
    fun waterDimsTheSkyOneLevelPerBlock() {
        val chunk = flatChunk()
        for (y in 64 until 70) chunk.setBlockStateId(3, y, 3, WATER)
        roof(chunk, y = 70, holeX = 3, holeZ = 3)

        val sections = SkyLight.compute(roofedWindow(chunk, y = 70))

        assertEquals(14, lightAt(sections, bounds, 3, 69, 3))
        assertEquals(11, lightAt(sections, bounds, 3, 66, 3))
        assertEquals(9, lightAt(sections, bounds, 3, 64, 3))
    }

    /** Light comes in from the chunk next door, which is what stops a black seam at the border. */
    @Test
    fun lightArrivesFromTheNeighbouringChunk() {
        val chunk = flatChunk()
        // Roofed over entirely, with open neighbours: the only light comes in from next door.
        roof(chunk, y = 66)

        val open = mutableMapOf<Pair<Int, Int>, Chunk>()
        val sections =
            SkyLight.compute(LightWindow(chunk) { cx, cz -> open.getOrPut(cx to cz) { flatChunk(cx, cz) } })

        assertEquals(14, lightAt(sections, bounds, 0, 65, 8))
        assertEquals(14, lightAt(sections, bounds, 15, 65, 8))
    }

    /** A torch lights what is around it, which is the whole of block light. parity: `BlockLightEngine`. */
    @Test
    fun aTorchLightsWhatIsAroundIt() {
        val chunk = flatChunk()
        val torch = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("torch")))
        chunk.setBlockStateId(8, 64, 8, torch)

        val sections = checkNotNull(BlockLight.compute(window(chunk)))

        assertEquals(14, lightAt(sections, bounds, 8, 64, 8))
        assertEquals(13, lightAt(sections, bounds, 9, 64, 8))
        assertEquals(12, lightAt(sections, bounds, 10, 64, 8))
        // Eight blocks away, and gone entirely by sixteen.
        assertEquals(6, lightAt(sections, bounds, 8, 64, 0))
        assertEquals(0, lightAt(sections, bounds, 0, 64, 0))
    }

    @Test
    fun aChunkWithNothingBurningHasNoBlockLightAtAll() {
        assertEquals(null, BlockLight.compute(window(flatChunk())))
    }

    /**
     * The bug from the cave screenshots: the opening is in the chunk next door, ten blocks away.
     * With a one block margin no light arrived and the cave was black until a block was placed;
     * light reaches fifteen blocks, so the window has to be the nine chunks vanilla lights with.
     */
    @Test
    fun lightArrivesFromAnOpeningInTheNextChunk() {
        // Solid rock from y = 64 up to y = 80, with a corridor at y = 65 running east.
        fun carved(
            chunkX: Int,
            chunkZ: Int,
        ): Chunk {
            val chunk = Chunk(chunkX, chunkZ, ChunkHeight.OVERWORLD)
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in ChunkHeight.OVERWORLD.minY..80) chunk.setBlockStateId(x, y, z, STONE)
                    chunk.setBlockStateId(x, 65, z, Blocks.AIR.stateId)
                }
            }
            return chunk
        }

        val centre = carved(0, 0)
        val built = mutableMapOf<Pair<Int, Int>, Chunk>()
        val window =
            LightWindow(centre) { chunkX, chunkZ ->
                built.getOrPut(chunkX to chunkZ) {
                    carved(chunkX, chunkZ).also { chunk ->
                        // The mouth: one open column down to the corridor, in the chunk to the west.
                        if (chunkX == -1 && chunkZ == 0) {
                            for (y in 65..80) chunk.setBlockStateId(14, y, 8, Blocks.AIR.stateId)
                        }
                    }
                }
            }

        val sections = SkyLight.compute(window)

        // Two blocks into this chunk, six blocks from the mouth: dim, but not black.
        assertTrue(lightAt(sections, bounds, 1, 65, 8) > 0, "the corridor is black at x = 1")
        assertTrue(lightAt(sections, bounds, 1, 65, 8) < 15)
        assertEquals(0, lightAt(sections, bounds, 15, 65, 8))
    }
}
