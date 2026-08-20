package org.basaltmc.server.runtime.entity

import org.basaltmc.server.runtime.entity.ai.VillagerSchedule
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertSame

/** parity: the `villager_activity` keyframes and `Raid.RaiderType`'s wave table. */
class RaidTest {
    @Test
    fun theVillagerScheduleFollowsTheKeyframes() {
        assertEquals(VillagerSchedule.Activity.IDLE, VillagerSchedule.activityAt(0))
        assertEquals(VillagerSchedule.Activity.IDLE, VillagerSchedule.activityAt(1999))
        assertEquals(VillagerSchedule.Activity.WORK, VillagerSchedule.activityAt(2000))
        assertEquals(VillagerSchedule.Activity.WORK, VillagerSchedule.activityAt(8999))
        assertEquals(VillagerSchedule.Activity.MEET, VillagerSchedule.activityAt(9000))
        assertEquals(VillagerSchedule.Activity.IDLE, VillagerSchedule.activityAt(11000))
        assertEquals(VillagerSchedule.Activity.REST, VillagerSchedule.activityAt(12000))
        assertEquals(VillagerSchedule.Activity.REST, VillagerSchedule.activityAt(23999))
    }

    @Test
    fun theScheduleRepeatsEveryDay() {
        assertEquals(VillagerSchedule.activityAt(2000), VillagerSchedule.activityAt(24000 * 7 + 2000))
    }

    @Test
    fun everyWaveBringsWhatTheTableSays() {
        assertEquals(4, RaidWaves.countFor("pillager", 1))
        assertEquals(2, RaidWaves.countFor("vindicator", 2))
        assertEquals(1, RaidWaves.countFor("ravager", 3))
        assertEquals(3, RaidWaves.countFor("witch", 4))
        assertEquals(1, RaidWaves.countFor("evoker", 5))
        assertEquals(0, RaidWaves.countFor("evoker", 1))
        assertEquals(0, RaidWaves.countFor("creeper", 1))
    }

    @Test
    fun theDifficultyDecidesHowManyWavesThereAre() {
        assertEquals(0, RaidWaves.groupsFor("peaceful"))
        assertEquals(3, RaidWaves.groupsFor("easy"))
        assertEquals(5, RaidWaves.groupsFor("normal"))
        assertEquals(7, RaidWaves.groupsFor("hard"))
    }

    @Test
    fun aSecondOmenExtendsTheRaidItWalksInto() {
        val manager = RaidManager()
        val first = assertNotNull(manager.createOrExtend(0, 64, 0, amplifier = 0, difficulty = "normal"))
        assertEquals(1, first.omenLevel)
        assertEquals(5, first.totalGroups)

        val second = assertNotNull(manager.createOrExtend(8, 64, 8, amplifier = 1, difficulty = "normal"))
        assertSame(first, second)
        assertEquals(3, first.omenLevel)
        assertEquals(1, manager.all.size)
    }

    @Test
    fun theOmenLevelStopsAtFive() {
        val manager = RaidManager()
        repeat(6) { manager.createOrExtend(0, 64, 0, amplifier = 1, difficulty = "hard") }
        assertEquals(Raid.MAX_OMEN_LEVEL, manager.all.single().omenLevel)
    }

    @Test
    fun peacefulHasNoRaids() {
        val manager = RaidManager()
        assertNull(manager.createOrExtend(0, 64, 0, amplifier = 0, difficulty = "peaceful"))
        assertEquals(0, manager.all.size)
    }

    @Test
    fun aRaidIsOnlyNearbyInsideItsOwnRadius() {
        val manager = RaidManager()
        manager.createOrExtend(0, 64, 0, amplifier = 0, difficulty = "normal")
        assertNotNull(manager.nearby(90.0, 64.0, 0.0))
        assertNull(manager.nearby(97.0, 64.0, 0.0))
    }
}
