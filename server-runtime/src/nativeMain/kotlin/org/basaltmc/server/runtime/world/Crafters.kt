package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.crafting.CraftingInput
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtIntArray
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * The block that crafts on its own when a signal reaches it.
 *
 * parity: `CrafterBlock`, which is the one block that is a container and a redstone component at
 * once: it books four ticks on the rising edge, crafts what its nine slots make, and puts the result
 * into whatever it faces. Every slot with something in it loses exactly one, whatever the recipe
 * asked for.
 *
 * A slot can be switched off from its own screen, which counts as full for the comparator and
 * refuses items from a hopper. That is kept beside the items in the block entity.
 */
@OptIn(ExperimentalUuidApi::class)
public object Crafters {
    public val BLOCK: Identifier = Identifier.vanilla("crafter")

    /** parity: `CrafterBlock`'s nine slots, its four ticks of delay and its six of animation. */
    public const val SIZE: Int = 9
    private const val TRIGGER_DELAY = 4
    private const val CRAFTING_TICKS = 6

    private const val CRAFTING_TICKS_KEY = "crafting_ticks_remaining"
    private const val DISABLED_SLOTS_KEY = "disabled_slots"

    /**
     * parity: `CrafterBlock.neighborChanged`: on the rising edge it books a turn and remembers it,
     * and when the signal goes it forgets both that and the animation.
     */
    public fun onNeighbourChange(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block.path != "crafter") return null
        val triggered = BlockStateDefinitions.propertiesOf(stateId)["triggered"] == "true"
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (signal == triggered) return null
        val wanted =
            if (signal) {
                world.state.scheduleBlockTick(x, y, z, BLOCK, TRIGGER_DELAY)
                mapOf("triggered" to "true")
            } else {
                mapOf("triggered" to "false", "crafting" to "false")
            }
        val turned = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /**
     * The turn itself. Answers what changed, so the caller can tell the clients.
     *
     * parity: `CrafterBlock.dispenseFrom`: no recipe means the failure sound and nothing else, and a
     * recipe means the result goes out the front and every slot loses one.
     */
    public fun craft(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        resultFor: (CraftingInput) -> ItemStack?,
        allocateEntityId: () -> Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block.path != "crafter") return null
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return null
        val slots = BlockInventory.read(entity.data, SIZE)
        val result = resultFor(inputOf(slots)) ?: return null
        if (result.count <= 0) return null

        val facing = BlockStateDefinitions.propertiesOf(stateId)["orientation"]?.substringBefore('_')
        val out = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: BlockFace.NORTH
        deliver(world, x, y, z, out, result, allocateEntityId)
        for (slot in slots.indices) {
            val there = slots[slot] ?: continue
            slots[slot] = if (there.count <= 1) null else there.copy(count = there.count - 1)
        }
        BlockInventory.write(entity.data, slots)
        entity.data.putInt(CRAFTING_TICKS_KEY, CRAFTING_TICKS)
        chunk.markDirty()
        val crafting = BlockStateDefinitions.stateIdOf(block, mapOf("crafting" to "true"), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, crafting)) return null
        return BlockChange(x, y, z, crafting)
    }

    /** parity: `CrafterBlockEntity.serverTick`, six ticks of animation and then the block goes still. */
    public fun tick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        data: NbtCompound,
    ): BlockChange? {
        val left = (data[CRAFTING_TICKS_KEY] as? NbtInt)?.value ?: 0
        if (left <= 0) return null
        data.putInt(CRAFTING_TICKS_KEY, left - 1)
        if (left - 1 > 0) return null
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val still = BlockStateDefinitions.stateIdOf(block, mapOf("crafting" to "false"), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, still)) return null
        return BlockChange(x, y, z, still)
    }

    /**
     * parity: `CrafterBlockEntity.getRedstoneSignal`, which counts slots rather than items: a slot
     * with one item in it reads the same as a full one, and a slot switched off reads as taken.
     */
    public fun comparatorSignal(data: NbtCompound): Int {
        val slots = BlockInventory.read(data, SIZE)
        val disabled = disabledSlots(data)
        return (0 until SIZE).count { slots[it] != null || it in disabled }
    }

    public fun disabledSlots(data: NbtCompound): Set<Int> {
        val stored = data[DISABLED_SLOTS_KEY] as? NbtIntArray ?: return emptySet()
        return stored.value.toSet()
    }

    /** parity: `CrafterBlockEntity.canPlaceItem`, which is what stops a hopper filling a slot that is off. */
    public fun accepts(
        data: NbtCompound,
        slot: Int,
    ): Boolean = slot !in disabledSlots(data)

    private fun inputOf(slots: Array<ItemStack?>): CraftingInput = CraftingInput.of(GRID, GRID, slots.toList())

    /** parity: `dispenseItem`, which prefers the container in front and throws it out when there is none. */
    private fun deliver(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        out: BlockFace,
        result: ItemStack,
        allocateEntityId: () -> Int,
    ) {
        val tx = x + out.stepX
        val ty = y + out.stepY
        val tz = z + out.stepZ
        if (insert(world, tx, ty, tz, result)) return
        world.state.spawnItem(
            WorldState.DroppedItem(
                entityId = allocateEntityId(),
                uuid = Uuid.random(),
                item = result.item,
                count = result.count,
                x = tx + 0.5,
                y = ty + 0.5,
                z = tz + 0.5,
            ),
        )
    }

    private fun insert(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stack: ItemStack,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        val size = Containers.SIZES[path] ?: return false
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return false
        val slots = BlockInventory.read(entity.data, size)
        for (slot in slots.indices) {
            val there = slots[slot]
            if (there != null && (there.item != stack.item || there.count + stack.count > MAX_STACK)) continue
            slots[slot] = if (there == null) stack else there.copy(count = there.count + stack.count)
            BlockInventory.write(entity.data, slots)
            chunk.markDirty()
            return true
        }
        return false
    }

    private const val GRID = 3
    private const val MAX_STACK = 64
}
