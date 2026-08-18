package org.basaltmc.server.world.loot

import org.basaltmc.server.random.BasaltRandom

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
     * Gates an alternative on the breaking tool's enchantments, almost always silk touch. Always
     * false, because this evaluator is handed a random and nothing else: which tool broke the block
     * never reaches it. Closing that means giving the loot context a tool, not a codec, so it is
     * declared here rather than guessed at.
     */
    public data object MatchTool : LootCondition {
        override fun test(random: BasaltRandom): Boolean = false
    }

    /**
     * parity: `LootItemEntityPropertyCondition` and `DamageSourcePropertiesCondition`, which ask
     * about the entity that died or what killed it: whether it was on fire, a baby, in water.
     *
     * This evaluator is handed a random and nothing else, so it answers false: a mob killed here is
     * not on fire and not a baby, which is what an ordinary kill looks like. The drops that hang on
     * it (a burning cow's cooked beef) are declared here rather than guessed at.
     */
    public data object EntityProperties : LootCondition {
        override fun test(random: BasaltRandom): Boolean = false
    }

    /**
     * parity: `LootItemKilledByPlayerCondition`. Everything that dies in this build dies to a player
     * or to a command, so this is true: it is what makes a mob drop the things only a player's kill
     * gives, like a zombie's rare armour roll.
     */
    public data object KilledByPlayer : LootCondition {
        override fun test(random: BasaltRandom): Boolean = true
    }

    /** parity: `InvertedLootItemCondition`, which is its own child answered backwards. */
    public data class Inverted(
        val inner: LootCondition,
    ) : LootCondition {
        override fun test(random: BasaltRandom): Boolean = !inner.test(random)
    }

    /** parity: `AnyOfCondition`, true when any one of its children is. */
    public data class AnyOf(
        val terms: List<LootCondition>,
    ) : LootCondition {
        override fun test(random: BasaltRandom): Boolean = terms.any { it.test(random) }
    }

    /** parity: `AllOfCondition`, true only when every one of them is. */
    public data class AllOf(
        val terms: List<LootCondition>,
    ) : LootCondition {
        override fun test(random: BasaltRandom): Boolean = terms.all { it.test(random) }
    }

    /**
     * parity: `LootItemRandomChanceWithEnchantedBonusCondition`: the plain chance, plus what the
     * killer's enchantment adds. Nothing carries an enchantment into a roll here, so it is the
     * unenchanted chance, which is what an ordinary kill rolls in vanilla too.
     */
    public data class RandomChanceWithEnchantedBonus(
        val chance: Float,
    ) : LootCondition {
        override fun test(random: BasaltRandom): Boolean = random.nextFloat() < chance
    }
}
