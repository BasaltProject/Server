package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private fun blockId(name: String): Int = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))!!.stateId

private fun flatGrassChunk(
    chunkX: Int,
    chunkZ: Int,
    surfaceY: Int = 64,
): Chunk {
    val chunk = Chunk(chunkX, chunkZ)
    for (x in 0 until 16) {
        for (z in 0 until 16) {
            for (y in WorldHeight.MIN_Y until surfaceY) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            chunk.setBlockStateId(x, surfaceY, z, Blocks.GRASS_BLOCK.stateId)
        }
    }
    return chunk
}

class PlainsDecorationTest {
    @Test
    fun placesShortGrassSomewhereAcrossSeveralChunks() {
        val shortGrass = blockId("short_grass")
        var found = false
        for (cx in 0 until 6) {
            val chunk = flatGrassChunk(cx, 0)
            PlainsDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(cx.toLong() * 7919L)))
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    if (chunk.getBlockStateId(x, 65, z) == shortGrass) found = true
                }
            }
        }
        assertTrue(found, "expected short_grass to appear somewhere across 6 sampled chunks")
    }

    @Test
    fun placesFlowersSomewhereAcrossSeveralChunks() {
        val flowerIds =
            setOf(
                "dandelion",
                "poppy",
                "azure_bluet",
                "oxeye_daisy",
                "cornflower",
                "orange_tulip",
                "red_tulip",
                "pink_tulip",
                "white_tulip",
            ).map(::blockId)
                .toSet()
        var found = false
        for (cx in 0 until 6) {
            val chunk = flatGrassChunk(cx, 0)
            PlainsDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(cx.toLong() * 7919L)))
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    if (chunk.getBlockStateId(x, 65, z) in flowerIds) found = true
                }
            }
        }
        assertTrue(found, "expected a flower to appear somewhere across 6 sampled chunks")
    }

    @Test
    fun everyPlacedPlantStandsOnSolidGround() {
        val plantIds =
            setOf(
                "short_grass",
                "tall_grass",
                "bush",
                "dandelion",
                "poppy",
                "azure_bluet",
                "oxeye_daisy",
                "cornflower",
                "orange_tulip",
                "red_tulip",
                "pink_tulip",
                "white_tulip",
            ).map(::blockId).toSet()
        for (cx in 0 until 4) {
            val chunk = flatGrassChunk(cx, 0)
            PlainsDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(cx.toLong() * 104729L)))
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        if (chunk.getBlockStateId(x, y, z) in plantIds) {
                            val below = chunk.getBlockStateId(x, y - 1, z)
                            assertTrue(
                                below != Blocks.AIR.stateId,
                                "plant at ($x,$y,$z) in chunk ($cx,0) has air below",
                            )
                        }
                    }
                }
            }
        }
    }

    @Test
    fun isDeterministicForTheSameSeed() {
        val chunkA = flatGrassChunk(2, -1)
        val chunkB = flatGrassChunk(2, -1)
        PlainsDecoration.decorate(chunkA, WorldgenRandom(XoroshiroRandom(555L)))
        PlainsDecoration.decorate(chunkB, WorldgenRandom(XoroshiroRandom(555L)))
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    assertEquals(chunkA.getBlockStateId(x, y, z), chunkB.getBlockStateId(x, y, z))
                }
            }
        }
    }

    @Test
    fun neverPlacesOnStoneWithNoAirAbove() {
        // A chunk with no exposed surface at all (solid stone to the very top): nothing should
        // ever get placed, since every feature needs air at the target position.
        val chunk = Chunk(9, 9)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        PlainsDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(42L)))
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(x, y, z))
                }
            }
        }
    }
}
