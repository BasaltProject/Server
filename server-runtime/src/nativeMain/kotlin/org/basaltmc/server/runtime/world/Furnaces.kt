package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Fuels
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * The three blocks that smelt, one tick at a time.
 *
 * parity: `AbstractFurnaceBlockEntity.serverTick`, followed line by line. The fire burns down
 * whether or not anything is cooking, the fuel is only eaten when there is something to cook and
 * room for the result, and a furnace whose fire went out loses its progress two ticks at a time
 * rather than all at once.
 *
 * The counters live in the block entity's own NBT under vanilla's keys, so a furnace survives a
 * save and reload mid-smelt.
 */
public object Furnaces {
    /** parity: the three cookers that are block entities. A campfire cooks too, but on its own clock. */
    public val COOKERS: Map<String, Recipe.Cooker> =
        mapOf(
            "furnace" to Recipe.Cooker.SMELTING,
            "blast_furnace" to Recipe.Cooker.BLASTING,
            "smoker" to Recipe.Cooker.SMOKING,
        )

    /** parity: `AbstractFurnaceMenu`'s slot order, which the window and the NBT share. */
    public const val INPUT_SLOT: Int = 0
    public const val FUEL_SLOT: Int = 1
    public const val RESULT_SLOT: Int = 2

    private const val INPUT = INPUT_SLOT
    private const val FUEL = FUEL_SLOT
    private const val RESULT = RESULT_SLOT

    /** parity: `AbstractFurnaceBlockEntity`'s own save keys. */
    private const val LIT_REMAINING = "lit_time_remaining"
    private const val LIT_TOTAL = "lit_total_time"
    private const val COOKED = "cooking_time_spent"
    private const val COOK_TOTAL = "cooking_total_time"

    /** parity: the two ticks a cooling furnace walks its progress back by. */
    private const val COOLING_STEP = 2

    /** A furnace as it stands: three slots and four counters. */
    public class State(
        public var litTimeRemaining: Int = 0,
        public var litTotalTime: Int = 0,
        public var cookingTimer: Int = 0,
        public var cookingTotalTime: Int = 0,
        public val items: Array<ItemStack?> = arrayOfNulls(3),
    ) {
        public val lit: Boolean get() = litTimeRemaining > 0
    }

    /**
     * One tick of the furnace.
     *
     * Answers whether anything at all moved, counters included, which is not the same question as
     * vanilla's `changed`: that one only decides when to save. Here the state lives in NBT and has
     * to be written back whenever it moved, so an idle furnace is the only one that answers no.
     */
    public fun tick(
        state: State,
        recipeFor: (ItemStack) -> Recipe.Cooking?,
        maxStackSize: Int = DEFAULT_STACK_SIZE,
    ): Boolean {
        var moved = false
        val wasLit = state.litTimeRemaining > 0
        var isLit = wasLit
        if (wasLit) {
            state.litTimeRemaining--
            isLit = state.litTimeRemaining > 0
            moved = true
        }
        val fuel = state.items[FUEL]
        val ingredient = state.items[INPUT]
        if (isLit || (fuel != null && ingredient != null)) {
            if (ingredient != null) {
                val recipe = recipeFor(ingredient)
                val result = recipe?.result
                if (result != null && canBurn(state, maxStackSize, result)) {
                    if (!isLit && fuel != null) {
                        val burn = Fuels.burnTicks(fuel.item)
                        state.litTimeRemaining = burn
                        state.litTotalTime = burn
                        if (burn > 0) {
                            consumeFuel(state, fuel)
                            isLit = true
                            moved = true
                        }
                    }
                    if (isLit) {
                        state.cookingTimer++
                        moved = true
                        if (state.cookingTimer == state.cookingTotalTime) {
                            state.cookingTimer = 0
                            state.cookingTotalTime = recipe.cookingTime
                            burn(state, ingredient, result)
                        }
                    } else {
                        moved = moved || state.cookingTimer != 0
                        state.cookingTimer = 0
                    }
                } else {
                    moved = moved || state.cookingTimer != 0
                    state.cookingTimer = 0
                }
            } else {
                moved = moved || state.cookingTimer != 0
                state.cookingTimer = 0
            }
        } else if (state.cookingTimer > 0) {
            state.cookingTimer = (state.cookingTimer - COOLING_STEP).coerceIn(0, state.cookingTotalTime)
            moved = true
        }
        return moved
    }

    /**
     * What the next item will take, which vanilla asks for whenever the input slot changes rather
     * than every tick. parity: `AbstractFurnaceBlockEntity.getTotalCookTime`.
     */
    public fun cookTimeFor(
        state: State,
        recipeFor: (ItemStack) -> Recipe.Cooking?,
    ): Int {
        val ingredient = state.items[INPUT] ?: return 0
        return recipeFor(ingredient)?.cookingTime ?: 0
    }

    /** parity: `canBurn`: an empty result slot always takes it, a matching one takes it while it fits. */
    private fun canBurn(
        state: State,
        maxStackSize: Int,
        result: ItemStack,
    ): Boolean {
        val inResult = state.items[RESULT] ?: return true
        if (inResult.item != result.item) return false
        return inResult.count + result.count <= maxStackSize
    }

    /** parity: `burn`, including the bucket a wet sponge fills as it dries. */
    private fun burn(
        state: State,
        ingredient: ItemStack,
        result: ItemStack,
    ) {
        val inResult = state.items[RESULT]
        state.items[RESULT] =
            if (inResult == null) result.copy() else inResult.copy(count = inResult.count + result.count)
        if (ingredient.item == WET_SPONGE && state.items[FUEL]?.item == BUCKET) {
            state.items[FUEL] = ItemStack(WATER_BUCKET, 1)
        }
        state.items[INPUT] = shrink(ingredient)
    }

    /** parity: `consumeFuel`: a bucket of lava leaves the bucket behind. */
    private fun consumeFuel(
        state: State,
        fuel: ItemStack,
    ) {
        val left = shrink(fuel)
        state.items[FUEL] = left ?: remainderOf(fuel.item)
    }

    private fun shrink(stack: ItemStack): ItemStack? =
        if (stack.count <= 1) null else stack.copy(count = stack.count - 1)

    /** parity: `Item.getCraftingRemainder`, which for the fuels that have one is always a bucket. */
    private fun remainderOf(item: Identifier): ItemStack? = if (item == LAVA_BUCKET) ItemStack(BUCKET, 1) else null

    /** Reads the counters and slots a saved furnace left behind. */
    public fun read(data: NbtCompound): State {
        val state =
            State(
                litTimeRemaining = data.intOr(LIT_REMAINING),
                litTotalTime = data.intOr(LIT_TOTAL),
                cookingTimer = data.intOr(COOKED),
                cookingTotalTime = data.intOr(COOK_TOTAL),
            )
        val slots = BlockInventory.read(data, state.items.size)
        for (index in state.items.indices) state.items[index] = slots[index]
        return state
    }

    /** Writes it back in the same shape, so the world file stays vanilla's. */
    public fun write(
        state: State,
        data: NbtCompound,
    ) {
        data.putInt(LIT_REMAINING, state.litTimeRemaining)
        data.putInt(LIT_TOTAL, state.litTotalTime)
        data.putInt(COOKED, state.cookingTimer)
        data.putInt(COOK_TOTAL, state.cookingTotalTime)
        BlockInventory.write(data, state.items)
    }

    private fun NbtCompound.intOr(key: String): Int = (this[key] as? org.basaltmc.wire.nbt.NbtInt)?.value ?: 0

    private val WET_SPONGE = Identifier.vanilla("wet_sponge")
    private val BUCKET = Identifier.vanilla("bucket")
    private val WATER_BUCKET = Identifier.vanilla("water_bucket")
    private val LAVA_BUCKET = Identifier.vanilla("lava_bucket")

    /** parity: `AbstractFurnaceBlockEntity.getMaxStackSize`. */
    public const val DEFAULT_STACK_SIZE: Int = 64
}
