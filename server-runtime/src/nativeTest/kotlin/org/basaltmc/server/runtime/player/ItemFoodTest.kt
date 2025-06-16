package org.basaltmc.server.runtime.player

import org.basaltmc.server.registry.ConsumeEffect
import org.basaltmc.server.registry.ItemFood
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ItemFoodTest {
    private fun food(name: String) = checkNotNull(ItemFood.of(Identifier.vanilla(name))) { "$name is not food" }

    @Test
    fun theFoodTableHasTheRealValues() {
        val apple = food("apple")
        assertEquals(4, apple.nutrition)
        assertEquals(2.4f, apple.saturation)
        assertFalse(apple.canAlwaysEat)
        assertEquals(8, food("cooked_beef").nutrition)
    }

    /** parity: `Consumable.DEFAULT_CONSUME_SECONDS`, 1.6 seconds, which is 32 ticks. */
    @Test
    fun eatingTakesThirtyTwoTicks() {
        assertEquals(32, food("apple").consumeTicks)
    }

    /** parity: a golden apple goes down on a full stomach, an ordinary one does not. */
    @Test
    fun aGoldenAppleCanAlwaysBeEaten() {
        assertTrue(food("golden_apple").canAlwaysEat)
    }

    /** parity: a golden apple's whole point is what it applies, not what it feeds. */
    @Test
    fun aGoldenAppleAppliesRegenerationAndAbsorption() {
        val applied = food("golden_apple").effects.filterIsInstance<ConsumeEffect.Apply>()
        assertEquals(2, applied.size)
        val regeneration = applied.first { it.effect == Identifier.vanilla("regeneration") }
        assertEquals(100, regeneration.durationTicks)
        assertEquals(1, regeneration.amplifier)
        assertEquals(1f, regeneration.probability)
        assertTrue(applied.any { it.effect == Identifier.vanilla("absorption") })
    }

    /** parity: raw chicken poisons three times out of ten, not always. */
    @Test
    fun rawChickenOnlySometimesGivesHunger() {
        val applied = food("chicken").effects.filterIsInstance<ConsumeEffect.Apply>().single()
        assertEquals(Identifier.vanilla("hunger"), applied.effect)
        assertEquals(0.3f, applied.probability)
    }

    /** parity: milk feeds nobody and clears everything, and leaves the bucket behind. */
    @Test
    fun milkClearsEveryEffectAndLeavesTheBucket() {
        val milk = food("milk_bucket")
        assertEquals(0, milk.nutrition)
        assertTrue(milk.effects.single() is ConsumeEffect.ClearAll)
        assertEquals(Identifier.vanilla("bucket"), milk.remainder)
    }

    @Test
    fun honeyTakesThePoisonAwayAndLeavesTheBottle() {
        val honey = food("honey_bottle")
        val removed = honey.effects.filterIsInstance<ConsumeEffect.Remove>().single()
        assertEquals(Identifier.vanilla("poison"), removed.effect)
        assertEquals(Identifier.vanilla("glass_bottle"), honey.remainder)
        assertEquals(40, honey.consumeTicks)
    }

    @Test
    fun somethingThatIsNotFoodIsNotInTheTable() {
        assertNull(ItemFood.of(Identifier.vanilla("stone")))
        assertNull(ItemFood.of(Identifier.vanilla("diamond_sword")))
    }
}
