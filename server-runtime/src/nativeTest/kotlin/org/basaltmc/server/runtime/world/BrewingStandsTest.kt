package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Brewing
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `BrewingStandBlockEntity.serverTick` and the table `PotionBrewing.bootstrap` builds. */
class BrewingStandsTest {
    private val potion = Identifier.vanilla("potion")
    private val blazePowder = Identifier.vanilla("blaze_powder")

    private fun stand(
        bottles: Int = 1,
        water: Boolean = true,
        ingredient: Identifier? = Identifier.vanilla("nether_wart"),
        fuel: Int = 1,
    ): BrewingStands.State {
        val state = BrewingStands.State()
        for (slot in 0 until bottles) {
            state.items[slot] = ItemStack(potion, 1)
            state.potions[slot] = if (water) "water" else "awkward"
        }
        ingredient?.let { state.items[BrewingStands.INGREDIENT_SLOT] = ItemStack(it, 1) }
        if (fuel > 0) state.items[BrewingStands.FUEL_SLOT] = ItemStack(blazePowder, fuel)
        return state
    }

    /** parity: the table itself. Water plus nether wart is the first brew everybody makes. */
    @Test
    fun theTableCameOutOfTheJar() {
        assertTrue(Brewing.all.size > 40, "only ${Brewing.all.size} mixes")
        assertTrue(Brewing.isIngredient(Identifier.vanilla("nether_wart")))
        // Stone really is one in 26.2 (water and stone make a mundane potion, awkward and stone an
        // infested one), so the negative case has to be something else.
        assertTrue(Brewing.isIngredient(Identifier.vanilla("stone")))
        assertFalse(Brewing.isIngredient(Identifier.vanilla("oak_planks")))

        val mixed = checkNotNull(Brewing.mix(potion, "water", Identifier.vanilla("nether_wart")))
        assertEquals("awkward", mixed.second, "water and nether wart make an awkward potion")

        val splash = checkNotNull(Brewing.mix(potion, "awkward", Identifier.vanilla("gunpowder")))
        assertEquals(Identifier.vanilla("splash_potion"), splash.first, "gunpowder changes the bottle")
        assertEquals("awkward", splash.second, "and leaves what is in it alone")
    }

    /** parity: one powder is twenty brews. */
    @Test
    fun itEatsThePowderBeforeItStarts() {
        val state = stand()

        BrewingStands.tick(state)

        assertEquals(Brewing.FUEL_USES - 1, state.fuel, "twenty uses, one spent on this brew")
        assertNull(state.items[BrewingStands.FUEL_SLOT], "and the powder is gone")
        assertEquals(Brewing.BREW_TIME, state.brewTime)
    }

    @Test
    fun fourHundredTicksLaterTheBottleHasTurned() {
        val state = stand(bottles = 3)

        repeat(Brewing.BREW_TIME + 1) { BrewingStands.tick(state) }

        for (slot in 0 until 3) assertEquals("awkward", state.potions[slot], "slot $slot")
        assertNull(state.items[BrewingStands.INGREDIENT_SLOT], "the wart is gone")
        assertEquals(0, state.brewTime)
    }

    /** parity: a brew whose ingredient is taken away is abandoned rather than finished. */
    @Test
    fun takingTheIngredientOutStopsIt() {
        val state = stand()
        BrewingStands.tick(state)
        assertEquals(Brewing.BREW_TIME, state.brewTime)

        state.items[BrewingStands.INGREDIENT_SLOT] = null
        BrewingStands.tick(state)

        assertEquals(0, state.brewTime, "it gives up instead of brewing nothing")
        assertEquals("water", state.potions[0], "and the bottle is untouched")
    }

    @Test
    fun withoutFuelNothingHappens() {
        val state = stand(fuel = 0)

        repeat(10) { BrewingStands.tick(state) }

        assertEquals(0, state.brewTime)
        assertEquals("water", state.potions[0])
    }

    /** The three bits the block shows, and a save that survives a reload. */
    @Test
    fun itSavesWhereItGotTo() {
        val state = stand(bottles = 2)
        BrewingStands.tick(state)
        val data = NbtCompound()
        BrewingStands.write(state, data)

        val back = BrewingStands.read(data)

        assertEquals(state.fuel, back.fuel)
        assertEquals(state.brewTime, back.brewTime)
        assertEquals("water", back.potions[0])
        assertEquals(listOf(true, true, false), BrewingStands.bottleBits(back).toList())
    }
}
