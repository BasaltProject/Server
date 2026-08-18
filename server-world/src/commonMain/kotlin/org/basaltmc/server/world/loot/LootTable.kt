package org.basaltmc.server.world.loot

import org.basaltmc.server.random.BasaltRandom

/**
 * A full loot table: every pool rolled in order, with table-level functions applied to each item
 * the moment it's produced (not batched after every pool finishes). Mirrors vanilla's
 * `LootTable.getRandomItemsRaw`, which decorates the per-pool output consumer with the table's
 * composite function before handing it to each pool, so RNG-consuming table functions interleave
 * with pool rolls in the same order a real client/server would see.
 */
public data class LootTable(
    val pools: List<LootPool>,
    val functions: List<LootFunction> = emptyList(),
) {
    public fun generate(
        random: BasaltRandom,
        luck: Float = 0f,
        /** parity: `LootContextParams.EXPLOSION_RADIUS`, which is what `explosion_decay` reads. */
        explosionRadius: Float? = null,
    ): List<ItemStack> {
        val output = mutableListOf<ItemStack>()
        for (pool in pools) {
            pool.generate(random, luck, explosionRadius) { stack ->
                var decorated = stack
                for (function in functions) decorated = function.apply(decorated, random, explosionRadius)
                // A stack an explosion swallowed whole is not a drop at all.
                if (decorated.count > 0) output += decorated
            }
        }
        return output
    }
}
