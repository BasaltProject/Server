package org.basaltmc.world

import org.basaltmc.registry.Blocks
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class ChunkSectionTest {
    @Test
    fun defaultsToAirEverywhere() {
        val section = ChunkSection()
        assertEquals(Blocks.AIR.stateId, section.getBlockStateId(0, 0, 0))
        assertEquals(Blocks.AIR.stateId, section.getBlockStateId(15, 15, 15))
        assertEquals(0, section.nonAirBlockCount)
    }

    @Test
    fun setAndGetRoundTrip() {
        val section = ChunkSection()
        section.setBlockStateId(3, 7, 11, Blocks.STONE.stateId)
        assertEquals(Blocks.STONE.stateId, section.getBlockStateId(3, 7, 11))
        assertEquals(1, section.nonAirBlockCount)
    }

    @Test
    fun caveAndVoidAirDoNotCountAsNonAirBlocks() {
        val section = ChunkSection()
        section.setBlockStateId(1, 2, 3, Blocks.CAVE_AIR.stateId)
        section.setBlockStateId(4, 5, 6, Blocks.VOID_AIR.stateId)
        assertEquals(0, section.nonAirBlockCount)
    }

    @Test
    fun distinctCoordinatesDoNotAlias() {
        val section = ChunkSection()
        section.setBlockStateId(0, 0, 1, Blocks.STONE.stateId)
        section.setBlockStateId(1, 0, 0, Blocks.DIRT.stateId)
        assertEquals(Blocks.AIR.stateId, section.getBlockStateId(0, 0, 0))
        assertEquals(Blocks.STONE.stateId, section.getBlockStateId(0, 0, 1))
        assertEquals(Blocks.DIRT.stateId, section.getBlockStateId(1, 0, 0))
    }

    @Test
    fun rejectsOutOfBoundsCoordinates() {
        val section = ChunkSection()
        assertFailsWith<IllegalArgumentException> { section.getBlockStateId(16, 0, 0) }
        assertFailsWith<IllegalArgumentException> { section.getBlockStateId(0, -1, 0) }
    }

    @Test
    fun countsWaterAndWaterPlantsAsNonEmptyFluids() {
        val section = ChunkSection()
        section.setBlockStateId(0, 0, 0, Blocks.WATER.stateId)
        section.setBlockStateId(1, 0, 0, Blocks.SEAGRASS.stateId)
        assertEquals(2, section.nonEmptyFluidCount)
    }
}
