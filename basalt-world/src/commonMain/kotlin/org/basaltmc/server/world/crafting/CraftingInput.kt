package org.basaltmc.server.world.crafting

import org.basaltmc.server.world.loot.ItemStack

/**
 * A crafting grid trimmed to the bounding box of its non-empty slots. Mirrors vanilla's
 * `CraftingInput.ofPositioned`: recipes never see the raw grid position or its untrimmed size, only
 * the occupied shape, which is what makes a 2x2 recipe match anywhere in a 3x3 grid.
 */
public class CraftingInput private constructor(
    public val width: Int,
    public val height: Int,
    private val items: List<ItemStack?>,
) {
    public val ingredientCount: Int = items.count { it != null }

    public fun item(
        x: Int,
        y: Int,
    ): ItemStack? = items[x + y * width]

    public companion object {
        public val EMPTY: CraftingInput = CraftingInput(0, 0, emptyList())

        public fun of(
            width: Int,
            height: Int,
            items: List<ItemStack?>,
        ): CraftingInput {
            if (width == 0 || height == 0) return EMPTY

            var left = width - 1
            var right = 0
            var top = height - 1
            var bottom = 0
            for (y in 0 until height) {
                var rowEmpty = true
                for (x in 0 until width) {
                    if (items[x + y * width] != null) {
                        left = minOf(left, x)
                        right = maxOf(right, x)
                        rowEmpty = false
                    }
                }
                if (!rowEmpty) {
                    top = minOf(top, y)
                    bottom = maxOf(bottom, y)
                }
            }

            val newWidth = right - left + 1
            val newHeight = bottom - top + 1
            if (newWidth <= 0 || newHeight <= 0) return EMPTY
            if (newWidth == width && newHeight == height) return CraftingInput(width, height, items)

            val trimmed = ArrayList<ItemStack?>(newWidth * newHeight)
            for (y in 0 until newHeight) {
                for (x in 0 until newWidth) {
                    trimmed += items[(x + left) + (y + top) * width]
                }
            }
            return CraftingInput(newWidth, newHeight, trimmed)
        }
    }
}
