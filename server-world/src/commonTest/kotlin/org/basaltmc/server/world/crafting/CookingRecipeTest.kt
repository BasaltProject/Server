package org.basaltmc.server.world.crafting

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `AbstractCookingRecipe` and the four types that extend it. */
class CookingRecipeTest {
    private fun parse(json: String): Recipe = RecipeJson.parse(json) { emptySet() }

    @Test
    fun aSmeltingRecipeTakesTwoHundredTicksWhenItSaysNothing() {
        val recipe =
            parse(
                """
                {"type":"minecraft:smelting","experience":0.7,"group":"iron_ingot",
                 "ingredient":"minecraft:iron_ore","result":{"id":"minecraft:iron_ingot"}}
                """.trimIndent(),
            ) as Recipe.Cooking

        assertEquals(Recipe.Cooker.SMELTING, recipe.cooker)
        assertEquals(200, recipe.cookingTime, "the default a furnace recipe gets")
        assertEquals(0.7f, recipe.experience)
        assertEquals(Identifier.vanilla("iron_ingot"), recipe.result.item)
        assertTrue(
            recipe.accepts(
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.vanilla("iron_ore"), 1),
            ),
        )
        assertFalse(
            recipe.accepts(
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.vanilla("gold_ore"), 1),
            ),
        )
    }

    @Test
    fun theOtherThreeCookersHaveTheirOwnDefaults() {
        for (
        (type, cooker, time) in
        listOf(
            Triple("blasting", Recipe.Cooker.BLASTING, 100),
            Triple("smoking", Recipe.Cooker.SMOKING, 100),
            Triple("campfire_cooking", Recipe.Cooker.CAMPFIRE, 100),
        )
        ) {
            val recipe =
                parse(
                    """
                    {"type":"minecraft:$type","ingredient":"minecraft:beef",
                     "result":{"id":"minecraft:cooked_beef"}}
                    """.trimIndent(),
                ) as Recipe.Cooking
            assertEquals(cooker, recipe.cooker)
            assertEquals(time, recipe.cookingTime, type)
        }
    }

    /** A file that names its own time keeps it: campfires take six times as long as a furnace. */
    @Test
    fun aStatedCookingTimeWins() {
        val recipe =
            parse(
                """
                {"type":"minecraft:campfire_cooking","cookingtime":600,"experience":0.35,
                 "ingredient":"minecraft:beef","result":{"id":"minecraft:cooked_beef"}}
                """.trimIndent(),
            ) as Recipe.Cooking

        assertEquals(600, recipe.cookingTime)
    }
}
