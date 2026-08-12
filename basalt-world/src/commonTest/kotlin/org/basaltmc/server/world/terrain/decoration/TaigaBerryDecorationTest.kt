package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class TaigaBerryDecorationTest {
    @Test
    fun commonBerryPatchPlacesAgeThreeBushes() {
        val seed = seedPassingRarity(32)
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
        TaigaGroundDecoration.COMMON_BERRY.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
        assertTrue(countSurface(chunk) > 0)
    }

    @Test
    fun rareBerryPatchUsesItsOneIn384Filter() {
        val seed = seedPassingRarity(384)
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
        TaigaGroundDecoration.RARE_BERRY.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
        assertTrue(countSurface(chunk) > 0)
    }

    @Test
    fun berryPatchRequiresGrassBlock() {
        val chunk = flatChunk(Blocks.DIRT.stateId)
        TaigaGroundDecoration.COMMON_BERRY.decorate(chunk, WorldgenRandom(XoroshiroRandom(seedPassingRarity(32))))
        assertEquals(0, countSurface(chunk))
    }

    private fun seedPassingRarity(chance: Int): Long =
        (0L..100_000L).first { seed -> WorldgenRandom(XoroshiroRandom(seed)).nextFloat() < 1.0f / chance }

    private fun flatChunk(groundStateId: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, groundStateId)
        }
        return chunk
    }

    private fun countSurface(chunk: Chunk): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (chunk.getBlockStateId(x, 63, z) == sweetBerryAge3StateId) result++
        }
        return result
    }

    private val sweetBerryAge3StateId =
        (
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sweet_berry_bush"))?.stateId
                ?: error("unknown block 'sweet_berry_bush'")
        ) + 3
}
