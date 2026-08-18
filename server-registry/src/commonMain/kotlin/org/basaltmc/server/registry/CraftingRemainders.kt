package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * What an item leaves behind in the grid when it is crafted with.
 *
 * parity: `Item.getCraftingRemainder`. Five items have one, and they are the reason a cake gives the
 * buckets back instead of eating them.
 */
public object CraftingRemainders {
    private val byItem: Map<Identifier, Identifier> =
        ALL_CRAFTING_REMAINDERS.associate { (item, left) ->
            Identifier.vanilla(item) to Identifier.vanilla(left)
        }

    public fun of(item: Identifier): Identifier? = byItem[item]
}
