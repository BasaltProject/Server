package org.basaltmc.server.world.crafting

import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * A recipe: the two ingredient-based crafting-table types, the four cooking ones and the
 * stonecutter's. Smithing and the special (dye, firework, banner duplicate...) types are a
 * documented gap, see [RecipeJson].
 */
public sealed class Recipe {
    public abstract val group: String
    public abstract val result: ItemStack

    public abstract fun matches(input: CraftingInput): Boolean

    /**
     * What this recipe makes of [input].
     *
     * parity: `Recipe.assemble`. Nearly every recipe answers with its own fixed result; the ones
     * that do not are exactly the specials, whose whole point is what they carry over.
     */
    public open fun assemble(input: CraftingInput): ItemStack = result

    /**
     * What each grid cell is left holding once this is crafted.
     *
     * parity: `Recipe.getRemainingItems`, which is what puts the empty bucket back where the milk
     * was. Cells that answer null are used up, which is nearly all of them.
     */
    public open fun remainingItems(input: CraftingInput): List<ItemStack?> =
        buildList {
            for (y in 0 until input.height) {
                for (x in 0 until input.width) {
                    val stack = input.item(x, y)
                    add(stack?.let { remainderOf(it.item)?.let { left -> ItemStack(left, 1) } })
                }
            }
        }

    public companion object {
        /** What an item leaves behind, filled in by the caller from the registry. */
        public var remainderOf: (Identifier) -> Identifier? = { null }
    }

    /**
     * What each of the four blocks that cook accepts, mirroring `AbstractCookingRecipe`'s subtypes.
     * A furnace cooks [SMELTING], a blast furnace [BLASTING], a smoker [SMOKING] and a campfire
     * [CAMPFIRE].
     */
    public enum class Cooker {
        SMELTING,
        BLASTING,
        SMOKING,
        CAMPFIRE,
    }

    /**
     * parity: `AbstractCookingRecipe`: one ingredient in, one stack out, over [cookingTime] ticks
     * and worth [experience] when it is taken.
     */
    public data class Cooking(
        override val group: String,
        override val result: ItemStack,
        val cooker: Cooker,
        val ingredient: Ingredient,
        val experience: Float,
        val cookingTime: Int,
    ) : Recipe() {
        /** A cooking recipe holds one item, so the grid form of the question is one non-empty slot. */
        override fun matches(input: CraftingInput): Boolean =
            input.ingredientCount == 1 && ingredient.test(input.item(0, 0))

        public fun accepts(stack: ItemStack?): Boolean = ingredient.test(stack)
    }

    /**
     * parity: `StonecutterRecipe`: one ingredient in, one stack out, and no cost beyond the item.
     *
     * Which of several results you get is chosen by hand rather than by the recipe, so the list is
     * kept in the order the recipes were read: the client filters that same list by what is in the
     * slot, and the button it sends back is an index into what is left.
     */
    public data class Stonecutting(
        override val group: String,
        override val result: ItemStack,
        val ingredient: Ingredient,
    ) : Recipe() {
        /** A stonecutter holds one item, so the grid form of the question is one non-empty slot. */
        override fun matches(input: CraftingInput): Boolean =
            input.ingredientCount == 1 && ingredient.test(input.item(0, 0))

        public fun accepts(stack: ItemStack?): Boolean = ingredient.test(stack)
    }

    /**
     * parity: `SmithingTransformRecipe` and `SmithingTrimRecipe`: three slots at a smithing table,
     * and either a new item or the base with a trim on it.
     */
    public data class Smithing(
        override val group: String,
        override val result: ItemStack,
        val template: Ingredient,
        val base: Ingredient,
        val addition: Ingredient,
        /** The trim this applies, or null when the recipe makes a different item instead. */
        val trimPattern: Identifier? = null,
    ) : Recipe() {
        /** A smithing recipe is never a crafting-table recipe, whatever is in the grid. */
        override fun matches(input: CraftingInput): Boolean = false

        public fun accepts(
            template: ItemStack?,
            base: ItemStack?,
            addition: ItemStack?,
        ): Boolean = this.template.test(template) && this.base.test(base) && this.addition.test(addition)

        /**
         * parity: `SmithingTransformRecipe.assemble` and `SmithingTrimRecipe.applyTrim`: a transform
         * makes its own item carrying what the base had, and a trim is the base itself with the
         * pattern and the material's colour written onto it.
         */
        public fun assemble(
            base: ItemStack,
            addition: ItemStack,
        ): ItemStack? {
            val pattern = trimPattern ?: return SpecialRecipes.withComponentsOf(result.item, 1, base)
            val material = SpecialRecipes.trimMaterialOf(addition.item) ?: return null
            val trim = NbtCompound()
            trim.putString("pattern", pattern.toString())
            trim.putString("material", material.toString())
            val trimmed = SpecialRecipes.withComponentsOf(base.item, 1, base)
            // parity: applying the trim it already has is not a recipe at all.
            if (trimmed.components[SpecialRecipes.TRIM] == trim) return null
            trimmed.components[SpecialRecipes.TRIM] = trim
            return trimmed
        }
    }

    /**
     * parity: the `crafting_special_*` family, `crafting_dye` and `crafting_imbue`: recipes whose
     * result depends on what went in rather than being a fixed stack.
     *
     * The rule lives in [kind]; the ingredients come from the recipe file, which is where 26.2 puts
     * them. [assemble] is what makes them worth having: without it the dye, the copy and the repair
     * would all produce a plain item with nothing carried over.
     */
    public data class Special(
        override val group: String,
        override val result: ItemStack,
        val kind: Kind,
        /** The thing being changed: the armour dyed, the book copied, the shield decorated. */
        val target: Ingredient? = null,
        /** What it is changed with: the dyes, the paper, the banner, the blank books. */
        val material: Ingredient? = null,
        /** A third ingredient, for the one recipe that has one: the rocket's stars. */
        val extra: Ingredient? = null,
        /**
         * The named extras a firework star takes, each at most once: the four shapes, the trail and
         * the twinkle. parity: `FireworkStarRecipe`'s own `shapes`, `trail` and `twinkle`.
         */
        val modifiers: List<Pair<String, Ingredient>> = emptyList(),
    ) : Recipe() {
        public enum class Kind {
            DYE,
            IMBUE,
            BOOK_CLONING,
            BANNER_DUPLICATE,
            FIREWORK_ROCKET,
            FIREWORK_STAR,
            FIREWORK_STAR_FADE,
            MAP_EXTENDING,
            SHIELD_DECORATION,
            REPAIR_ITEM,
            DECORATED_POT,
        }

        override fun matches(input: CraftingInput): Boolean = SpecialCrafting.matches(this, input)

        override fun assemble(input: CraftingInput): ItemStack = SpecialCrafting.assemble(this, input) ?: result

        /**
         * parity: `BookCloningRecipe.getRemainingItems`, which puts the book being copied back: it
         * is the one recipe whose source survives being crafted with.
         */
        override fun remainingItems(input: CraftingInput): List<ItemStack?> {
            val kept = super.remainingItems(input).toMutableList()
            if (kind != Kind.BOOK_CLONING) return kept
            var slot = 0
            for (y in 0 until input.height) {
                for (x in 0 until input.width) {
                    val stack = input.item(x, y)
                    if (stack != null && target?.test(stack) == true) kept[slot] = stack.copy(count = 1)
                    slot++
                }
            }
            return kept
        }
    }

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
