package org.basaltmc.world

import org.basaltmc.registry.Blocks
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class NoiseWorldGeneratorTest {
    @Test
    fun everyColumnHasBedrockAtTheBottom() {
        val chunk = NoiseWorldGenerator.generate(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(x, WorldHeight.MIN_Y, z))
            }
        }
    }

    @Test
    fun producesBothSolidAndAirBlocksNotADegenerateChunk() {
        val chunk = NoiseWorldGenerator.generate(0, 0)
        var solid = 0
        var air = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    if (chunk.getBlockStateId(x, y, z) == Blocks.AIR.stateId) air++ else solid++
                }
            }
        }
        assertTrue(solid > 0, "expected at least some solid blocks")
        assertTrue(air > 0, "expected at least some air blocks")
    }

    @Test
    fun everyColumnHasAGrassSurfaceSomewhereNearSeaLevel() {
        val chunk = NoiseWorldGenerator.generate(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val hasGrass =
                    (WorldHeight.MIN_Y..WorldHeight.MAX_Y).any { y ->
                        chunk.getBlockStateId(x, y, z) ==
                            Blocks.GRASS_BLOCK.stateId
                    }
                assertTrue(hasGrass, "column ($x, $z) has no grass surface")
            }
        }
    }

    @Test
    fun differentChunksProduceDifferentTerrain() {
        val a = NoiseWorldGenerator.generate(0, 0)
        val b = NoiseWorldGenerator.generate(5, 5)
        var anyDifference = false
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    if (a.getBlockStateId(x, y, z) != b.getBlockStateId(x, y, z)) anyDifference = true
                }
            }
        }
        assertTrue(anyDifference, "expected chunk (0,0) and (5,5) to differ")
    }

    @Test
    fun isDeterministicForTheSameChunkPosition() {
        val a = NoiseWorldGenerator.generate(3, -2)
        val b = NoiseWorldGenerator.generate(3, -2)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    assertEquals(a.getBlockStateId(x, y, z), b.getBlockStateId(x, y, z))
                }
            }
        }
    }

    @Test
    fun treesAppearSomewhereAcrossSeveralChunks() {
        val foundLog =
            (0 until 10).any { i ->
                val chunk = NoiseWorldGenerator.generate(i, 0)
                (0 until 16).any { x ->
                    (0 until 16).any { z ->
                        (WorldHeight.MIN_Y..WorldHeight.MAX_Y).any { y ->
                            chunk.getBlockStateId(x, y, z) ==
                                Blocks.OAK_LOG.stateId
                        }
                    }
                }
            }
        assertTrue(foundLog, "expected at least one oak log across 10 sampled chunks")
    }

    @Test
    fun everyTreeTrunkStandsDirectlyOnGrass() {
        for (chunkX in 0 until 10) {
            val chunk = NoiseWorldGenerator.generate(chunkX, 0)
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    // The lowest oak log in the column, if any, is the trunk's base.
                    val trunkBaseY =
                        (WorldHeight.MIN_Y..WorldHeight.MAX_Y).firstOrNull { y ->
                            chunk.getBlockStateId(x, y, z) ==
                                Blocks.OAK_LOG.stateId
                        }
                            ?: continue
                    assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(x, trunkBaseY - 1, z))
                }
            }
        }
    }
}
