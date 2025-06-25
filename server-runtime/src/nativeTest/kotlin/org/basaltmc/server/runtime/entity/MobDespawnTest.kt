package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `Mob.checkDespawn` and `MobCategory`'s own distances. */
@OptIn(ExperimentalUuidApi::class)
class MobDespawnTest {
    private fun mob(type: String = "zombie"): ServerEntity =
        ServerEntity(1, Uuid.random(), Identifier.vanilla(type), 1, 0.0, 64.0, 0.0)

    /** parity: past the category's despawn distance it goes at once, no roll involved. */
    @Test
    fun farAwayItGoesAtOnce() {
        val zombie = mob()

        assertTrue(MobDespawn.shouldDespawn(zombie, MobCategory.MONSTER, 129.0, LegacyRandom(1)))
        assertFalse(MobDespawn.shouldDespawn(zombie, MobCategory.MONSTER, 127.0, LegacyRandom(1)))
    }

    /** parity: inside 32 blocks it is never forgotten, and the idle clock is reset. */
    @Test
    fun closeByItStaysAndItsClockResets() {
        val zombie = mob()
        zombie.idleTicks = 5000

        assertFalse(MobDespawn.shouldDespawn(zombie, MobCategory.MONSTER, 10.0, LegacyRandom(1)))
        assertEquals(0, zombie.idleTicks, "standing next to it resets the clock")
    }

    /** parity: between the two distances it needs both ten minutes of quiet and the one in 800. */
    @Test
    fun inBetweenItNeedsTimeAndLuck() {
        val zombie = mob()
        zombie.idleTicks = 100

        assertFalse(
            MobDespawn.shouldDespawn(zombie, MobCategory.MONSTER, 60.0, alwaysRolling()),
            "ten minutes have to pass first",
        )

        zombie.idleTicks = MobDespawn.IDLE_BEFORE_RANDOM + 1
        assertTrue(MobDespawn.shouldDespawn(zombie, MobCategory.MONSTER, 60.0, alwaysRolling()))
        assertFalse(
            MobDespawn.shouldDespawn(zombie, MobCategory.MONSTER, 60.0, neverRolling()),
            "and the roll still has to come up",
        )
    }

    /** parity: `isPersistenceRequired`, which is what a name tag leaves behind. */
    @Test
    fun aPersistentMobIsNeverForgotten() {
        val zombie = mob()
        zombie.persistent = true

        assertFalse(MobDespawn.shouldDespawn(zombie, MobCategory.MONSTER, 1000.0, LegacyRandom(1)))
    }

    /** With nobody in the world at all, nothing despawns. */
    @Test
    fun anEmptyWorldForgetsNothing() {
        assertFalse(MobDespawn.shouldDespawn(mob(), MobCategory.MONSTER, null, LegacyRandom(1)))
    }

    /** parity: `MobCategory`'s own table, which is what a spawn cap counts against. */
    @Test
    fun theCategoriesAreVanillasOwn() {
        assertEquals(70, MobCategory.MONSTER.maxPerChunk)
        assertEquals(10, MobCategory.CREATURE.maxPerChunk)
        assertEquals(64, MobCategory.WATER_AMBIENT.despawnDistance, "the one that is not 128")
        assertEquals(32, MobCategory.NO_DESPAWN_DISTANCE)
        assertEquals(MobCategory.CREATURE, MobCategory.byName("creature"))
    }

    /** A random whose very next roll comes up zero, so the one in eight hundred always hits. */
    private fun alwaysRolling(): LegacyRandom = seededSoThatRollIs(0)

    /** And one whose next roll never does. */
    private fun neverRolling(): LegacyRandom = seededSoThatRollIs(1)

    private fun seededSoThatRollIs(wanted: Int): LegacyRandom {
        var seed = 0L
        while (seed < 10_000) {
            val rolled = LegacyRandom(seed).nextInt(MobDespawn.RANDOM_ODDS)
            if ((wanted == 0 && rolled == 0) || (wanted != 0 && rolled != 0)) return LegacyRandom(seed)
            seed++
        }
        error("no seed rolls $wanted")
    }
}
