package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.entity.MobCategory
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `LocalMobCapCalculator`, independent caps for players close to each spawning chunk. */
class LocalMobCapsTest {
    @Test
    fun aChunkMaySpawnWhenAnyNearbyPlayerStillHasRoom() {
        val caps = LocalMobCaps(listOf(MobCapPlayer(1, 8.0, 8.0), MobCapPlayer(2, 248.0, 8.0)))
        repeat(MobCategory.CREATURE.maxPerChunk) { caps.add(MobCategory.CREATURE, 0, 0) }

        assertFalse(caps.canSpawn(MobCategory.CREATURE, 0, 0))
        assertTrue(caps.canSpawn(MobCategory.CREATURE, 15, 0))
        assertFalse(caps.canSpawn(MobCategory.CREATURE, 100, 100), "no nearby player means no spawning")
        assertEquals(MobCategory.CREATURE.maxPerChunk, caps.count(1, MobCategory.CREATURE))
        assertEquals(0, caps.count(2, MobCategory.CREATURE))
    }

    @Test
    fun oneMobCountsForEveryPlayerCloseToItsChunk() {
        val caps = LocalMobCaps(listOf(MobCapPlayer(1, 8.0, 8.0), MobCapPlayer(2, 24.0, 8.0)))

        caps.add(MobCategory.MONSTER, 0, 0)

        assertEquals(1, caps.count(1, MobCategory.MONSTER))
        assertEquals(1, caps.count(2, MobCategory.MONSTER))
    }

    @Test
    fun globalCapCandidatesUseTheSeventeenBySeventeenPlayerSquare() {
        val caps = LocalMobCaps(listOf(MobCapPlayer(1, 8.0, 8.0)))

        assertTrue(caps.isSpawnCandidate(8, 8))
        assertTrue(caps.isSpawnCandidate(-8, -8))
        assertFalse(caps.isSpawnCandidate(9, 0))
        assertFalse(caps.isSpawnCandidate(0, -9))
    }
}
