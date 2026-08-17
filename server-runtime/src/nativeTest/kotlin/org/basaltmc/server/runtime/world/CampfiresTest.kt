package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.crafting.Ingredient
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `CampfireBlockEntity.cookTick` and `cooldownTick`. */
class CampfiresTest {
    private val raw = Identifier.vanilla("porkchop")
    private val cooked = Identifier.vanilla("cooked_porkchop")

    private val recipe =
        Recipe.Cooking(
            group = "",
            result = ItemStack(cooked, 1),
            cooker = Recipe.Cooker.CAMPFIRE,
            ingredient = Ingredient(setOf(raw)),
            experience = 0.35f,
            cookingTime = 600,
        )

    private fun recipeFor(stack: ItemStack): Recipe.Cooking? = recipe.takeIf { it.accepts(stack) }

    /** parity: `placeFood`, one piece into the first free slot at the recipe's own time. */
    @Test
    fun itTakesOnePieceAtATime() {
        val state = Campfires.State()

        assertTrue(Campfires.placeFood(state, ItemStack(raw, 8), ::recipeFor))

        assertEquals(1, state.items[0]?.count, "one piece, whatever the player was holding")
        assertEquals(600, state.total[0])
        assertNull(state.items[1])
    }

    /** parity: a campfire cooks what a campfire cooks, and refuses the rest. */
    @Test
    fun itRefusesWhatItCannotCook() {
        val state = Campfires.State()

        assertFalse(Campfires.placeFood(state, ItemStack(Identifier.vanilla("iron_ore"), 1), ::recipeFor))
        assertNull(state.items[0])
    }

    @Test
    fun aFullFireTakesNoMore() {
        val state = Campfires.State()
        repeat(4) { assertTrue(Campfires.placeFood(state, ItemStack(raw, 1), ::recipeFor)) }

        assertFalse(Campfires.placeFood(state, ItemStack(raw, 1), ::recipeFor), "four is all it holds")
    }

    /** parity: `cookTick`, which hands the result back rather than keeping it. */
    @Test
    fun whatIsDoneComesOff() {
        val state = Campfires.State()
        Campfires.placeFood(state, ItemStack(raw, 1), ::recipeFor)

        repeat(599) { assertTrue(Campfires.tick(state, lit = true, ::recipeFor).isEmpty()) }
        val done = Campfires.tick(state, lit = true, ::recipeFor)

        assertEquals(1, done.size)
        assertEquals(cooked, done[0].item)
        assertNull(state.items[0], "and the slot is free again")
        assertEquals(0, state.progress[0])
    }

    /** parity: `cooldownTick`, two at a time, which is what makes a fire going out cost time. */
    @Test
    fun anUnlitFireLosesProgressTwoAtATime() {
        val state = Campfires.State()
        Campfires.placeFood(state, ItemStack(raw, 1), ::recipeFor)
        repeat(10) { Campfires.tick(state, lit = true, ::recipeFor) }
        assertEquals(10, state.progress[0])

        assertTrue(Campfires.tick(state, lit = false, ::recipeFor).isEmpty())
        assertEquals(8, state.progress[0])

        repeat(10) { Campfires.tick(state, lit = false, ::recipeFor) }
        assertEquals(0, state.progress[0], "and it stops at nothing rather than going under")
        assertEquals(1, state.items[0]?.count, "the food stays on the fire")
    }

    @Test
    fun itSavesAndLoadsWhatIsOnIt() {
        val state = Campfires.State()
        Campfires.placeFood(state, ItemStack(raw, 1), ::recipeFor)
        repeat(5) { Campfires.tick(state, lit = true, ::recipeFor) }

        val data = NbtCompound()
        Campfires.write(state, data)
        val back = Campfires.read(data)

        assertEquals(raw, back.items[0]?.item)
        assertEquals(5, back.progress[0])
        assertEquals(600, back.total[0])
    }

    @Test
    fun onlyALitCampfireCooks() {
        val lit =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("campfire"),
                    mapOf("lit" to "true", "facing" to "north", "signal_fire" to "false", "waterlogged" to "false"),
                ),
            )
        val out =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("campfire"),
                    mapOf("lit" to "false", "facing" to "north", "signal_fire" to "false", "waterlogged" to "false"),
                ),
            )

        assertTrue(Campfires.isLitCampfire(lit))
        assertFalse(Campfires.isLitCampfire(out))
        assertTrue(Campfires.isCampfire("soul_campfire"), "a soul campfire cooks the same")
    }
}
