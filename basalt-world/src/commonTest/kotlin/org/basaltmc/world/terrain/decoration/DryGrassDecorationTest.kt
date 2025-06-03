package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class DryGrassDecorationTest {
    @Test
    fun desertPlacesBothEquallyWeightedDryGrassStates() {
        val counts = placeAcrossSeeds(DryGrassDecoration.DESERT)
        assertTrue(counts.first > 0, "expected short dry grass")
        assertTrue(counts.second > 0, "expected tall dry grass")
    }

    @Test
    fun badlandsPlacesBothEquallyWeightedDryGrassStates() {
        val counts = placeAcrossSeeds(DryGrassDecoration.BADLANDS)
        assertTrue(counts.first > 0, "expected short dry grass")
        assertTrue(counts.second > 0, "expected tall dry grass")
    }

    private fun placeAcrossSeeds(decoration: BiomeVegetalDecoration): Pair<Int, Int> {
        var short = 0
        var tall = 0
        for (seed in 0L until 32L) {
            val chunk = flatChunk()
            decoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            short += count(chunk, shortDryGrassStateId)
            tall += count(chunk, tallDryGrassStateId)
        }
        return short to tall
    }

    private fun flatChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
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

    private val shortDryGrassStateId = stateId("short_dry_grass")
    private val tallDryGrassStateId = stateId("tall_dry_grass")
}
