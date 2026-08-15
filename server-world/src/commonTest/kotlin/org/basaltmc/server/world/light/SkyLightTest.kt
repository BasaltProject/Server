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
private fun flatChunk(): Chunk {
    val chunk = Chunk(0, 0, ChunkHeight.OVERWORLD)
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

/** A lid at y = 70 with a single hole, so the only light below comes down that column. */
private fun roofExceptOneHole(
    chunk: Chunk,
    holeX: Int,
    holeZ: Int,
) {
    for (x in 0 until 16) {
        for (z in 0 until 16) {
            if (x == holeX && z == holeZ) continue
            chunk.setBlockStateId(x, 70, z, STONE)
        }
    }
}

class SkyLightTest {
    private val bounds = ChunkHeight.OVERWORLD

    @Test
    fun openSkyIsFullyLitAndSolidGroundIsNot() {
        val sections = SkyLight.compute(flatChunk()) { _, _, _ -> null }

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

        val sections = SkyLight.compute(chunk) { _, _, _ -> null }

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

        // The roof carries on into the neighbours, so the only way in is that column.
        val sections = SkyLight.compute(chunk) { _, y, _ -> if (y == 66) STONE else null }

        assertEquals(15, lightAt(sections, bounds, 15, 65, 8))
        assertEquals(14, lightAt(sections, bounds, 14, 65, 8))
        assertEquals(13, lightAt(sections, bounds, 13, 65, 8))
        assertTrue(lightAt(sections, bounds, 0, 65, 8) < 5)
    }

    @Test
    fun glassLetsTheSkyStraightThrough() {
        val chunk = flatChunk()
        for (y in 64 until 70) chunk.setBlockStateId(1, y, 1, GLASS)
        roofExceptOneHole(chunk, holeX = 1, holeZ = 1)

        val sections = SkyLight.compute(chunk) { _, y, _ -> if (y == 70) STONE else null }

        assertEquals(15, lightAt(sections, bounds, 1, 64, 1))
    }

    /** parity: water's own `lightBlock` of 1, so a column of it dims the sky one level per block. */
    @Test
    fun waterDimsTheSkyOneLevelPerBlock() {
        val chunk = flatChunk()
        for (y in 64 until 70) chunk.setBlockStateId(3, y, 3, WATER)
        roofExceptOneHole(chunk, holeX = 3, holeZ = 3)

        val sections = SkyLight.compute(chunk) { _, y, _ -> if (y == 70) STONE else null }

        assertEquals(14, lightAt(sections, bounds, 3, 69, 3))
        assertEquals(11, lightAt(sections, bounds, 3, 66, 3))
        assertEquals(9, lightAt(sections, bounds, 3, 64, 3))
    }

    /** Light comes in from the chunk next door, which is what stops a black seam at the border. */
    @Test
    fun lightArrivesFromTheNeighbouringChunk() {
        val chunk = flatChunk()
        // Roofed over entirely: without a neighbour there would be no light under it at all.
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 66, z, STONE)
        }

        val sections = SkyLight.compute(chunk) { _, y, _ -> if (y == 66) Blocks.AIR.stateId else null }

        assertEquals(14, lightAt(sections, bounds, 0, 65, 8))
        assertEquals(14, lightAt(sections, bounds, 15, 65, 8))
    }

    /** A torch lights what is around it, which is the whole of block light. parity: `BlockLightEngine`. */
    @Test
    fun aTorchLightsWhatIsAroundIt() {
        val chunk = flatChunk()
        val torch = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("torch")))
        chunk.setBlockStateId(8, 64, 8, torch)

        val sections = BlockLight.compute(chunk) { _, _, _ -> null }

        assertEquals(14, lightAt(sections, bounds, 8, 64, 8))
        assertEquals(13, lightAt(sections, bounds, 9, 64, 8))
        assertEquals(12, lightAt(sections, bounds, 10, 64, 8))
        // Eight blocks away, and gone entirely by sixteen.
        assertEquals(6, lightAt(sections, bounds, 8, 64, 0))
        assertEquals(0, lightAt(sections, bounds, 0, 64, 0))
    }

    @Test
    fun aChunkWithNothingBurningHasNoBlockLightAtAll() {
        assertEquals(false, BlockLight.hasEmitters(flatChunk()))
    }
}
