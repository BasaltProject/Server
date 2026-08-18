package org.basaltmc.server.runtime.world

import org.basaltmc.wire.nbt.NbtCompound

/**
 * The shulker box: a chest whose contents go with it when it is picked up.
 *
 * parity: `ShulkerBoxBlock.getDrops` puts the block entity's slots into the item's
 * `minecraft:container` component, and `BlockItem.updateCustomBlockEntityTag` writes them back into
 * the block when it is placed. That round trip is the whole point of the block, so it is here rather
 * than in the loot table: the table's `set_contents` is what does it in vanilla.
 */
public object Shulkers {
    /** parity: `ShulkerBoxBlockEntity`, which is a chest's twenty-seven. */
    public const val SIZE: Int = 27

    /** parity: `DataComponents.CONTAINER`, which is where the contents ride. */
    public const val CONTAINER: String = "minecraft:container"

    private val COLOURS =
        listOf(
            "white",
            "orange",
            "magenta",
            "light_blue",
            "yellow",
            "lime",
            "pink",
            "gray",
            "light_gray",
            "cyan",
            "purple",
            "blue",
            "brown",
            "green",
            "red",
            "black",
        )

    /** Every shulker box there is: the plain one and one per dye. */
    public val BLOCKS: List<String> = listOf("shulker_box") + COLOURS.map { "${it}_shulker_box" }

    public fun isShulkerBox(path: String?): Boolean = path != null && path in BLOCKS

    /** The contents of the box at these coordinates, in the form an item component takes. */
    public fun contentsOf(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): NbtCompound? {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return null
        val slots = BlockInventory.read(entity.data, SIZE)
        if (slots.all { it == null }) return null
        val carried = NbtCompound()
        BlockInventory.write(carried, slots)
        return carried
    }

    /** Puts contents carried by an item back into the box that was just placed. */
    public fun restore(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        carried: NbtCompound,
    ) {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return
        BlockInventory.write(entity.data, BlockInventory.read(carried, SIZE))
        chunk.markDirty()
    }
}
