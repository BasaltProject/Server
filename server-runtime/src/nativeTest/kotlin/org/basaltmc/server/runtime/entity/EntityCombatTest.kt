package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `LivingEntity.hurtServer` and the knockback every hit that lands is worth. */
@OptIn(ExperimentalUuidApi::class)
class EntityCombatTest {
    private fun mob(type: String = "cow"): ServerEntity =
        ServerEntity(
            entityId = 1,
            uuid = Uuid.random(),
            type = Identifier.vanilla(type),
            protocolType = 20,
            x = 0.0,
            y = 64.0,
            z = 0.0,
        )

    @Test
    fun aHitTakesHealthAndStartsTheCooldown() {
        val cow = mob()
        val before = cow.health
        val result = EntityCombat.hurt(cow, 3f)
        assertTrue(result.landed)
        assertFalse(result.died)
        assertEquals(before - 3.0, cow.health, 1e-9)
        assertEquals(EntityCombat.HURT_COOLDOWN_TICKS, cow.hurtCooldown)
    }

    @Test
    fun aSmallerHitInsideTheCooldownDoesNothing() {
        val cow = mob()
        EntityCombat.hurt(cow, 3f)
        val health = cow.health
        assertFalse(EntityCombat.hurt(cow, 2f).landed)
        assertEquals(health, cow.health, 1e-9)
    }

    @Test
    fun aBiggerHitInsideTheCooldownOnlyLandsTheDifference() {
        val cow = mob()
        EntityCombat.hurt(cow, 3f)
        val health = cow.health
        val result = EntityCombat.hurt(cow, 5f)
        assertTrue(result.landed)
        assertEquals(health - 2.0, cow.health, 1e-9)
    }

    @Test
    fun theCooldownRunsOutAndTheNextHitLandsWhole() {
        val cow = mob()
        EntityCombat.hurt(cow, 3f)
        repeat(EntityCombat.HURT_COOLDOWN_TICKS) { EntityCombat.tick(cow) }
        val health = cow.health
        assertTrue(EntityCombat.hurt(cow, 1f).landed)
        assertEquals(health - 1.0, cow.health, 1e-9)
    }

    @Test
    fun enoughDamageKills() {
        val cow = mob()
        val result = EntityCombat.hurt(cow, 100f)
        assertTrue(result.died)
        assertEquals(0.0, cow.health, 1e-9)
    }

    @Test
    fun armourTakesSomeOfItOff() {
        val zombie = mob("zombie")
        val plain = mob("cow")
        EntityCombat.hurt(zombie, 10f)
        EntityCombat.hurt(plain, 10f)
        assertTrue(20.0 - zombie.health < 10.0, "a zombie's two points of armour stop some of it")
    }

    @Test
    fun aHitPushesAwayFromWhoeverSwung() {
        val cow = mob()
        cow.onGround = true
        EntityCombat.hurt(cow, 1f, sourceX = 1.0, sourceZ = 0.0)
        assertTrue(cow.velocityX < 0.0, "hit from the east, pushed west")
        assertTrue(cow.velocityY > 0.0, "and lifted off the ground")
    }

    @Test
    fun aDeadMobCannotBeHitAgain() {
        val cow = mob()
        EntityCombat.hurt(cow, 100f)
        assertFalse(EntityCombat.hurt(cow, 1f).landed)
    }
}
