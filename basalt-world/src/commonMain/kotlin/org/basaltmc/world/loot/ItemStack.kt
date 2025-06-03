package org.basaltmc.world.loot

import org.basaltmc.common.Identifier

/** A stack of an item: what a loot table actually produces. No components/NBT yet. */
public data class ItemStack(
    val item: Identifier,
    var count: Int,
)
