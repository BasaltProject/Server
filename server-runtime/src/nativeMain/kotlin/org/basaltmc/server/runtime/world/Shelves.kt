package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.SoundSource

/**
 * The shelf: three slots you swap by hand, and a whole hotbar at once when it is powered.
 *
 * parity: `ShelfBlock` over `ShelfBlockEntity`. Powering it joins it to the shelves either side into
 * a chain of at most three (`SideChainPartBlock`), and clicking any of them then swaps the player's
 * nine hotbar slots against the chain's nine. A comparator behind it reads which of the three slots
 * are full as three bits, not how full they are.
 */
public object Shelves {
    /** parity: `ShelfBlockEntity`, three slots. */
    public const val SIZE: Int = 3
    public const val ROWS: Int = 1
    public const val COLUMNS: Int = 3

    /** parity: `ShelfBlock.getMaxChainLength`, three shelves, which is one hotbar. */
    public const val MAX_CHAIN: Int = 3

    private const val UNCONNECTED = "unconnected"
    private const val LEFT = "left"
    private const val RIGHT = "right"
    private const val CENTER = "center"

    public fun isShelf(path: String?): Boolean = path != null && path.endsWith("shelf") && path != "chiseled_bookshelf"

    /** Which slot a click lands on, or null when it was not on the front of the shelf. */
    public fun slotAt(
        stateId: Int,
        face: String,
        clickX: Float,
        clickY: Float,
        clickZ: Float,
    ): Int? {
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return null
        return SelectableSlots.hitSlot(facing, face, clickX, clickY, clickZ, ROWS, COLUMNS)
    }

    /**
     * parity: `swapSingleItem`: whatever is in the slot comes out and whatever was held goes in,
     * both at once, so an empty hand empties the slot and a full one exchanges.
     */
    public fun swap(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        slot: Int,
        held: ItemStack?,
    ): SwapResult? {
        if (slot !in 0 until SIZE) return null
        val entity = entityAt(world, x, y, z) ?: return null
        val slots = BlockInventory.read(entity.data, SIZE)
        val removed = slots[slot]
        if (removed == null && held == null) return null
        slots[slot] = held
        BlockInventory.write(entity.data, slots)
        world.state.chunkIfLoaded(x shr 4, z shr 4)?.markDirty()
        val sound =
            when {
                removed != null && held == null -> "block.shelf.take_item"
                removed != null -> "block.shelf.single_swap"
                else -> "block.shelf.place_item"
            }
        return SwapResult(removed, listOf(BlockEffect.Sound(sound, SoundSource.BLOCKS, 1f, 1f)))
    }

    /** What came off the shelf, and what the swap sounded like. */
    public class SwapResult(
        public val removed: ItemStack?,
        public val effects: List<BlockEffect>,
    )

    /**
     * parity: `swapHotbar`: every shelf in the chain against the hotbar slots that line up with it,
     * counted backwards from the ninth so a chain of one uses the last three.
     *
     * [hotbar] is the player's nine slots, swapped in place. Answers false when nothing moved.
     */
    public fun swapHotbar(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        hotbar: Array<ItemStack?>,
    ): Boolean {
        val chain = chainAt(world, x, y, z)
        if (chain.isEmpty()) return false
        var swapped = false
        for ((index, position) in chain.withIndex()) {
            val entity = entityAt(world, position[0], position[1], position[2]) ?: continue
            val slots = BlockInventory.read(entity.data, SIZE)
            for (slot in 0 until SIZE) {
                val hotbarSlot = hotbar.size - (chain.size - index) * SIZE + slot
                if (hotbarSlot !in hotbar.indices) continue
                val fromPlayer = hotbar[hotbarSlot]
                val fromShelf = slots[slot]
                if (fromPlayer == null && fromShelf == null) continue
                slots[slot] = fromPlayer
                hotbar[hotbarSlot] = fromShelf
                swapped = true
            }
            BlockInventory.write(entity.data, slots)
            world.state.chunkIfLoaded(position[0] shr 4, position[2] shr 4)?.markDirty()
        }
        return swapped
    }

    /**
     * parity: `ShelfBlock.neighborChanged`: the signal flips `powered`, an unpowered shelf leaves
     * its chain, and a powered one joins whatever is beside it.
     */
    public fun reactToSignal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (!isShelf(BlockStateDefinitions.blockOf(stateId)?.path)) return
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (signal == (properties["powered"] == "true")) return
        val wanted = mutableMapOf("powered" to signal.toString())
        if (!signal) wanted["side_chain"] = UNCONNECTED
        val turned = BlockStateDefinitions.stateIdOf(blockOf(stateId), wanted, stateId) ?: return
        if (!world.state.setBlockState(x, y, z, turned)) return
        changes +=
            BlockChange(
                x,
                y,
                z,
                turned,
                listOf(
                    BlockEffect.Sound(
                        if (signal) "block.shelf.activate" else "block.shelf.deactivate",
                        SoundSource.BLOCKS,
                        1f,
                        1f,
                    ),
                ),
            )
        if (signal) connect(world, x, y, z, changes) else disconnect(world, x, y, z, changes)
    }

    /**
     * parity: `getAllBlocksConnectedTo`, left to right: the shelves that share this one's chain,
     * this one included. A shelf with no power is a chain of nothing.
     */
    public fun chainAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray> {
        val stateId = world.state.getBlockState(x, y, z) ?: return emptyList()
        if (!connectable(world, x, y, z)) return emptyList()
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return emptyList()
        val chain = ArrayDeque<IntArray>()
        chain += intArrayOf(x, y, z)
        walk(world, x, y, z, facing, clockwise = true, towards = LEFT) { chain.addFirst(it) }
        walk(world, x, y, z, facing, clockwise = false, towards = RIGHT) { chain.addLast(it) }
        return chain.toList()
    }

    private fun walk(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        facing: String,
        clockwise: Boolean,
        towards: String,
        take: (IntArray) -> Unit,
    ) {
        val step = if (clockwise) turn(facing, 1) else turn(facing, -1)
        for (steps in 1 until MAX_CHAIN) {
            val at = intArrayOf(x + step[0] * steps, y, z + step[1] * steps)
            if (!connectable(world, at[0], at[1], at[2]) || facingOf(world, at) != facing) return
            val part = partOf(world, at)
            if (part == CENTER || part == towards) take(at)
            // parity: `isUnconnectableOrChainEnd`, which stops the walk at anything but a centre.
            if (part != CENTER) return
        }
    }

    /** parity: `updateSelfAndNeighborsOnPoweringUp`, which only joins chains that still fit in three. */
    private fun connect(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return
        val left = neighbour(world, x, y, z, facing, clockwise = true)
        val right = neighbour(world, x, y, z, facing, clockwise = false)
        var part = UNCONNECTED
        var length = 1
        val onLeft = left?.let { chainAt(world, it[0], it[1], it[2]).size } ?: 0
        val onRight = right?.let { chainAt(world, it[0], it[1], it[2]).size } ?: 0
        if (onLeft > 0 && length + onLeft <= MAX_CHAIN) {
            part = connectedToLeft(part)
            setPart(world, checkNotNull(left), connectedToRight(partOf(world, left)), changes)
            length += onLeft
        }
        if (onRight > 0 && length + onRight <= MAX_CHAIN) {
            part = connectedToRight(part)
            setPart(world, checkNotNull(right), connectedToLeft(partOf(world, right)), changes)
        }
        setPart(world, intArrayOf(x, y, z), part, changes)
    }

    /** parity: `updateNeighborsAfterPoweringDown`, which leaves the shelves either side as they were. */
    private fun disconnect(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return
        neighbour(world, x, y, z, facing, clockwise = true)?.let {
            setPart(world, it, disconnectedFromRight(partOf(world, it)), changes)
        }
        neighbour(world, x, y, z, facing, clockwise = false)?.let {
            setPart(world, it, disconnectedFromLeft(partOf(world, it)), changes)
        }
    }

    /** parity: `SideChainPart.whenConnectedToTheLeft`, whose names read the other way round. */
    private fun connectedToLeft(part: String) = if (part == CENTER || part == LEFT) CENTER else RIGHT

    private fun connectedToRight(part: String) = if (part == CENTER || part == RIGHT) CENTER else LEFT

    private fun disconnectedFromRight(part: String) = if (part == RIGHT || part == CENTER) RIGHT else UNCONNECTED

    private fun disconnectedFromLeft(part: String) = if (part == LEFT || part == CENTER) LEFT else UNCONNECTED

    private fun setPart(
        world: LoadedWorld,
        at: IntArray,
        part: String,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(at[0], at[1], at[2]) ?: return
        if (BlockStateDefinitions.propertiesOf(stateId)["side_chain"] == part) return
        val turned =
            BlockStateDefinitions.stateIdOf(blockOf(stateId), mapOf("side_chain" to part), stateId)
                ?: return
        if (!world.state.setBlockState(at[0], at[1], at[2], turned)) return
        changes += BlockChange(at[0], at[1], at[2], turned)
    }

    private fun neighbour(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        facing: String,
        clockwise: Boolean,
    ): IntArray? {
        val step = if (clockwise) turn(facing, 1) else turn(facing, -1)
        val at = intArrayOf(x + step[0], y, z + step[1])
        if (!connectable(world, at[0], at[1], at[2])) return null
        return if (facingOf(world, at) == facing) at else null
    }

    /** parity: `isConnectable`, which is a wooden shelf that is powered and nothing else. */
    private fun connectable(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        if (!isShelf(BlockStateDefinitions.blockOf(stateId)?.path)) return false
        return BlockStateDefinitions.propertiesOf(stateId)["powered"] == "true"
    }

    private fun partOf(
        world: LoadedWorld,
        at: IntArray?,
    ): String {
        val stateId = at?.let { world.state.getBlockState(it[0], it[1], it[2]) } ?: return UNCONNECTED
        return BlockStateDefinitions.propertiesOf(stateId)["side_chain"] ?: UNCONNECTED
    }

    private fun facingOf(
        world: LoadedWorld,
        at: IntArray,
    ): String? {
        val stateId = world.state.getBlockState(at[0], at[1], at[2]) ?: return null
        return BlockStateDefinitions.propertiesOf(stateId)["facing"]
    }

    private fun blockOf(stateId: Int) = checkNotNull(BlockStateDefinitions.blockOf(stateId))

    /** parity: `Direction.getClockWise`/`getCounterClockWise`, which is how a shelf finds its sides. */
    private fun turn(
        facing: String,
        way: Int,
    ): IntArray {
        val order = listOf("north", "east", "south", "west")
        val index = order.indexOf(facing)
        if (index < 0) return intArrayOf(0, 0)
        return when (order[(index + way + order.size) % order.size]) {
            "north" -> intArrayOf(0, -1)
            "south" -> intArrayOf(0, 1)
            "west" -> intArrayOf(-1, 0)
            else -> intArrayOf(1, 0)
        }
    }

    /**
     * parity: `ShelfBlock.getAnalogOutputSignal`, three bits, one per slot, and only read from
     * behind: a comparator at the side of a shelf gets nothing.
     */
    public fun comparatorSignal(
        entity: ChunkBlockEntity?,
        facing: String?,
        from: String?,
    ): Int {
        if (facing == null || from == null) return 0
        if (from != opposite(facing)) return 0
        val slots = BlockInventory.read(entity?.data ?: return 0, SIZE)
        var signal = 0
        for (slot in 0 until SIZE) {
            if (slots[slot] != null) signal = signal or (1 shl slot)
        }
        return signal
    }

    private fun opposite(facing: String) =
        when (facing) {
            "north" -> "south"
            "south" -> "north"
            "west" -> "east"
            else -> "west"
        }

    private fun entityAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): ChunkBlockEntity? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (!isShelf(block.path)) return null
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        return chunk.blockEntityAt(x and 15, y, z and 15)
            ?: ChunkBlockEntity(x and 15, y, z and 15, block, NbtCompound()).also { chunk.setBlockEntity(it) }
    }
}
