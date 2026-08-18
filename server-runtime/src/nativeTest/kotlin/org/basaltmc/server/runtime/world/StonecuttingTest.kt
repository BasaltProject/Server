package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.crafting.Ingredient
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.crafting.RecipeJson
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

/** parity: `StonecutterRecipe`, read from the operator's own jar like every other recipe. */
class StonecuttingTest {
    private val stone = Identifier.vanilla("stone")

    /** The shape a stonecutting file actually has: one ingredient, one result, no time and no cost. */
    @Test
    fun itReadsAStonecuttingRecipe() {
        val json =
            """
            {
              "type": "minecraft:stonecutting",
              "ingredient": "minecraft:stone",
              "result": { "count": 2, "id": "minecraft:stone_slab" }
            }
            """.trimIndent()

        val recipe = assertIs<Recipe.Stonecutting>(RecipeJson.parse(json) { emptySet() })

        assertEquals(Identifier.vanilla("stone_slab"), recipe.result.item)
        assertEquals(2, recipe.result.count)
        assertTrue(recipe.accepts(ItemStack(stone, 1)))
        assertTrue(!recipe.accepts(ItemStack(Identifier.vanilla("dirt"), 1)))
    }

    /**
     * The order of the options is the contract with the client: it filters the same list by what is
     * in the slot and answers with an index into what is left.
     */
    @Test
    fun theOptionsKeepTheirOrder() {
        val slab = recipe("stone_slab", 2)
        val stairs = recipe("stone_stairs", 1)
        val bricks = recipe("stone_bricks", 1)
        val all = listOf(slab, stairs, bricks)

        val forStone = all.filter { it.accepts(ItemStack(stone, 1)) }

        assertEquals(3, forStone.size)
        assertEquals(Identifier.vanilla("stone_stairs"), forStone[1].result.item, "the second button")
    }

    private fun recipe(
        result: String,
        count: Int,
    ) = Recipe.Stonecutting(
        group = "",
        result = ItemStack(Identifier.vanilla(result), count),
        ingredient = Ingredient(setOf(stone)),
    )
}
