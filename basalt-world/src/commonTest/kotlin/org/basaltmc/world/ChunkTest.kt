package org.basaltmc.world

import org.basaltmc.registry.Blocks
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class ChunkTest {
    @Test
    fun hasTheExpectedSectionCount() {
        assertEquals(WorldHeight.SECTION_COUNT, Chunk(0, 0).sectionCount)
    }

    @Test
    fun setAndGetAcrossTheFullHeightRange() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(0, WorldHeight.MIN_Y, 0, Blocks.BEDROCK.stateId)
        chunk.setBlockStateId(0, WorldHeight.MAX_Y, 0, Blocks.STONE.stateId)
        chunk.setBlockStateId(0, 0, 0, Blocks.DIRT.stateId)

        assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(0, WorldHeight.MIN_Y, 0))
        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(0, WorldHeight.MAX_Y, 0))
        assertEquals(Blocks.DIRT.stateId, chunk.getBlockStateId(0, 0, 0))
    }

    @Test
    fun rejectsYOutsideWorldBounds() {
        val chunk = Chunk(0, 0)
        assertFailsWith<IllegalArgumentException> { chunk.getBlockStateId(0, WorldHeight.MIN_Y - 1, 0) }
        assertFailsWith<IllegalArgumentException> { chunk.getBlockStateId(0, WorldHeight.MAX_Y + 1, 0) }
    }

    @Test
    fun heightmapOfAnEmptyChunkIsAllMinY() {
        val chunk = Chunk(0, 0)
        assertEquals(List(256) { WorldHeight.MIN_Y }, chunk.heightmap().toList())
    }

    @Test
    fun heightmapReflectsTheTopmostNonAirBlock() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(5, 10, 5, Blocks.STONE.stateId)
        val heightmap = chunk.heightmap()
        assertEquals(11, heightmap[5 * 16 + 5])
        assertEquals(WorldHeight.MIN_Y, heightmap[0])
    }

    @Test
    fun heightmapTypesDistinguishTheOceanFloorFromWaterSurface() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(2, 10, 3, Blocks.STONE.stateId)
        for (y in 11..20) chunk.setBlockStateId(2, y, 3, Blocks.WATER.stateId)
        chunk.setBlockStateId(2, 20, 3, Blocks.KELP.stateId)

        val index = 2 + 3 * 16
        assertEquals(11, chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR)[index])
        assertEquals(21, chunk.heightmap(Chunk.HeightmapType.WORLD_SURFACE)[index])
        assertEquals(21, chunk.heightmap(Chunk.HeightmapType.MOTION_BLOCKING)[index])
    }
}
