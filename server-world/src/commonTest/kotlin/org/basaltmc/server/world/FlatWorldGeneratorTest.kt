package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks
import kotlin.test.Test
import kotlin.test.assertEquals

class FlatWorldGeneratorTest {
    @Test
    fun layersMatchTheClassicSuperflatPreset() {
        val chunk = FlatWorldGenerator.generate(0, 0)
        assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(0, WorldHeight.MIN_Y, 0))
        assertEquals(Blocks.DIRT.stateId, chunk.getBlockStateId(0, WorldHeight.MIN_Y + 1, 0))
        assertEquals(Blocks.DIRT.stateId, chunk.getBlockStateId(0, WorldHeight.MIN_Y + 2, 0))
        assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(0, WorldHeight.MIN_Y + 3, 0))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(0, WorldHeight.MIN_Y + 4, 0))
    }

    @Test
    fun coversTheWholeSixteenBySixteenColumn() {
        val chunk = FlatWorldGenerator.generate(2, -3)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(x, WorldHeight.MIN_Y + 3, z))
            }
        }
    }

    @Test
    fun preservesTheRequestedChunkCoordinates() {
        val chunk = FlatWorldGenerator.generate(4, -7)
        assertEquals(4, chunk.chunkX)
        assertEquals(-7, chunk.chunkZ)
    }

    @Test
    fun heightmapIsFlatAtTheSurfaceEverywhere() {
        val chunk = FlatWorldGenerator.generate(0, 0)
        val expected = WorldHeight.MIN_Y + 4
        assertEquals(List(256) { expected }, chunk.heightmap().toList())
    }
}
