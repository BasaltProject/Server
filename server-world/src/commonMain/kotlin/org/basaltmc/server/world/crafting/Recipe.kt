package org.basaltmc.server.world.crafting

import org.basaltmc.server.world.loot.ItemStack

/**
 * A crafting-table recipe. Only the two ingredient-based crafting-table types (`minecraft:
 * crafting_shaped`, `minecraft:crafting_shapeless`) are modeled; furnace/stonecutter/smithing and
 * the special (dye, firework, banner duplicate...) recipe types are a documented gap, see
 * [RecipeJson].
 */
public sealed class Recipe {
    public abstract val group: String
    public abstract val result: ItemStack

    public abstract fun matches(input: CraftingInput): Boolean

    /** Mirrors `ShapedRecipe`/`ShapedRecipePattern`: a fixed-shape grid of ingredients, tried both as-is and mirrored. */
    public data class Shaped(
        override val group: String,
        override val result: ItemStack,
        val width: Int,
        val height: Int,
        val ingredients: List<Ingredient?>,
    ) : Recipe() {
        private val ingredientCount: Int = ingredients.count { it != null }
        private val symmetrical: Boolean = isSymmetrical()

        override fun matches(input: CraftingInput): Boolean {
            if (input.ingredientCount != ingredientCount) return false
            if (input.width != width || input.height != height) return false
            if (!symmetrical && matchesFlip(input, xFlip = true)) return true
            return matchesFlip(input, xFlip = false)
        }

        private fun matchesFlip(
            input: CraftingInput,
            xFlip: Boolean,
        ): Boolean {
            for (y in 0 until height) {
                for (x in 0 until width) {
                    val expected = if (xFlip) ingredients[width - x - 1 + y * width] else ingredients[x + y * width]
                    if (!Ingredient.testOptional(expected, input.item(x, y))) return false
                }
            }
            return true
        }

        // Mirrors `Util.isSymmetrical`: a width-1 pattern is trivially symmetrical, otherwise every
        // slot must equal its horizontal mirror.
        private fun isSymmetrical(): Boolean {
            if (width == 1) return true
            val centerX = width / 2
            for (y in 0 until height) {
                for (leftX in 0 until centerX) {
                    val rightX = width - 1 - leftX
                    if (ingredients[leftX + y * width] != ingredients[rightX + y * width]) return false
                }
            }
            return true
        }
    }

    /** Mirrors `ShapelessRecipe`: any arrangement works as long as every item can be matched to a distinct ingredient. */
    public data class Shapeless(
        override val group: String,
        override val result: ItemStack,
        val ingredients: List<Ingredient>,
    ) : Recipe() {
        override fun matches(input: CraftingInput): Boolean {
            if (input.ingredientCount != ingredients.size) return false
            val items = mutableListOf<ItemStack>()
            for (y in 0 until input.height) {
                for (x in 0 until input.width) {
                    input.item(x, y)?.let { items += it }
                }
            }
            return hasPerfectMatching(items, ingredients)
        }
    }
}

/**
 * Whether every item can be assigned to a distinct ingredient it satisfies: a bipartite perfect
 * matching (Kuhn's algorithm with augmenting paths). Vanilla answers the same question via
 * `StackedContents.canCraft`; this isn't traced from its bytecode like the loot table RNG code is,
 * but craftability is a pure yes/no decision with no RNG involved, so any correct matching algorithm
 * reaches the same accept/reject result.
 */
private fun hasPerfectMatching(
    items: List<ItemStack>,
    ingredients: List<Ingredient>,
): Boolean {
    val assignedItem = IntArray(ingredients.size) { -1 }

    fun tryAssign(
        itemIndex: Int,
        visited: BooleanArray,
    ): Boolean {
        for (i in ingredients.indices) {
            if (visited[i] || !ingredients[i].test(items[itemIndex])) continue
            visited[i] = true
            if (assignedItem[i] == -1 || tryAssign(assignedItem[i], visited)) {
                assignedItem[i] = itemIndex
                return true
            }
        }
        return false
    }

    for (itemIndex in items.indices) {
        if (!tryAssign(itemIndex, BooleanArray(ingredients.size))) return false
    }
    return true
}
