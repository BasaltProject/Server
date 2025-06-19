package org.basaltmc.server.runtime.world

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

/** parity: `AbstractFurnaceBlockEntity.serverTick`. */
class FurnacesTest {
    private val ironOre = Identifier.vanilla("iron_ore")
    private val ironIngot = Identifier.vanilla("iron_ingot")
    private val coal = Identifier.vanilla("coal")

    private val smeltIron =
        Recipe.Cooking(
            group = "",
            result = ItemStack(ironIngot, 1),
            cooker = Recipe.Cooker.SMELTING,
            ingredient = Ingredient(setOf(ironOre)),
            experience = 0.7f,
            cookingTime = 200,
        )

    private val recipes: (ItemStack) -> Recipe.Cooking? = { if (it.item == ironOre) smeltIron else null }

    private fun loaded(
        input: ItemStack? = null,
        fuel: ItemStack? = null,
        result: ItemStack? = null,
    ): Furnaces.State {
        val state = Furnaces.State()
        state.items[0] = input
        state.items[1] = fuel
        state.items[2] = result
        state.cookingTotalTime = Furnaces.cookTimeFor(state, recipes)
        return state
    }

    private fun run(
        state: Furnaces.State,
        ticks: Int,
    ) {
        repeat(ticks) { Furnaces.tick(state, recipes) }
    }

    @Test
    fun itLightsOnlyWhenThereIsSomethingToCook() {
        val idle = loaded(fuel = ItemStack(coal, 1))
        run(idle, 5)

        assertFalse(idle.lit, "fuel alone does not light a furnace")
        assertEquals(1, idle.items[1]?.count, "and the coal is still there")
    }

    @Test
    fun oneCoalSmeltsEightItems() {
        val furnace = loaded(input = ItemStack(ironOre, 64), fuel = ItemStack(coal, 1))

        // The tick that lights the fire also cooks, so the fire outlasts its own ticks by one.
        run(furnace, 1601)

        assertEquals(8, furnace.items[2]?.count, "1600 ticks of coal at 200 a piece")
        assertEquals(ironIngot, furnace.items[2]?.item)
        assertEquals(56, furnace.items[0]?.count, "and eight ores went in")
        assertNull(furnace.items[1], "the coal is gone")
        assertFalse(furnace.lit)
    }

    /** parity: `consumeFuel`, which leaves the bucket behind. */
    @Test
    fun aLavaBucketLeavesTheBucket() {
        val furnace = loaded(input = ItemStack(ironOre, 1), fuel = ItemStack(Identifier.vanilla("lava_bucket"), 1))

        run(furnace, 1)

        assertEquals(Identifier.vanilla("bucket"), furnace.items[1]?.item)
        assertTrue(furnace.lit)
        assertEquals(20000, furnace.litTotalTime)
    }

    /** parity: `canBurn`: a full result slot stops the furnace rather than voiding the item. */
    @Test
    fun aFullResultSlotStopsIt() {
        val furnace =
            loaded(
                input = ItemStack(ironOre, 10),
                fuel = ItemStack(coal, 1),
                result = ItemStack(ironIngot, 64),
            )

        run(furnace, 400)

        assertEquals(64, furnace.items[2]?.count, "nothing was lost")
        assertEquals(10, furnace.items[0]?.count, "and nothing was eaten")
        assertEquals(1, furnace.items[1]?.count, "not even the fuel")
    }

    /** parity: the `cookingTimer - 2` a furnace that ran out of fuel walks back. */
    @Test
    fun progressFallsBackTwiceAsFastAsItRose() {
        val furnace = loaded(input = ItemStack(ironOre, 1), fuel = ItemStack(Identifier.vanilla("stick"), 1))

        run(furnace, 100)
        assertEquals(100, furnace.cookingTimer, "a stick burns for a hundred ticks and cooks on every one")

        run(furnace, 10)
        assertEquals(80, furnace.cookingTimer, "and then it cools two ticks at a time")

        run(furnace, 100)
        assertEquals(0, furnace.cookingTimer, "down to nothing, and no further")
    }

    /** parity: `burn`'s own oddity: a wet sponge fills the bucket that is sitting in the fuel slot. */
    @Test
    fun aWetSpongeFillsTheBucketBesideIt() {
        val sponge = Identifier.vanilla("wet_sponge")
        val dryIt =
            Recipe.Cooking(
                group = "",
                result = ItemStack(Identifier.vanilla("sponge"), 1),
                cooker = Recipe.Cooker.SMELTING,
                ingredient = Ingredient(setOf(sponge)),
                experience = 0.15f,
                cookingTime = 200,
            )
        val state = Furnaces.State()
        state.items[0] = ItemStack(sponge, 1)
        state.items[1] = ItemStack(coal, 1)
        state.cookingTotalTime = 200
        repeat(201) { Furnaces.tick(state, { stack -> if (stack.item == sponge) dryIt else null }) }
        // The coal was eaten on the first tick, so by the time it dries the slot is empty; put a
        // bucket there the way a player would and dry a second one.
        state.items[0] = ItemStack(sponge, 1)
        state.items[1] = ItemStack(Identifier.vanilla("bucket"), 1)
        repeat(200) { Furnaces.tick(state, { stack -> if (stack.item == sponge) dryIt else null }) }

        assertEquals(Identifier.vanilla("water_bucket"), state.items[1]?.item)
    }

    /** A furnace saved mid-smelt comes back where it was. */
    @Test
    fun itSurvivesASaveAndReload() {
        val furnace = loaded(input = ItemStack(ironOre, 3), fuel = ItemStack(coal, 2))
        run(furnace, 50)

        val data = NbtCompound()
        Furnaces.write(furnace, data)
        val reloaded = Furnaces.read(data)

        assertEquals(furnace.litTimeRemaining, reloaded.litTimeRemaining)
        assertEquals(furnace.cookingTimer, reloaded.cookingTimer)
        assertEquals(furnace.cookingTotalTime, reloaded.cookingTotalTime)
        assertEquals(ironOre, reloaded.items[0]?.item)
        assertEquals(3, reloaded.items[0]?.count)
        assertEquals(1, reloaded.items[1]?.count, "one coal was eaten")
    }
}
