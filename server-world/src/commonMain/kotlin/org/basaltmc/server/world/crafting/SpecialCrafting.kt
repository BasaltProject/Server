package org.basaltmc.server.world.crafting

import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType

/**
 * What each special recipe accepts and what it makes of it.
 *
 * parity: one class each in vanilla (`DyeRecipe`, `BookCloningRecipe`, `RepairItemRecipe`...), all
 * of the same shape: one slot holds the thing being changed, the rest hold what changes it, and the
 * result is that thing with a component put on it.
 */
public object SpecialCrafting {
    /** How much damage an item of this kind takes before it breaks, filled in by the caller. */
    public var maxDamageOf: (Identifier) -> Int = { 0 }

    /** The colour each dye item stands for, filled in by the caller from the item's own component. */
    public var dyeColourOf: (Identifier) -> Int? = { null }

    public fun matches(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): Boolean = assemble(recipe, input) != null

    /** parity: each recipe's own `assemble`, answering null where vanilla answers an empty stack. */
    public fun assemble(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? =
        when (recipe.kind) {
            Recipe.Special.Kind.REPAIR_ITEM -> repair(input)
            Recipe.Special.Kind.IMBUE -> imbue(recipe, input)
            Recipe.Special.Kind.DECORATED_POT -> decoratedPot(recipe, input)
            Recipe.Special.Kind.MAP_EXTENDING -> mapExtending(recipe, input)
            Recipe.Special.Kind.DYE -> dye(recipe, input)
            Recipe.Special.Kind.FIREWORK_STAR_FADE -> dye(recipe, input)
            Recipe.Special.Kind.BOOK_CLONING -> bookCloning(recipe, input)
            Recipe.Special.Kind.BANNER_DUPLICATE -> bannerDuplicate(recipe, input)
            Recipe.Special.Kind.SHIELD_DECORATION -> shieldDecoration(recipe, input)
            Recipe.Special.Kind.FIREWORK_STAR -> fireworkStar(recipe, input)
            Recipe.Special.Kind.FIREWORK_ROCKET -> fireworkRocket(recipe, input)
        }

    /** parity: `RepairItemRecipe`, the only one whose two inputs are the same item. */
    private fun repair(input: CraftingInput): ItemStack? {
        if (input.ingredientCount != 2) return null
        val stacks = occupied(input)
        val first = stacks.getOrNull(0) ?: return null
        val second = stacks.getOrNull(1) ?: return null
        if (first.count != 1 || second.count != 1) return null
        return SpecialRecipes.repair(first, second, maxDamageOf)
    }

    /** parity: `ImbueRecipe`: the thing being imbued in the middle, the eight around it the same. */
    private fun imbue(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.width != 3 || input.height != 3 || input.ingredientCount != 9) return null
        val centre = input.item(1, 1) ?: return null
        if (recipe.target?.test(centre) != true) return null
        for (y in 0 until 3) {
            for (x in 0 until 3) {
                if (x == 1 && y == 1) continue
                if (recipe.material?.test(input.item(x, y)) != true) return null
            }
        }
        // parity: the arrows come out carrying the potion that was in the middle.
        return SpecialRecipes.withComponentsOf(recipe.result.item, recipe.result.count, centre)
    }

    /** parity: `DecoratedPotRecipe`, four sherds in a diamond and nothing in the corners. */
    private fun decoratedPot(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.width != 3 || input.height != 3 || input.ingredientCount != 4) return null
        val sides = listOf(input.item(1, 0), input.item(0, 1), input.item(2, 1), input.item(1, 2))
        if (sides.any { recipe.material?.test(it) != true }) return null
        return recipe.result.copy()
    }

    /** parity: `MapExtendingRecipe`: a map in the middle and eight of the material around it. */
    private fun mapExtending(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.width != 3 || input.height != 3 || input.ingredientCount != 9) return null
        val map = input.item(1, 1) ?: return null
        if (recipe.target?.test(map) != true) return null
        for (y in 0 until 3) {
            for (x in 0 until 3) {
                if (x == 1 && y == 1) continue
                if (recipe.material?.test(input.item(x, y)) != true) return null
            }
        }
        return SpecialRecipes.withComponentsOf(recipe.result.item, 1, map)
    }

    /**
     * parity: `DyeRecipe` and `FireworkStarFadeRecipe`: exactly one thing being coloured and at
     * least one dye, and the colours are averaged onto whatever it already had.
     */
    private fun dye(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.ingredientCount < 2) return null
        var target: ItemStack? = null
        val dyes = mutableListOf<Int>()
        for (stack in occupied(input)) {
            if (recipe.target?.test(stack) == true && target == null) {
                target = stack
                continue
            }
            if (recipe.material?.test(stack) != true) return null
            dyes += dyeColourOf(stack.item) ?: return null
        }
        val coloured = target ?: return null
        if (dyes.isEmpty()) return null
        val current = (coloured.components[SpecialRecipes.DYED_COLOR] as? NbtInt)?.value
        return SpecialRecipes.withComponentsOf(
            recipe.result.item,
            recipe.result.count,
            coloured,
            SpecialRecipes.DYED_COLOR to NbtInt(SpecialRecipes.mixDyes(current, dyes)),
        )
    }

    /** parity: `BookCloningRecipe`: one written book and as many blank ones as you like. */
    private fun bookCloning(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.ingredientCount < 2) return null
        var source: ItemStack? = null
        var copies = 0
        for (stack in occupied(input)) {
            if (recipe.target?.test(stack) == true && source == null) {
                source = stack
                continue
            }
            if (recipe.material?.test(stack) != true) return null
            copies += stack.count
        }
        val written = source ?: return null
        if (copies < 1) return null
        // parity: the copies carry the book, which is the whole point of copying it.
        return SpecialRecipes.withComponentsOf(recipe.result.item, copies, written)
    }

    /** parity: `BannerDuplicateRecipe`: one patterned banner and one blank of the same colour. */
    private fun bannerDuplicate(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.ingredientCount != 2) return null
        val stacks = occupied(input)
        if (stacks.any { recipe.target?.test(it) != true }) return null
        val patterned = stacks.firstOrNull { patternsOf(it).isNotEmpty() } ?: return null
        val blank = stacks.firstOrNull { patternsOf(it).isEmpty() } ?: return null
        if (patterned === blank) return null
        return SpecialRecipes.withComponentsOf(recipe.result.item, 1, patterned)
    }

    /** parity: `ShieldDecorationRecipe`: a plain shield and a banner, and the shield takes it on. */
    private fun shieldDecoration(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.ingredientCount != 2) return null
        val stacks = occupied(input)
        val banner = stacks.firstOrNull { recipe.material?.test(it) == true } ?: return null
        val shield = stacks.firstOrNull { it !== banner && recipe.target?.test(it) == true } ?: return null
        if (patternsOf(shield).isNotEmpty()) return null
        val patterns = banner.components[SpecialRecipes.BANNER_PATTERNS]
        return SpecialRecipes.withComponentsOf(recipe.result.item, 1, shield).also {
            if (patterns != null) it.components[SpecialRecipes.BANNER_PATTERNS] = patterns
        }
    }

    /**
     * parity: `FireworkStarRecipe`: one gunpowder, at least one dye, and any of the shape and effect
     * modifiers, each at most once.
     */
    private fun fireworkStar(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.ingredientCount < 2) return null
        var gunpowder = false
        val dyes = mutableListOf<Int>()
        val extras = mutableSetOf<String>()
        for (stack in occupied(input)) {
            val modifier = recipe.modifiers.firstOrNull { it.second.test(stack) }
            when {
                recipe.target?.test(stack) == true && !gunpowder -> gunpowder = true
                recipe.material?.test(stack) == true -> dyes += dyeColourOf(stack.item) ?: return null
                // parity: one shape, one trail and one twinkle at most, and a second of any of them
                // makes the whole thing not a recipe.
                modifier != null -> if (!extras.add(modifier.first)) return null
                else -> return null
            }
        }
        if (!gunpowder || dyes.isEmpty()) return null
        if (extras.count { it in SHAPES } > 1) return null
        val components = NbtCompound()
        components[FIREWORK_COLOURS] = NbtList(NbtType.INT, dyes.map { NbtInt(it) })
        extras.firstOrNull { it in SHAPES }?.let { components[FIREWORK_SHAPE] = NbtString(it) }
        if ("trail" in extras) components[FIREWORK_TRAIL] = NbtInt(1)
        if ("twinkle" in extras) components[FIREWORK_TWINKLE] = NbtInt(1)
        return ItemStack(recipe.result.item, recipe.result.count, components)
    }

    /** parity: `FireworkExplosion.Shape`, the four a recipe can ask for on top of the small ball. */
    private val SHAPES = setOf("burst", "creeper", "large_ball", "star")

    private const val FIREWORK_SHAPE = "minecraft:firework_explosion_shape"
    private const val FIREWORK_TRAIL = "minecraft:firework_explosion_trail"
    private const val FIREWORK_TWINKLE = "minecraft:firework_explosion_twinkle"

    /**
     * parity: `FireworkRocketRecipe`: one paper, one to three gunpowder, and any number of stars.
     * The flight time is the gunpowder count, and the stars ride along in the rocket.
     */
    private fun fireworkRocket(
        recipe: Recipe.Special,
        input: CraftingInput,
    ): ItemStack? {
        if (input.ingredientCount < 2) return null
        var shell = false
        var fuel = 0
        val stars = mutableListOf<ItemStack>()
        for (stack in occupied(input)) {
            when {
                recipe.target?.test(stack) == true && !shell -> shell = true
                recipe.material?.test(stack) == true -> if (++fuel > MAX_FLIGHT) return null
                recipe.extra?.test(stack) == true -> stars += stack
                else -> return null
            }
        }
        if (!shell || fuel < 1) return null
        val components = NbtCompound()
        components[FLIGHT_DURATION] = NbtInt(fuel)
        return ItemStack(recipe.result.item, recipe.result.count, components)
    }

    /** parity: `Fireworks.flightDuration`, which is one to three and nothing else. */
    private const val MAX_FLIGHT = 3

    private const val FLIGHT_DURATION = "minecraft:fireworks_flight_duration"

    /** parity: `FireworkExplosion.colors`, the list of colours a star carries. */
    private const val FIREWORK_COLOURS = "minecraft:firework_explosion_colors"

    private fun patternsOf(stack: ItemStack): List<Any> {
        val patterns = stack.components[SpecialRecipes.BANNER_PATTERNS] as? NbtList ?: return emptyList()
        return List(patterns.size) { patterns[it] }
    }

    private fun occupied(input: CraftingInput): List<ItemStack> =
        buildList {
            for (y in 0 until input.height) {
                for (x in 0 until input.width) input.item(x, y)?.let { add(it) }
            }
        }
}
