package org.basaltmc.server.runtime.player

import org.basaltmc.server.registry.ItemArmour
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class CombatTest {
    private fun assertClose(
        expected: Float,
        actual: Float,
    ) = assertTrue(kotlin.math.abs(expected - actual) < 0.0001f, "expected $expected but was $actual")

    /** parity: a full set of iron is 15 armour, and a ten point hit eats five of it before it counts. */
    @Test
    fun ironArmourTakesFortyPercentOffATenPointHit() {
        assertClose(6f, Combat.afterArmour(damage = 10f, armour = 15f, toughness = 0f))
    }

    /** parity: armour is worth less against a big hit, because the hit eats into it first. */
    @Test
    fun armourIsWorthLessAgainstABigHit() {
        val small = Combat.afterArmour(damage = 4f, armour = 20f, toughness = 0f) / 4f
        val big = Combat.afterArmour(damage = 40f, armour = 20f, toughness = 0f) / 40f
        assertTrue(big > small, "a big hit should get through more of the armour, not less")
    }

    /** parity: `MIN_ARMOUR_RATIO`, the fifth of the armour a hit can never eat through. */
    @Test
    fun armourNeverStopsCountingCompletely() {
        assertClose(0.84f, Combat.afterArmour(damage = 1000f, armour = 20f, toughness = 0f) / 1000f)
    }

    @Test
    fun toughnessKeepsArmourWorthMoreAgainstBigHits() {
        val plain = Combat.afterArmour(damage = 30f, armour = 20f, toughness = 0f)
        val tough = Combat.afterArmour(damage = 30f, armour = 20f, toughness = 8f)
        assertTrue(tough < plain, "toughness should let armour hold up better")
    }

    /** parity: resistance takes a fifth off per level and nothing at all above level four. */
    @Test
    fun resistanceCountsInFifths() {
        assertClose(8f, Combat.afterResistance(damage = 10f, amplifier = 0))
        assertClose(6f, Combat.afterResistance(damage = 10f, amplifier = 1))
        assertClose(0f, Combat.afterResistance(damage = 10f, amplifier = 4))
        assertClose(0f, Combat.afterResistance(damage = 10f, amplifier = 9))
    }

    @Test
    fun protectionIsCappedAtTwenty() {
        assertClose(2f, Combat.afterProtection(damage = 10f, protection = 20f))
        assertClose(2f, Combat.afterProtection(damage = 10f, protection = 100f))
    }

    /** parity: three blocks are free, and every one after that is half a heart. */
    @Test
    fun aFallCostsHalfAHeartPerBlockPastThree() {
        assertEquals(0, Combat.fallDamage(0.0))
        assertEquals(0, Combat.fallDamage(3.0))
        assertEquals(1, Combat.fallDamage(4.0))
        assertEquals(20, Combat.fallDamage(23.0))
    }

    @Test
    fun theArmourTableHasTheRealValues() {
        val chestplate = checkNotNull(ItemArmour.of(Identifier.vanilla("diamond_chestplate")))
        assertEquals(8f, chestplate.armour)
        assertEquals(2f, chestplate.toughness)
        assertEquals("chest", chestplate.slot)
        assertEquals(2f, checkNotNull(ItemArmour.of(Identifier.vanilla("iron_helmet"))).armour)
    }

    @Test
    fun theDifficultyDecidesHowMuchOfAMobsHitLands() {
        assertEquals(0f, Combat.afterDifficulty(6f, "peaceful"))
        assertEquals(4f, Combat.afterDifficulty(6f, "easy"))
        assertEquals(6f, Combat.afterDifficulty(6f, "normal"))
        assertEquals(9f, Combat.afterDifficulty(6f, "hard"))
    }

    @Test
    fun easyNeverMakesAHitBigger() {
        assertEquals(1f, Combat.afterDifficulty(1f, "easy"))
    }

    @Test
    fun aSwingRechargesOverItsOwnDelay() {
        // A bare hand swings four times a second, so five ticks is a full swing.
        assertEquals(1f, Combat.attackStrengthScale(20, 4.0))
        assertTrue(Combat.attackStrengthScale(0, 4.0) < 0.2f)
        assertEquals(1f, Combat.attackStrengthScale(5, 4.0))
    }

    @Test
    fun anUnchargedSwingStillLandsAFifthOfIt() {
        assertEquals(0.2f, Combat.chargeScale(0f))
        assertEquals(1f, Combat.chargeScale(1f))
    }

    @Test
    fun aPushTakesHalfTheSpeedAndSendsItTheOtherWay() {
        val moved =
            Combat.knockback(
                power = 0.4,
                towardsX = 1.0,
                towardsZ = 0.0,
                velocityX = 0.2,
                velocityY = 0.0,
                velocityZ = 0.0,
                onGround = true,
            )
        assertEquals(0.1 - 0.4, moved[0], 1e-9)
        assertEquals(0.4, moved[1], 1e-9)
    }
}
