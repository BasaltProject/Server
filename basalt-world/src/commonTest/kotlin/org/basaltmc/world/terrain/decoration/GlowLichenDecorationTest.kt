package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import kotlin.test.Test
import kotlin.test.assertTrue

class GlowLichenDecorationTest {
    @Test
    fun placesLichenOnUndergroundStoneFaces() {
        var placed = 0
        for (seed in 0L until 8L) {
            val chunk = layeredCave(water = false)
            GlowLichenDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            placed += countLichen(chunk)
        }
        assertTrue(placed > 0)
    }

    @Test
    fun lichenPlacedInWaterKeepsAFluidState() {
        var waterlogged = 0
        for (seed in 0L until 8L) {
            val chunk = layeredCave(water = true)
            GlowLichenDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..50) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        if (Blocks.isGlowLichenState(stateId) && Blocks.hasWaterFluid(stateId)) waterlogged++
                    }
                }
            }
        }
        assertTrue(waterlogged > 0)
    }

    private fun layeredCave(water: Boolean): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..62) {
                    val hollowLayer = y <= 50 && (y and 1) != 0
                    chunk.setBlockStateId(x, y, z, if (hollowLayer) if (water) Blocks.WATER.stateId else Blocks.AIR.stateId else Blocks.STONE.stateId)
                }
            }
        }
        return chunk
    }

    private fun countLichen(chunk: Chunk): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..50) if (Blocks.isGlowLichenState(chunk.getBlockStateId(x, y, z))) result++
            }
        }
        return result
    }
}
