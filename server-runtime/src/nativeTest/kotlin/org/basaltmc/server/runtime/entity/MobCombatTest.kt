package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `NearestAttackableTargetGoal` and `MeleeAttackGoal`. */
@OptIn(ExperimentalUuidApi::class)
class MobCombatTest {
    private fun mob(
        type: String = "zombie",
        x: Double = 0.0,
        z: Double = 0.0,
    ): ServerEntity =
        ServerEntity(
            entityId = 1,
            uuid = Uuid.random(),
            type = Identifier.vanilla(type),
            protocolType = 20,
            x = x,
            y = 64.0,
            z = z,
        )

    private fun target(
        x: Double,
        z: Double,
        y: Double = 64.0,
    ) = MobCombat.Target(99, x, y, z, 0.6, 1.8)

    /** Rolls until the one-in-ten check lets the goal look, which is what vanilla does too. */
    private fun acquire(
        mob: ServerEntity,
        targets: List<MobCombat.Target>,
        light: Int = 0,
    ): MobCombat.Target? {
        val random = LegacyRandom(1L)
        repeat(200) {
            MobCombat.acquire(mob, targets, random, light)?.let { return it }
        }
        return null
    }

    @Test
    fun onlyTheMobsThatHuntDoHunt() {
        assertTrue(MobCombat.isMelee(Identifier.vanilla("zombie")))
        assertFalse(MobCombat.isMelee(Identifier.vanilla("cow")))
        assertFalse(MobCombat.isMelee(Identifier.vanilla("skeleton")), "a skeleton shoots instead")
        assertFalse(MobCombat.isMelee(Identifier.vanilla("creeper")), "a creeper explodes instead")
    }

    @Test
    fun theNearestTargetInRangeWins() {
        val zombie = mob()
        val far = target(20.0, 0.0)
        val near = target(4.0, 0.0)
        assertEquals(near, acquire(zombie, listOf(far, near)))
    }

    @Test
    fun nobodyOutsideTheFollowRangeIsPicked() {
        assertNull(acquire(mob(), listOf(target(100.0, 0.0))))
    }

    @Test
    fun aSpiderInTheLightNoticesNobody() {
        val spider = mob("spider")
        assertNotNull(acquire(spider, listOf(target(3.0, 0.0)), light = 0))
        assertNull(acquire(spider, listOf(target(3.0, 0.0)), light = 15))
    }

    @Test
    fun reachIsTheTwoBoxesTouching() {
        val zombie = mob()
        assertTrue(MobCombat.withinReach(zombie, target(1.0, 0.0)))
        assertFalse(MobCombat.withinReach(zombie, target(3.0, 0.0)))
        assertFalse(MobCombat.withinReach(zombie, target(1.0, 0.0, y = 70.0)), "out of reach above it")
    }
}
