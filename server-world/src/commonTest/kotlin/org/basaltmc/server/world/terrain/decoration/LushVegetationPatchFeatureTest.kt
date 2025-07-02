package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertTrue

class LushVegetationPatchFeatureTest {
    @Test
    fun floorMossReplacesTheGroundAndDistributesVegetationAboveIt() {
        val chunk = caveChunk()
        LushVegetationPatchFeature.placeFloorMoss(
            chunk,
            WorldgenRandom(XoroshiroRandom(1234L)),
            DecorationPos(8, FLOOR_Y + 1, 8),
        )

        assertTrue(countBlock(chunk, FLOOR_Y, "moss_block") > 0)
        assertTrue(
            listOf("flowering_azalea", "azalea", "moss_carpet", "short_grass", "tall_grass")
                .sumOf { path -> countBlock(chunk, FLOOR_Y + 1, path) } > 0,
        )
    }

    @Test
    fun ceilingMossReplacesTheCeilingAndHangsCaveVinesBelowIt() {
        var foundVines = false
        for (seed in 0L..100L) {
            val chunk = caveChunk()
            LushVegetationPatchFeature.placeCeilingMoss(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                DecorationPos(8, CEILING_Y - 1, 8),
            )
            assertTrue(countBlock(chunk, CEILING_Y, "moss_block") > 0)
            foundVines =
                (FLOOR_Y + 1 until CEILING_Y).any { y ->
                    countBlock(chunk, y, "cave_vines") + countBlock(chunk, y, "cave_vines_plant") > 0
                }
            if (foundVines) break
        }
        assertTrue(foundVines, "expected sampled ceiling moss to grow cave vines")
    }

    @Test
    fun clayPatchReplacesLushGroundAtTheScannedFloor() {
        val chunk = caveChunk()
        LushVegetationPatchFeature.placeClay(
            chunk,
            WorldgenRandom(XoroshiroRandom(3L)),
            DecorationPos(8, FLOOR_Y + 1, 8),
        )
        assertTrue(countBlock(chunk, FLOOR_Y, "clay") > 0)
    }

    private fun caveChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    chunk.setBlockStateId(x, FLOOR_Y - 1, z, Blocks.STONE.stateId)
                    chunk.setBlockStateId(x, FLOOR_Y, z, Blocks.STONE.stateId)
                    chunk.setBlockStateId(x, CEILING_Y, z, Blocks.STONE.stateId)
                    chunk.setBlockStateId(x, CEILING_Y + 1, z, Blocks.STONE.stateId)
                }
            }
        }

    private fun countBlock(
        chunk: Chunk,
        y: Int,
        path: String,
    ): Int {
        val block = Identifier.vanilla(path)
        var count = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block == block) count++
            }
        }
        return count
    }

    private companion object {
        const val FLOOR_Y = 30
        const val CEILING_Y = 50
    }
}
