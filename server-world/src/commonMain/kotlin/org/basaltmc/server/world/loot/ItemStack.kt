package org.basaltmc.server.world.loot

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * A stack of an item: what a loot table produces and what a crafting grid holds.
 *
 * [components] is what the item carries beyond its own defaults, in the same form the rest of the
 * server stores it. Most of what comes out of a loot table has none; a crafting recipe that dyes,
 * repairs, copies a book or decorates a shield is defined entirely by what it does to them, so the
 * grid has to carry them or those recipes cannot exist at all.
 */
public data class ItemStack(
    val item: Identifier,
    var count: Int,
    val components: NbtCompound = NbtCompound(),
)
