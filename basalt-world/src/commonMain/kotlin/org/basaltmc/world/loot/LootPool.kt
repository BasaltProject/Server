package org.basaltmc.world.loot

import org.basaltmc.random.BasaltRandom
import kotlin.math.floor

/**
 * One pool of a loot table: rolls a number of times, picking one weighted-random entry per roll.
 * Mirrors vanilla's `LootPool`, verified against its decompiled `addRandomItems`/`addRandomItem`:
 * entry conditions are re-evaluated on every single roll (not once up front), and the weighted pick
 * is `random.nextInt(totalWeight)` then a linear scan subtracting weights until negative
 * (`WeightedRandom.getRandomItem`), not any other algorithm.
 */
public data class LootPool(
    val entries: List<LootEntry>,
    val rolls: NumberProvider = NumberProvider.Constant(1f),
    val bonusRolls: NumberProvider = NumberProvider.Constant(0f),
    val conditions: List<LootCondition> = emptyList(),
    val functions: List<LootFunction> = emptyList(),
) {
    public fun generate(
        random: BasaltRandom,
        luck: Float,
        output: (ItemStack) -> Unit,
    ) {
        if (!conditions.all { it.test(random) }) return

        val count = rolls.getInt(random) + floor(bonusRolls.getFloat(random) * luck).toInt()
        repeat(count) { addRandomItem(random, luck, output) }
    }

    private fun addRandomItem(
        random: BasaltRandom,
        luck: Float,
        output: (ItemStack) -> Unit,
    ) {
        val validEntries = mutableListOf<LootEntry>()
        var totalWeight = 0
        for (entry in entries) {
            if (!entry.canRun(random)) continue
            val weight = entry.weightFor(luck)
            if (weight > 0) {
                validEntries += entry
                totalWeight += weight
            }
        }
        if (totalWeight == 0 || validEntries.isEmpty()) return

        val chosen =
            if (validEntries.size == 1) {
                validEntries[0]
            } else {
                var index = random.nextInt(totalWeight)
                validEntries.first { entry ->
                    index -= entry.weightFor(luck)
                    index < 0
                }
            }

        var stack = chosen.createItemStack(random)
        for (function in functions) stack = function.apply(stack, random)
        output(stack)
    }
}
