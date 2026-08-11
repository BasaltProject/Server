package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ExtraVegetationDecorationTest {
    @Test
    fun desertSugarCaneFormsTwoToFourBlockColumnsBesideWater() {
        val chunk = checkerboardBank(Blocks.WATER.stateId)
        ExtraVegetationDecoration.DESERT.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        var columns = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, SURFACE_Y + 1, z) != Blocks.SUGAR_CANE.stateId) continue
                columns++
                var height = 0
                while (chunk.getBlockStateId(x, SURFACE_Y + 1 + height, z) == Blocks.SUGAR_CANE.stateId) height++
                assertTrue(height in 2..4, "sugar cane at ($x,$z) has height $height")
                assertTrue(hasAdjacentWater(chunk, x, z), "sugar cane at ($x,$z) is not beside water")
            }
        }
        assertTrue(columns > 0, "expected desert sugar cane on the checkerboard bank")
    }

    @Test
    fun sugarCaneAcceptsFrostedIceBesideItsSubstrate() {
        val frostedIce = stateId("frosted_ice")
        val chunk = checkerboardBank(frostedIce)
        ExtraVegetationDecoration.DESERT.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        assertTrue(count(chunk, Blocks.SUGAR_CANE.stateId) > 0)
    }

    @Test
    fun sugarCaneDoesNotTreatFarmlandAsOverworldSubstrate() {
        val chunk = checkerboardBank(Blocks.WATER.stateId, stateId("farmland"))
        ExtraVegetationDecoration.DESERT.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        assertEquals(0, count(chunk, Blocks.SUGAR_CANE.stateId))
    }

    @Test
    fun mangroveNearWaterFeaturePlacesFireflyBushes() {
        var placed = 0
        for (seed in 0L until 32L) {
            val chunk = checkerboardBank(Blocks.WATER.stateId)
            ExtraVegetationDecoration.MANGROVE.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            placed += count(chunk, Blocks.FIREFLY_BUSH.stateId)
        }

        assertTrue(placed > 0, "expected a firefly bush across sampled near-water placements")
    }

    @Test
    fun pumpkinRarityEventuallyPlacesOnGrassOnly() {
        var pumpkinChunk: Chunk? = null
        for (seed in 0L until 2_000L) {
            val chunk = flatSurface(Blocks.GRASS_BLOCK.stateId)
            ExtraVegetationDecoration.PUMPKIN_ONLY.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            if (count(chunk, Blocks.PUMPKIN.stateId) > 0) {
                pumpkinChunk = chunk
                break
            }
        }

        val chunk = pumpkinChunk
        assertTrue(chunk != null, "expected the 1/300 pumpkin feature across 2,000 seeds")
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, SURFACE_Y + 1, z) == Blocks.PUMPKIN.stateId) {
                    assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(x, SURFACE_Y, z))
                }
            }
        }
    }

    private fun checkerboardBank(
        adjacentStateId: Int,
        substrateStateId: Int = Blocks.GRASS_BLOCK.stateId,
    ): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val stateId = if ((x + z) and 1 == 0) substrateStateId else adjacentStateId
                chunk.setBlockStateId(x, SURFACE_Y, z, stateId)
            }
        }
        return chunk
    }

    private fun flatSurface(stateId: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, SURFACE_Y, z, stateId)
        }
        return chunk
    }

    private fun hasAdjacentWater(
        chunk: Chunk,
        x: Int,
        z: Int,
    ): Boolean =
        listOf(x + 1 to z, x - 1 to z, x to z + 1, x to z - 1).any { (otherX, otherZ) ->
            Blocks.hasWaterFluid(chunk.getBlockStateId(otherX and 15, SURFACE_Y, otherZ and 15))
        }

    private fun count(
        chunk: Chunk,
        stateId: Int,
    ): Int =
        (0 until chunk.sectionCount).sumOf { section ->
            chunk.section(section).blockStateIds().count { it == stateId }
        }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private companion object {
        const val SURFACE_Y = 62
    }
}
