package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class PaleMossDecorationTest {
    @Test
    fun replacesGroundAndDistributesVegetation() {
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
        PaleMossDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(9182L)))

        assertTrue(countAt(chunk, 62, paleMossBlockStateId) > 0)
        assertTrue(countPlants(chunk) > 0)
    }

    @Test
    fun doesNotReplaceBlocksOutsideMossReplaceableTag() {
        val sandStateId = stateId("sand")
        val chunk = flatChunk(sandStateId)
        PaleMossDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(9182L)))

        assertEquals(256, countAt(chunk, 62, sandStateId))
        assertEquals(0, countAt(chunk, 62, paleMossBlockStateId))
    }

    @Test
    fun isDeterministicForSameSeed() {
        val first = flatChunk(Blocks.GRASS_BLOCK.stateId)
        val second = flatChunk(Blocks.GRASS_BLOCK.stateId)
        PaleMossDecoration.decorate(first, WorldgenRandom(XoroshiroRandom(77L)))
        PaleMossDecoration.decorate(second, WorldgenRandom(XoroshiroRandom(77L)))

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 61..64) assertEquals(first.getBlockStateId(x, y, z), second.getBlockStateId(x, y, z))
            }
        }
    }

    private fun flatChunk(groundStateId: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, groundStateId)
        }
        return chunk
    }

    private fun countAt(
        chunk: Chunk,
        y: Int,
        stateId: Int,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (chunk.getBlockStateId(x, y, z) == stateId) result++
        }
        return result
    }

    private fun countPlants(chunk: Chunk): Int =
        plantStateIds.sumOf { stateId -> countAt(chunk, 63, stateId) + countAt(chunk, 64, stateId) }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val paleMossBlockStateId = stateId("pale_moss_block")
    private val plantStateIds = setOf(stateId("pale_moss_carpet"), stateId("short_grass"), stateId("tall_grass"))
}
