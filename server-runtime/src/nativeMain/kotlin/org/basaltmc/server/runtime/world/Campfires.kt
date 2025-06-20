package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIntArray

/**
 * The campfire, which cooks four things at once and hands each one back on the ground.
 *
 * parity: `CampfireBlockEntity.cookTick` while it is lit and `cooldownTick` while it is not: what is
 * on a campfire that goes out loses its progress two at a time rather than all at once. Nothing is
 * taken out by hand: when a slice is done it drops beside the fire.
 */
public object Campfires {
    public val BLOCK: Identifier = Identifier.vanilla("campfire")

    /** parity: `CampfireBlockEntity`'s four slots, one per side of the fire. */
    public const val SIZE: Int = 4

    private const val PROGRESS_KEY = "CookingTimes"
    private const val TOTAL_KEY = "CookingTotalTimes"

    /** parity: `cooldownTick`, which takes two off the progress of each slot per tick. */
    private const val COOLDOWN_STEP = 2

    /** What is on the fire, and how far along each piece is. */
    public class State(
        public val items: Array<ItemStack?> = arrayOfNulls(SIZE),
        public val progress: IntArray = IntArray(SIZE),
        public val total: IntArray = IntArray(SIZE),
    )

    /**
     * One turn. Answers the slots that finished, so the caller can drop them; whether anything moved
     * is [State] having changed, which the caller writes back.
     */
    public fun tick(
        state: State,
        lit: Boolean,
        resultFor: (ItemStack) -> Recipe.Cooking?,
    ): List<ItemStack> {
        if (!lit) {
            for (slot in 0 until SIZE) {
                if (state.progress[slot] > 0) {
                    state.progress[slot] = (state.progress[slot] - COOLDOWN_STEP).coerceIn(0, state.total[slot])
                }
            }
            return emptyList()
        }
        val done = mutableListOf<ItemStack>()
        for (slot in 0 until SIZE) {
            val stack = state.items[slot] ?: continue
            state.progress[slot]++
            if (state.progress[slot] < state.total[slot]) continue
            done += resultFor(stack)?.result ?: stack
            state.items[slot] = null
            state.progress[slot] = 0
            state.total[slot] = 0
        }
        return done
    }

    /**
     * parity: `placeFood`: the first free slot takes it, at the recipe's own cooking time, and food
     * with no campfire recipe is refused rather than left sitting there.
     */
    public fun placeFood(
        state: State,
        stack: ItemStack,
        resultFor: (ItemStack) -> Recipe.Cooking?,
    ): Boolean {
        for (slot in 0 until SIZE) {
            if (state.items[slot] != null) continue
            val recipe = resultFor(stack) ?: return false
            state.items[slot] = stack.copy(count = 1)
            state.progress[slot] = 0
            state.total[slot] = recipe.cookingTime
            return true
        }
        return false
    }

    /** parity: `CampfireBlock.LIT`, and a soul campfire cooks the same as an ordinary one. */
    public fun isLitCampfire(stateId: Int): Boolean {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        if (path != "campfire" && path != "soul_campfire") return false
        return BlockStateDefinitions.propertiesOf(stateId)["lit"] == "true"
    }

    public fun isCampfire(path: String): Boolean = path == "campfire" || path == "soul_campfire"

    public fun read(data: NbtCompound): State {
        val state = State()
        val slots = BlockInventory.read(data, SIZE)
        for (index in slots.indices) state.items[index] = slots[index]
        (data[PROGRESS_KEY] as? NbtIntArray)?.value?.let { stored ->
            for (index in 0 until minOf(SIZE, stored.size)) state.progress[index] = stored[index]
        }
        (data[TOTAL_KEY] as? NbtIntArray)?.value?.let { stored ->
            for (index in 0 until minOf(SIZE, stored.size)) state.total[index] = stored[index]
        }
        return state
    }

    public fun write(
        state: State,
        data: NbtCompound,
    ) {
        BlockInventory.write(data, state.items)
        data[PROGRESS_KEY] = NbtIntArray(state.progress.copyOf())
        data[TOTAL_KEY] = NbtIntArray(state.total.copyOf())
    }
}
