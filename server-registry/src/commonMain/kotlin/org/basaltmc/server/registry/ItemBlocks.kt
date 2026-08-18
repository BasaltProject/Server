package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * Which block an item puts down.
 *
 * parity: `BlockItem.getBlock`. Nearly every item shares its name with its block, and the dozen
 * that do not are the ones that made this necessary: redstone dust is `redstone_wire`, string is
 * `tripwire`, and every seed is called something else from the crop it grows.
 */
public object ItemBlocks {
    private val byItem: Map<Identifier, Identifier> =
        ALL_ITEM_BLOCKS.associate { (item, block) -> Identifier.vanilla(item) to Identifier.vanilla(block) }

    private val byBlock: Map<Identifier, Identifier> = byItem.entries.associate { (item, block) -> block to item }

    /** The block [item] places, which is [item] itself unless the table says otherwise. */
    public fun of(item: Identifier): Identifier = byItem[item] ?: item

    /**
     * The item [block] is picked up as, which is [block] itself unless the table says otherwise.
     *
     * parity: `Block.asItem`. Without it, middle-clicking dust, a crop or a cocoa pod gave nothing,
     * because no item is called `redstone_wire`.
     */
    public fun itemOf(block: Identifier): Identifier = byBlock[block] ?: block
}
