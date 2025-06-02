package org.basaltmc.world.loot

import org.basaltmc.random.BasaltRandom

/**
 * A predicate gating a pool or entry. Mirrors vanilla's `LootItemCondition`. The rest
 * (`killed_by_player`, `table_bonus`, `entity_properties`...) are a documented gap, since they need
 * an entity context this evaluator doesn't model yet.
 */
public sealed interface LootCondition {
    public fun test(random: BasaltRandom): Boolean

    /** `random.nextFloat() < chance`: verified against the real jar's decompiled condition class. */
    public data class RandomChance(
        val chance: NumberProvider,
    ) : LootCondition {
        override fun test(random: BasaltRandom): Boolean = random.nextFloat() < chance.getFloat(random)
    }

    /**
     * Almost every block loot table's pool carries this, gating the whole pool on "this block was
     * not destroyed by an explosion" (each item's chance divided by the explosion's radius). This
     * evaluator has no explosion context at all, so nothing here is ever the result of one: always
     * true, which is the correct answer for every break this build currently causes (a player's
     * pick or fist, never TNT or a creeper).
     */
    public data object SurvivesExplosion : LootCondition {
        override fun test(random: BasaltRandom): Boolean = true
    }

    /**
     * Gates an alternative on the breaking tool's enchantments, almost always silk touch. Item
     * stacks on the wire carry no components yet ([org.basaltmc.protocol.play.WireItemStack]
     * refuses to read one that does), so no enchantment can exist in this build: always false. Not
     * a parity shortcut, a consequence of a gap that has to close first.
     */
    public data object MatchTool : LootCondition {
        override fun test(random: BasaltRandom): Boolean = false
    }
}
