package org.basaltmc.world.crafting

import org.basaltmc.common.Identifier
import org.basaltmc.world.loot.ItemStack
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class RecipeTest {
    private val plank = Identifier.vanilla("oak_planks")
    private val stick = Identifier.vanilla("stick")
    private val diamond = Identifier.vanilla("diamond")

    private fun grid(
        width: Int,
        height: Int,
        vararg cells: Pair<Int, Identifier>,
    ): CraftingInput {
        val items = arrayOfNulls<ItemStack?>(width * height)
        for ((index, id) in cells) items[index] = ItemStack(id, 1)
        return CraftingInput.of(width, height, items.toList())
    }

    @Test
    fun stickRecipeMatchesTwoStackedPlanksAnywhereInA3x3Grid() {
        val recipe =
            Recipe.Shaped(
                group = "",
                result = ItemStack(stick, 4),
                width = 1,
                height = 2,
                ingredients = listOf(Ingredient(setOf(plank)), Ingredient(setOf(plank))),
            )
        // Planks placed in the bottom-right column of a 3x3 grid: trimming must still find the 1x2 shape.
        val input = grid(3, 3, 7 to plank, 4 to plank)
        assertTrue(recipe.matches(input))
    }

    @Test
    fun shapedRecipeRejectsWrongIngredientCount() {
        val recipe =
            Recipe.Shaped(
                group = "",
                result = ItemStack(stick, 4),
                width = 1,
                height = 2,
                ingredients = listOf(Ingredient(setOf(plank)), Ingredient(setOf(plank))),
            )
        val input = grid(1, 1, 0 to plank)
        assertEquals(false, recipe.matches(input))
    }

    @Test
    fun asymmetricShapedRecipeMatchesBothMirroredOrientations() {
        val recipe =
            Recipe.Shaped(
                group = "",
                result = ItemStack(stick, 1),
                width = 2,
                height = 1,
                ingredients = listOf(Ingredient(setOf(plank)), Ingredient(setOf(diamond))),
            )
        assertTrue(recipe.matches(grid(2, 1, 0 to plank, 1 to diamond)))
        assertTrue(recipe.matches(grid(2, 1, 0 to diamond, 1 to plank)))
    }

    @Test
    fun symmetricShapedRecipeDoesNotDoubleMatch() {
        val recipe =
            Recipe.Shaped(
                group = "",
                result = ItemStack(stick, 1),
                width = 2,
                height = 1,
                ingredients = listOf(Ingredient(setOf(plank)), Ingredient(setOf(plank))),
            )
        assertTrue(recipe.matches(grid(2, 1, 0 to plank, 1 to plank)))
    }

    @Test
    fun shapelessRecipeMatchesAnyArrangement() {
        val recipe =
            Recipe.Shapeless(
                group = "",
                result = ItemStack(diamond, 1),
                ingredients = listOf(Ingredient(setOf(plank)), Ingredient(setOf(stick))),
            )
        assertTrue(recipe.matches(grid(3, 3, 0 to stick, 8 to plank)))
        assertTrue(recipe.matches(grid(3, 3, 0 to plank, 8 to stick)))
    }

    @Test
    fun shapelessRecipeRejectsAnUnsatisfiableAssignment() {
        val recipe =
            Recipe.Shapeless(
                group = "",
                result = ItemStack(diamond, 1),
                ingredients = listOf(Ingredient(setOf(plank)), Ingredient(setOf(plank))),
            )
        assertEquals(false, recipe.matches(grid(3, 3, 0 to plank, 8 to stick)))
    }

    @Test
    fun craftingInputTrimsToTheOccupiedBoundingBox() {
        val input = grid(3, 3, 4 to plank)
        assertEquals(1, input.width)
        assertEquals(1, input.height)
        assertEquals(plank, input.item(0, 0)?.item)
    }

    @Test
    fun parsesARealShapedRecipeJsonWithATagIngredient() {
        val json =
            """
            {
                "type": "minecraft:crafting_shaped",
                "group": "planks",
                "key": { "#": "#minecraft:planks" },
                "pattern": ["#", "#"],
                "result": { "id": "minecraft:stick", "count": 4 }
            }
            """.trimIndent()
        val tags = mapOf(Identifier.vanilla("planks") to setOf(plank))
        val recipe = RecipeJson.parse(json) { tag -> tags[tag] ?: emptySet() } as Recipe.Shaped

        assertEquals("planks", recipe.group)
        assertEquals(ItemStack(stick, 4), recipe.result)
        assertTrue(recipe.matches(grid(1, 2, 0 to plank, 1 to plank)))
    }

    @Test
    fun parsesARealShapelessRecipeJson() {
        val json =
            """
            {
                "type": "minecraft:crafting_shapeless",
                "ingredients": ["minecraft:oak_planks", "minecraft:stick"],
                "result": { "id": "minecraft:diamond" }
            }
            """.trimIndent()
        val recipe = RecipeJson.parse(json) { emptySet() } as Recipe.Shapeless

        assertEquals(1, recipe.result.count)
        assertTrue(recipe.matches(grid(3, 3, 0 to stick, 8 to plank)))
    }

    @Test
    fun unsupportedRecipeTypesThrowWithTheTypeName() {
        val json = """{"type": "minecraft:smelting", "result": {"id": "minecraft:stick"}}"""
        val exception = assertFailsWith<RecipeParseException> { RecipeJson.parse(json) { emptySet() } }
        assertTrue(exception.message!!.contains("minecraft:smelting"))
    }

    @Test
    fun ingredientTestOptionalRejectsAFilledSlotWhereThePatternExpectsEmpty() {
        assertEquals(false, Ingredient.testOptional(null, ItemStack(plank, 1)))
    }
}
