package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** How high one item stacks, and how long it waits between uses. */
public class ItemStackingEntry(
    public val maxStackSize: Int,
    public val cooldownTicks: Int,
)

/**
 * The two item components that decide how a stack behaves outside a recipe.
 *
 * parity: `minecraft:max_stack_size` and `minecraft:use_cooldown`, both written in the item's own
 * component file: an ender pearl waits a second because its file says so, not because any class
 * mentions ender pearls.
 */
public object ItemStacking {
    /** parity: `Item.Properties`' own defaults, which most items never change. */
    public const val DEFAULT_STACK_SIZE: Int = 64

    private val byItem: Map<String, ItemStackingEntry> =
        ALL_ITEM_STACKING
            .mapNotNull { row ->
                val parts = row.split('|')
                if (parts.size != 3) return@mapNotNull null
                val stack = parts[1].toIntOrNull() ?: return@mapNotNull null
                val cooldown = parts[2].toIntOrNull() ?: return@mapNotNull null
                parts[0] to ItemStackingEntry(stack, cooldown)
            }.toMap()

    public fun of(item: Identifier): ItemStackingEntry? = byItem[item.path]

    public fun maxStackSize(item: Identifier): Int = byItem[item.path]?.maxStackSize ?: DEFAULT_STACK_SIZE

    /** How long the item is greyed out after a use, or zero when it is never greyed out. */
    public fun cooldownTicks(item: Identifier): Int = byItem[item.path]?.cooldownTicks ?: 0
}
