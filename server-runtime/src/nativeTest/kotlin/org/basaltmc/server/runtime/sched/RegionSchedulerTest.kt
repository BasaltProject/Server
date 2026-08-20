package org.basaltmc.server.runtime.sched

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertSame
import kotlin.test.assertTrue

/** parity of model with `TickRegions`: how many regions there are is not a setting. */
class RegionSchedulerTest {
    @Test
    fun aWorldInOnePieceIsOneRegionWhereverYouAsk() {
        val scheduler = RegionScheduler()

        val spawn = scheduler.regionFor("world", chunkX = 0, chunkZ = 0)
        val faraway = scheduler.regionFor("world", chunkX = 4000, chunkZ = -9000)

        assertSame(spawn, faraway, "the coordinates are ignored while the world is one region")
        assertEquals(1, scheduler.regionsOf("world").size)
    }

    @Test
    fun aRegionNeverSpansTwoWorlds() {
        val scheduler = RegionScheduler()

        val overworld = scheduler.regionFor("world")
        val nether = scheduler.regionFor("world_nether")

        assertFalse(overworld === nether)
        assertEquals("world", overworld.worldName)
        assertEquals("world_nether", nether.worldName)
        assertEquals(2, scheduler.regions.size)
    }

    @Test
    fun aTurnRunsTheGlobalOwnerAndThenEveryRegion() {
        val scheduler = RegionScheduler()
        scheduler.regionFor("world")
        scheduler.regionFor("world_nether")
        val order = mutableListOf<String>()
        scheduler.global.bindToCurrentThread()
        scheduler.global.submit { order += "global" }

        scheduler.tick(onFailure = { throw it }) { region -> order += region.worldName }

        assertEquals(listOf("global", "world", "world_nether"), order)
    }

    @Test
    fun whatTheGlobalOwnerLeavesForTheEndRunsAfterTheRegions() {
        val scheduler = RegionScheduler()
        scheduler.regionFor("world")
        val order = mutableListOf<String>()
        scheduler.global.bindToCurrentThread()
        scheduler.global.scheduleAtEnd { order += "global end" }

        scheduler.tick(onFailure = { throw it }) { region -> order += region.worldName }

        assertEquals(listOf("world", "global end"), order)
    }

    @Test
    fun aRegionIsTickedByWhoeverOwnsIt() {
        val scheduler = RegionScheduler()
        val region = scheduler.regionFor("world")
        var ownedWhileTicking = false

        scheduler.tick(onFailure = { throw it }) { ownedWhileTicking = it.owner.isCurrent }

        assertTrue(ownedWhileTicking, "the turn runs on the thread that claims the region")
    }

    @Test
    fun closingStopsEverything() {
        val scheduler = RegionScheduler()
        val region = scheduler.regionFor("world")

        scheduler.close()

        assertTrue(scheduler.global.isClosed)
        assertTrue(region.owner.isClosed)
    }

    @Test
    fun theSettingsAreCarriedAsTheyWereGiven() {
        val scheduler = RegionScheduler(regional = true, threads = 4)
        scheduler.regionFor("world")

        assertTrue(scheduler.regional)
        assertEquals(4, scheduler.threads)
        // Asking for regions does not make them: what splits a world is the chunks it has
        // loaded, so with none of them this is still one region.
        assertEquals(1, scheduler.regionsOf("world").size)
    }
}
