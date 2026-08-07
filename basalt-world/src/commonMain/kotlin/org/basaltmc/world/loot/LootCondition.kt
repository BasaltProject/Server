package org.basaltmc.world.loot

import org.basaltmc.random.BasaltRandom

/**
 * A predicate gating a pool or entry. Mirrors vanilla's `LootItemCondition`; only
 * `minecraft:random_chance` is implemented so far (the simplest and most common one). The rest
 * (`match_tool`, `killed_by_player`, `table_bonus`, `entity_properties`...) are a documented gap,
 * since they need a block/entity/tool context this evaluator doesn't model yet.
 */
public sealed interface LootCondition {
    public fun test(random: BasaltRandom): Boolean

    /** `random.nextFloat() < chance`: verified against the real jar's decompiled condition class. */
    public data class RandomChance(
        val chance: NumberProvider,
    ) : LootCondition {
        override fun test(random: BasaltRandom): Boolean = random.nextFloat() < chance.getFloat(random)
    }
}
