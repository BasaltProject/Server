package org.basaltmc.server.runtime.sched

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** parity of model with `ThreadedRegionizer`: sections that touch are one region. */
class RegionMapTest {
    private fun sections(vararg pairs: Pair<Int, Int>): Set<Long> =
        pairs.map { RegionMap.sectionKey(it.first, it.second) }.toSet()

    @Test
    fun aChunkFallsInTheSectionSixteenChunksWide() {
        assertEquals(0, RegionMap.sectionOf(0))
        assertEquals(0, RegionMap.sectionOf(15))
        assertEquals(1, RegionMap.sectionOf(16))
        assertEquals(-1, RegionMap.sectionOf(-1), "negative chunks belong to the section below zero")
        assertEquals(-1, RegionMap.sectionOf(-16))
        assertEquals(-2, RegionMap.sectionOf(-17))
    }

    @Test
    fun nothingLoadedIsNoRegions() {
        assertEquals(emptyList(), RegionMap.components(emptySet()))
    }

    @Test
    fun sectionsSideBySideAreOneRegion() {
        val groups = RegionMap.components(sections(0 to 0, 1 to 0, 2 to 0))

        assertEquals(1, groups.size)
        assertEquals(3, groups.single().size)
    }

    @Test
    fun touchingOnlyAtACornerStillCountsAsTouching() {
        val groups = RegionMap.components(sections(0 to 0, 1 to 1))

        assertEquals(1, groups.size, "what interacts across a diagonal has to tick together")
    }

    @Test
    fun sectionsFarApartAreTwoRegions() {
        val groups = RegionMap.components(sections(0 to 0, 40 to 40))

        assertEquals(2, groups.size)
    }

    @Test
    fun theBiggestGroupComesFirst() {
        val groups = RegionMap.components(sections(0 to 0, 40 to 0, 41 to 0, 42 to 0))

        assertEquals(3, groups.first().size, "so a merge keeps the region that already owns most")
        assertEquals(1, groups.last().size)
    }

    @Test
    fun aSectionKeySurvivesTheRoundTrip() {
        val key = RegionMap.sectionKey(-7, 12)

        assertEquals(-7, RegionMap.sectionX(key))
        assertEquals(12, RegionMap.sectionZ(key))
        assertTrue(RegionMap.sectionKeyOfChunk(-100, 200) == RegionMap.sectionKey(-7, 12))
    }
}
