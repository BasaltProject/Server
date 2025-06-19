package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * The two blocks that throw their contents at the world when a signal reaches them.
 *
 * parity: `DispenserBlock.neighborChanged`, which books four ticks and remembers it was triggered,
 * and `DropperBlock.dispenseFrom`, which puts the item into whatever container it faces and throws
 * it otherwise. A dispenser with nothing in it plays the click and does nothing.
 *
 * The special behaviours of a dispenser (shooting an arrow, pouring a bucket, shearing a sheep) are
 * `DispenseItemBehavior`s and nearly all of them make an entity, which is §6.4. What both blocks do
 * with an ordinary item is the same, and that is what this does.
 */
@OptIn(ExperimentalUuidApi::class)
public object Dispensers {
    public val DISPENSER: Identifier = Identifier.vanilla("dispenser")

    /** parity: `DispenserBlock`'s own nine slots and its four ticks of delay. */
    public const val SIZE: Int = 9
    private const val DELAY = 4

    /** parity: `DefaultDispenseItemBehavior.spawnItem`, the shove it gives what it throws. */
    private const val SPEED_LOW = 0.2
    private const val SPEED_SPREAD = 0.1

    /**
     * parity: `DispenserBlock.neighborChanged`, which listens at its own block *and* the one above,
     * and only books a turn on the rising edge.
     */
    public fun onNeighbourChange(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        if (path != "dispenser" && path != "dropper") return null
        val triggered = BlockStateDefinitions.propertiesOf(stateId)["triggered"] == "true"
        val signal =
            Redstone.bestNeighbourSignal(world, x, y, z) > 0 || Redstone.bestNeighbourSignal(world, x, y + 1, z) > 0
        if (signal == triggered) return null
        if (signal) world.state.scheduleBlockTick(x, y, z, DISPENSER, DELAY)
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val turned =
            BlockStateDefinitions.stateIdOf(block, mapOf("triggered" to signal.toString()), stateId)
                ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /**
     * The turn itself: one item out of a slot picked at random, into the container in front or into
     * the world. parity: `DispenserBlock.dispenseFrom` and `DropperBlock`'s own.
     */
    public fun fire(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        allocateEntityId: () -> Int,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        if (path != "dispenser" && path != "dropper") return false
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return false
        val slots = BlockInventory.read(entity.data, SIZE)
        val slot = randomFullSlot(slots, random) ?: return false
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return false
        val out = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return false
        val stack = slots[slot] ?: return false

        val into = containerInFront(world, x, y, z, out)
        if (into != null) {
            if (!insertOne(world, into, stack, out.opposite)) return false
        } else {
            throwOut(world, x, y, z, out, stack, random, allocateEntityId)
        }
        slots[slot] = if (stack.count <= 1) null else stack.copy(count = stack.count - 1)
        BlockInventory.write(entity.data, slots)
        chunk.markDirty()
        return true
    }

    /** parity: `DispenserBlockEntity.getRandomSlot`, which picks evenly among the slots with something in. */
    private fun randomFullSlot(
        slots: Array<ItemStack?>,
        random: LegacyRandom,
    ): Int? {
        var chosen: Int? = null
        var seen = 0
        for (slot in slots.indices) {
            if (slots[slot] == null) continue
            seen++
            if (random.nextInt(seen) == 0) chosen = slot
        }
        return chosen
    }

    private fun containerInFront(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        out: BlockFace,
    ): Triple<Int, Int, Int>? {
        val tx = x + out.stepX
        val ty = y + out.stepY
        val tz = z + out.stepZ
        val stateId = world.state.getBlockState(tx, ty, tz) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        if (Containers.SIZES[path] == null) return null
        return Triple(tx, ty, tz)
    }

    private fun insertOne(
        world: LoadedWorld,
        into: Triple<Int, Int, Int>,
        stack: ItemStack,
        from: BlockFace,
    ): Boolean {
        val (tx, ty, tz) = into
        val stateId = world.state.getBlockState(tx, ty, tz) ?: return false
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        val size = Containers.SIZES[path] ?: return false
        val chunk = world.state.chunkIfLoaded(tx shr 4, tz shr 4) ?: return false
        val entity = chunk.blockEntityAt(tx and 15, ty, tz and 15) ?: return false
        val slots = BlockInventory.read(entity.data, size)
        for (slot in slots.indices) {
            val there = slots[slot]
            if (there != null && (there.item != stack.item || there.count >= MAX_STACK)) continue
            slots[slot] = if (there == null) stack.copy(count = 1) else there.copy(count = there.count + 1)
            BlockInventory.write(entity.data, slots)
            chunk.markDirty()
            return true
        }
        return false
    }

    /** parity: `DefaultDispenseItemBehavior.spawnItem`: out of the face, a little below the middle. */
    private fun throwOut(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        out: BlockFace,
        stack: ItemStack,
        random: LegacyRandom,
        allocateEntityId: () -> Int,
    ) {
        val centreX = x + 0.5 + DISPENSE_REACH * out.stepX
        val centreZ = z + 0.5 + DISPENSE_REACH * out.stepZ
        var centreY = y + 0.5 + DISPENSE_REACH * out.stepY
        centreY -= if (out == BlockFace.UP || out == BlockFace.DOWN) 0.125 else 0.15625
        val push = random.nextDouble() * SPEED_SPREAD + SPEED_LOW
        world.state.spawnItem(
            WorldState.DroppedItem(
                entityId = allocateEntityId(),
                uuid = Uuid.random(),
                item = stack.item,
                count = 1,
                x = centreX,
                y = centreY,
                z = centreZ,
                velocityX = out.stepX * push,
                velocityY = 0.2,
                velocityZ = out.stepZ * push,
            ),
        )
    }

    /** parity: `DispenserBlock.getDispensePosition`'s own 0.7 out of the face. */
    private const val DISPENSE_REACH = 0.7
    private const val MAX_STACK = 64
}
