package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class MeadowFlowerDecorationTest {
    @Test
    fun dualNoiseFlowersPlaceMeadowStatePalette() {
        val chunk = flatGrassChunk()
        MeadowDecoration.FLOWERS.decorate(chunk, WorldgenRandom(XoroshiroRandom(41L)))
        assertTrue(count(chunk, meadowStates) > 0)
    }

    @Test
    fun meadowWildflowersUseBiomeNoiseDensity() {
        val chunk = flatGrassChunk()
        ForestFloorDecoration.MEADOW_WILDFLOWERS.decorate(chunk, WorldgenRandom(XoroshiroRandom(19L)))
        assertTrue(countMatching(chunk, Blocks::isWildflowersState) > 0)
    }

    @Test
    fun rareMeadowTreeSelectorPlacesFancyOakOrSuperBirch() {
        val chunk = flatGrassChunk()
        MeadowDecoration.TREES.decorate(chunk, WorldgenRandom(XoroshiroRandom(seedPassingTreeRarity())))
        assertTrue(countAll(chunk, treeLogStates) > 0)
        assertTrue(countAll(chunk, treeLeafStates) > 0)
    }

    @Test
    fun meadowTreesRejectWaterCoveredSurface() {
        val chunk = flatGrassChunk()
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 63, z, Blocks.WATER.stateId)
        }
        MeadowDecoration.TREES.decorate(chunk, WorldgenRandom(XoroshiroRandom(seedPassingTreeRarity())))
        assertEquals(0, countAll(chunk, treeLogStates))
    }

    private fun flatGrassChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
    }

    private fun count(
        chunk: Chunk,
        states: Set<Int>,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, 63, z) in states) result++
                if (chunk.getBlockStateId(x, 64, z) in states) result++
            }
        }
        return result
    }

    private fun countAll(
        chunk: Chunk,
        states: Set<Int>,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 63..90) if (chunk.getBlockStateId(x, y, z) in states) result++
            }
        }
        return result
    }

    private fun countMatching(
        chunk: Chunk,
        matches: (Int) -> Boolean,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (matches(chunk.getBlockStateId(x, 63, z))) result++
                if (matches(chunk.getBlockStateId(x, 64, z))) result++
            }
        }
        return result
    }

    private fun seedPassingTreeRarity(): Long =
        (0L..10_000L).first { seed -> WorldgenRandom(XoroshiroRandom(seed)).nextFloat() < 0.01f }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val meadowStates =
        setOf("tall_grass", "allium", "poppy", "azure_bluet", "dandelion", "cornflower", "oxeye_daisy", "short_grass")
            .map(::stateId)
            .toSet()
    private val oakLogStateId = stateId("oak_log")
    private val treeLogStates = setOf(oakLogStateId - 1, oakLogStateId, oakLogStateId + 1, stateId("birch_log"))
    private val treeLeafStates = setOf(stateId("oak_leaves"), stateId("birch_leaves"))
}
