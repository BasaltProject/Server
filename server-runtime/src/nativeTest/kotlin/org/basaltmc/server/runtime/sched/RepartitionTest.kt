package org.basaltmc.server.runtime.sched

import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** Splitting a world and putting it back together, without losing what was queued. */
class RepartitionTest {
    private fun chunks(vararg pairs: Pair<Int, Int>): List<Long> =
        pairs.map { (it.first.toLong() shl 32) or (it.second.toLong() and 0xFFFFFFFFL) }

    @Test
    fun withRegionsOffNothingIsEverSplit() {
        val scheduler = RegionScheduler(regional = false)
        scheduler.regionFor("world")

        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))

        assertEquals(1, scheduler.regionsOf("world").size, "the default mode is one region, always")
    }

    @Test
    fun chunksFarApartBecomeTwoRegions() {
        val scheduler = RegionScheduler(regional = true)

        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))

        assertEquals(2, scheduler.regionsOf("world").size)
    }

    @Test
    fun eachRegionAnswersForItsOwnChunks() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))

        val spawn = scheduler.regionFor("world", 0, 0)
        val faraway = scheduler.regionFor("world", 4000, 4000)

        assertFalse(spawn === faraway)
        assertTrue(spawn.owns(0, 0))
        assertFalse(spawn.owns(4000, 4000))
        assertTrue(faraway.owns(4000, 4000))
    }

    @Test
    fun onlyTheFirstRegionOfAWorldRunsWhatIsNotSplitYet() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))

        val primaries = scheduler.regionsOf("world").count { it.isPrimary }

        assertEquals(1, primaries, "the weather and the clock belong to the world, not to a piece of it")
    }

    /** Claimed by another thread, so submitting from this one queues instead of running here. */
    private fun claimElsewhere(owner: Owner) {
        val claimed = AtomicInt(0)
        NativeThread.start {
            owner.bindToCurrentThread()
            claimed.value = 1
        }
        var waited = 0
        while (claimed.value == 0 && waited < WAIT_LIMIT_MILLIS) {
            sleepMillis(5)
            waited += 5
        }
    }

    @Test
    fun growingTogetherMergesAndTheWorkFollows() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0, 100 to 0))
        assertEquals(2, scheduler.regionsOf("world").size)
        var ran = 0
        val faraway = scheduler.regionFor("world", 100, 0)
        claimElsewhere(faraway.owner)
        faraway.owner.submit { ran++ }
        assertEquals(1, faraway.owner.pending)

        // The chunks between them load, so the two groups touch and become one. Every sixteenth
        // chunk, because that is where one section ends and the next begins.
        scheduler.repartition("world", chunks(0 to 0, 16 to 0, 32 to 0, 48 to 0, 64 to 0, 80 to 0, 96 to 0, 100 to 0))

        val regions = scheduler.regionsOf("world")
        assertEquals(1, regions.size)
        assertEquals(1, regions.single().owner.runPending(), "what it had queued moved rather than vanishing")
        assertEquals(1, ran)
    }

    @Test
    fun aRegionThatDisappearsHandsItsWorkOver() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))
        val faraway = scheduler.regionFor("world", 4000, 4000)
        var ran = 0
        claimElsewhere(faraway.owner)
        faraway.owner.submit { ran++ }

        // Everything out there unloads: only the spawn is left.
        scheduler.repartition("world", chunks(0 to 0))

        val regions = scheduler.regionsOf("world")
        assertEquals(1, regions.size)
        assertTrue(faraway.owner.isClosed, "the region that went away runs nothing else")
        regions.single().owner.runPending()
        assertEquals(1, ran, "and its work went to the one that stayed")
    }

    @Test
    fun aWorldSplittingInTwoKeepsTicking() {
        val scheduler = RegionScheduler(regional = true)
        scheduler.repartition("world", chunks(0 to 0, 16 to 0, 32 to 0))
        assertEquals(1, scheduler.regionsOf("world").size)

        // The middle unloads, so what was one region is now two.
        scheduler.repartition("world", chunks(0 to 0, 4000 to 4000))

        assertEquals(2, scheduler.regionsOf("world").size)
        val ticked = mutableListOf<Int>()
        scheduler.tick(onFailure = { throw it }) { ticked += it.id }
        assertEquals(2, ticked.size, "both of them get a turn")
    }

    private companion object {
        const val WAIT_LIMIT_MILLIS = 2000
    }
}
