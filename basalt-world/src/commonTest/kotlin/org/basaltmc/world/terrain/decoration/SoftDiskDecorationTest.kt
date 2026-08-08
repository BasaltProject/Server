package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class SoftDiskDecorationTest {
    @Test
    fun defaultDisksReplaceOnlyUnderwaterDirtAndGrass() {
        val chunk = underwaterFloor(Blocks.DIRT.stateId)
        val untouchedStone = count(chunk, Blocks.STONE.stateId)
        SoftDiskDecoration.DEFAULT.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        assertTrue(count(chunk, stateId("sand")) > 0, "expected sand")
        assertTrue(count(chunk, stateId("gravel")) > 0, "expected gravel")
        assertEquals(untouchedStone, count(chunk, Blocks.STONE.stateId))
        assertEquals(63, chunk.heightmap(Chunk.HeightmapType.MOTION_BLOCKING).max())
    }

    @Test
    fun defaultDisksRequireWaterAtTheOceanFloorHeight() {
        val chunk = dryFloor(Blocks.DIRT.stateId)
        val dirtBefore = count(chunk, Blocks.DIRT.stateId)
        SoftDiskDecoration.DEFAULT.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        assertEquals(dirtBefore, count(chunk, Blocks.DIRT.stateId))
        assertEquals(0, count(chunk, stateId("sand")))
        assertEquals(0, count(chunk, stateId("clay")))
        assertEquals(0, count(chunk, stateId("gravel")))
    }

    @Test
    fun swampRunsOnlyItsClayDisk() {
        val chunk = underwaterFloor(Blocks.DIRT.stateId)
        SoftDiskDecoration.SWAMP.decorate(chunk, WorldgenRandom(XoroshiroRandom(99L)))

        assertTrue(count(chunk, stateId("clay")) > 0)
        assertEquals(0, count(chunk, stateId("sand")))
        assertEquals(0, count(chunk, stateId("gravel")))
    }

    @Test
    fun mangroveRunsGrassThenClayDisksOnMud() {
        val chunk = underwaterFloor(stateId("mud"))
        SoftDiskDecoration.MANGROVE.decorate(chunk, WorldgenRandom(XoroshiroRandom(99L)))

        assertTrue(count(chunk, Blocks.DIRT.stateId) > 0, "expected dirt from disk_grass")
        assertEquals(0, count(chunk, stateId("sand")))
        assertEquals(0, count(chunk, stateId("gravel")))
    }

    private fun underwaterFloor(surfaceStateId: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, FLOOR_Y - 1, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, FLOOR_Y, z, surfaceStateId)
                for (y in FLOOR_Y + 1 until 63) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun dryFloor(surfaceStateId: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, FLOOR_Y - 1, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, FLOOR_Y, z, surfaceStateId)
            }
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

    private companion object {
        const val FLOOR_Y = 40
    }
}
