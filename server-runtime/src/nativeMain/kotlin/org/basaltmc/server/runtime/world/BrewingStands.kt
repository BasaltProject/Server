package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Brewing
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtString

/**
 * The brewing stand, one tick at a time.
 *
 * parity: `BrewingStandBlockEntity.serverTick`, in its order: the powder is eaten first and only
 * when the fuel has run out, a brew that stops matching its ingredient is abandoned rather than
 * finished, and the three bottle bits on the block follow whatever is standing in it.
 *
 * The potion a bottle holds is a component in vanilla; here it is kept beside the item in the block
 * entity's own NBT, which is enough to brew with and is marked as the shortcut it is.
 */
public object BrewingStands {
    /** parity: `BrewingStandBlockEntity`'s five slots: three bottles, the ingredient and the fuel. */
    public const val SIZE: Int = 5

    public const val INGREDIENT_SLOT: Int = 3
    public const val FUEL_SLOT: Int = 4

    private const val FUEL_KEY = "Fuel"
    private const val BREW_TIME_KEY = "BrewTime"
    private const val POTIONS_KEY = "BasaltPotions"

    /** A stand as it stands: five slots, the fuel left and how far along the brew is. */
    public class State(
        public var fuel: Int = 0,
        public var brewTime: Int = 0,
        public val items: Array<ItemStack?> = arrayOfNulls(SIZE),
        public val potions: Array<String?> = arrayOfNulls(3),
    )

    /** One turn. Answers whether anything moved, so the caller writes it back only when it did. */
    public fun tick(state: State): Boolean {
        var moved = false
        val fuelStack = state.items[FUEL_SLOT]
        if (state.fuel <= 0 && fuelStack != null && fuelStack.item == Brewing.FUEL) {
            state.fuel = Brewing.FUEL_USES
            state.items[FUEL_SLOT] = if (fuelStack.count <= 1) null else fuelStack.copy(count = fuelStack.count - 1)
            moved = true
        }
        val brewable = isBrewable(state)
        if (state.brewTime > 0) {
            state.brewTime--
            if (state.brewTime == 0 && brewable) {
                brew(state)
            } else if (!brewable) {
                state.brewTime = 0
            }
            return true
        }
        if (brewable && state.fuel > 0) {
            state.fuel--
            state.brewTime = Brewing.BREW_TIME
            return true
        }
        return moved
    }

    /** parity: `isBrewable`: an ingredient that brews, and at least one bottle it can turn. */
    public fun isBrewable(state: State): Boolean {
        val ingredient = state.items[INGREDIENT_SLOT] ?: return false
        if (!Brewing.isIngredient(ingredient.item)) return false
        for (slot in 0 until 3) {
            val bottle = state.items[slot] ?: continue
            if (Brewing.mix(bottle.item, state.potions[slot], ingredient.item) != null) return true
        }
        return false
    }

    /** parity: `doBrew`: every bottle that can turn does, and the ingredient goes. */
    private fun brew(state: State) {
        val ingredient = state.items[INGREDIENT_SLOT] ?: return
        for (slot in 0 until 3) {
            val bottle = state.items[slot] ?: continue
            val mixed = Brewing.mix(bottle.item, state.potions[slot], ingredient.item) ?: continue
            state.items[slot] = bottle.copy(item = mixed.first, count = bottle.count)
            state.potions[slot] = mixed.second
        }
        state.items[INGREDIENT_SLOT] =
            if (ingredient.count <= 1) null else ingredient.copy(count = ingredient.count - 1)
    }

    /** Which of the three bottle slots are taken, which is what the block itself shows. */
    public fun bottleBits(state: State): BooleanArray = BooleanArray(3) { state.items[it] != null }

    public fun read(data: NbtCompound): State {
        val state =
            State(
                fuel = (data[FUEL_KEY] as? NbtInt)?.value ?: 0,
                brewTime = (data[BREW_TIME_KEY] as? NbtInt)?.value ?: 0,
            )
        val slots = BlockInventory.read(data, SIZE)
        for (index in slots.indices) state.items[index] = slots[index]
        for (slot in 0 until 3) {
            state.potions[slot] = (data["$POTIONS_KEY$slot"] as? NbtString)?.value
        }
        return state
    }

    public fun write(
        state: State,
        data: NbtCompound,
    ) {
        data.putInt(FUEL_KEY, state.fuel)
        data.putInt(BREW_TIME_KEY, state.brewTime)
        BlockInventory.write(data, state.items)
        for (slot in 0 until 3) {
            val potion = state.potions[slot]
            if (potion == null) data.remove("$POTIONS_KEY$slot") else data.putString("$POTIONS_KEY$slot", potion)
        }
    }

    public val BLOCK: Identifier = Identifier.vanilla("brewing_stand")
}
