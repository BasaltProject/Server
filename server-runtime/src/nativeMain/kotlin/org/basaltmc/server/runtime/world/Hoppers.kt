package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt

/**
 * The block that moves one item at a time, which is where redstone and containers meet.
 *
 * parity: `HopperBlockEntity.tryMoveItems`: nothing happens while it is on cooldown or while its
 * `enabled` is false, it pushes one item into whatever it faces, pulls one out of whatever is above
 * it, and any move at all puts it back on an eight tick cooldown.
 *
 * Picking items up off the floor is an entity's job and is not here.
 */
public object Hoppers {
    /** parity: `HopperBlockEntity.HOPPER_CONTAINER_SIZE`. */
    public const val SIZE: Int = 5

    /** parity: `HopperBlockEntity.MOVE_ITEM_SPEED`, the eight ticks between moves. */
    private const val COOLDOWN = 8

    private const val COOLDOWN_KEY = "TransferCooldown"

    /** parity: the sides `AbstractFurnaceBlockEntity.getSlotsForFace` opens to a hopper. */
    private val FURNACE_SLOTS_FOR_UP = intArrayOf(0)
    private val FURNACE_SLOTS_FOR_DOWN = intArrayOf(2, 1)
    private val FURNACE_SLOTS_FOR_SIDES = intArrayOf(1)

    /**
     * One hopper's turn. Answers whether anything moved, so the caller marks the chunk and tells
     * whoever is looking at the two containers.
     */
    public fun tick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        if (BlockStateDefinitions.blockOf(stateId)?.path != "hopper") return false
        val entity = entityAt(world, x, y, z) ?: return false
        val cooldown = (entity.data[COOLDOWN_KEY] as? NbtInt)?.value ?: 0
        if (cooldown > 0) {
            entity.data.putInt(COOLDOWN_KEY, cooldown - 1)
            return true
        }
        // parity: `HopperBlock.ENABLED`, which a redstone signal turns off.
        if (BlockStateDefinitions.propertiesOf(stateId)["enabled"] == "false") return false
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return false
        val out = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return false
        val slots = BlockInventory.read(entity.data, SIZE)
        var moved = push(world, x, y, z, out, slots)
        if (!moved) moved = pull(world, x, y, z, slots)
        if (!moved) return false
        BlockInventory.write(entity.data, slots)
        entity.data.putInt(COOLDOWN_KEY, COOLDOWN)
        return true
    }

    /** parity: `ejectItems`: the first slot with something in it gives one item to what it faces. */
    private fun push(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        out: BlockFace,
        slots: Array<ItemStack?>,
    ): Boolean {
        val tx = x + out.stepX
        val ty = y + out.stepY
        val tz = z + out.stepZ
        val target = containerAt(world, tx, ty, tz) ?: return false
        for (slot in slots.indices) {
            val stack = slots[slot] ?: continue
            if (!insert(world, tx, ty, tz, target, stack, out.opposite)) continue
            slots[slot] = if (stack.count <= 1) null else stack.copy(count = stack.count - 1)
            return true
        }
        return false
    }

    /** parity: `suckInItems` for the container half: one item out of whatever sits on top. */
    private fun pull(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        slots: Array<ItemStack?>,
    ): Boolean {
        val above = containerAt(world, x, y + 1, z) ?: return false
        val source = BlockInventory.read(above.data, above.size)
        val takeable = slotsFor(above.path, BlockFace.DOWN)
        for (slot in takeable) {
            val stack = source.getOrNull(slot) ?: continue
            if (!accepts(slots, stack)) continue
            add(slots, stack)
            source[slot] = if (stack.count <= 1) null else stack.copy(count = stack.count - 1)
            BlockInventory.write(above.data, source)
            return true
        }
        return false
    }

    /** Puts one of [stack] into the container, answering whether it went in. */
    private fun insert(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        target: Container,
        stack: ItemStack,
        from: BlockFace,
    ): Boolean {
        val slots = BlockInventory.read(target.data, target.size)
        val allowed = slotsFor(target.path, from)
        val one = stack.copy(count = 1)
        for (slot in allowed) {
            val there = slots.getOrNull(slot)
            if (there != null && (there.item != one.item || there.count >= MAX_STACK)) continue
            slots[slot] = if (there == null) one else there.copy(count = there.count + 1)
            BlockInventory.write(target.data, slots)
            world.state.chunkIfLoaded(x shr 4, z shr 4)?.markDirty()
            return true
        }
        return false
    }

    private fun accepts(
        slots: Array<ItemStack?>,
        stack: ItemStack,
    ): Boolean = slots.any { it == null || (it.item == stack.item && it.count < MAX_STACK) }

    private fun add(
        slots: Array<ItemStack?>,
        stack: ItemStack,
    ) {
        for (slot in slots.indices) {
            val there = slots[slot]
            if (there != null && (there.item != stack.item || there.count >= MAX_STACK)) continue
            slots[slot] = if (there == null) stack.copy(count = 1) else there.copy(count = there.count + 1)
            return
        }
    }

    /**
     * parity: `WorldlyContainer.getSlotsForFace`: a furnace takes fuel from the side, ingredients
     * from above and gives its result downwards. Everything else opens every slot to every side.
     */
    private fun slotsFor(
        path: String,
        from: BlockFace,
    ): IntArray {
        if (path !in Furnaces.COOKERS) return IntArray(Containers.SIZES[path] ?: 0) { it }
        return when (from) {
            BlockFace.DOWN -> FURNACE_SLOTS_FOR_DOWN
            BlockFace.UP -> FURNACE_SLOTS_FOR_UP
            else -> FURNACE_SLOTS_FOR_SIDES
        }
    }

    /** A container this build keeps contents for, with the block entity that holds them. */
    private class Container(
        val path: String,
        val size: Int,
        val data: NbtCompound,
    )

    private fun containerAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Container? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        val size = if (path == "hopper") SIZE else Containers.SIZES[path] ?: return null
        val entity = entityAt(world, x, y, z) ?: return null
        return Container(path, size, entity.data)
    }

    private fun entityAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): org.basaltmc.server.world.ChunkBlockEntity? {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        return chunk.blockEntityAt(x and 15, y, z and 15)
    }

    /** parity: `Item.getMaxStackSize`, which this build does not read yet. */
    private const val MAX_STACK = 64
}
