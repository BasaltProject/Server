package org.basaltmc.server.runtime.sched

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** What a region does and does not answer for, which is what stops two of them ticking one chunk. */
class RegionTickScopeTest {
    private fun chunks(vararg pairs: Pair<Int, Int>): List<Long> =
        pairs.map { (it.first.toLong() shl 32) or (it.second.toLong() and 0xFFFFFFFFL) }

    @Test
    fun aWorldInOnePieceOwnsEverythingWithoutKeepingASet() {
        val scheduler = RegionScheduler()
        val region = scheduler.regionFor("world")

        assertTrue(region.owns(0, 0))
        assertTrue(region.owns(100_000, -100_000), "no set to keep up to date while there is one region")
        assertTrue(region.ownsPosition(-4321.5, 987.25))
    }

    @Test
    fun eachSideOfASplitOwnsItsOwnGroundAndNoMore() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))
        val spawn = scheduler.regionFor("world", 0, 0)
        val faraway = scheduler.regionFor("world", 4000, 4000)

        assertTrue(spawn.ownsPosition(8.5, 8.5))
        assertFalse(spawn.ownsPosition(64_000.0, 64_000.0))
        assertTrue(faraway.ownsPosition(64_000.0, 64_000.0))
        assertFalse(faraway.ownsPosition(8.5, 8.5))
    }

    @Test
    fun everyChunkIsOwnedByExactlyOneRegion() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))
        val regions = scheduler.regionsOf("world")

        for (chunk in listOf(0 to 0, 5 to 5, 15 to 15, 4000 to 4000, 4009 to 4009)) {
            val owners = regions.count { it.owns(chunk.first, chunk.second) }
            assertEquals(1, owners, "chunk ${chunk.first}, ${chunk.second} is owned by $owners regions")
        }
    }

    @Test
    fun groundNobodyHasLoadedBelongsToNoRegion() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0))
        val regions = scheduler.regionsOf("world")

        // A section away from anything loaded. It gets an owner on the turn after it loads, which
        // is when the world is split again, and until then nothing ticks it.
        assertEquals(0, regions.count { it.owns(9_000, 9_000) })
    }

    @Test
    fun aNegativePositionLandsInTheChunkItReallyIsIn() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(-1 to -1))
        val region = scheduler.regionsOf("world").single()

        assertTrue(region.ownsPosition(-0.5, -0.5), "half a block west of zero is chunk -1, not chunk 0")
    }
}
