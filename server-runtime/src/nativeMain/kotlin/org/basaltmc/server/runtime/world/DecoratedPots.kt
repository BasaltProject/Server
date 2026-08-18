package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.SoundSource

/**
 * The decorated pot, which holds one stack and nothing else.
 *
 * parity: `DecoratedPotBlock` over `DecoratedPotBlockEntity`. It is a container with one slot and no
 * screen: items go in one at a time by hand, a hopper fills and empties it, and a comparator reads
 * how full that single stack is. Clicking it with an empty hand only makes it wobble.
 */
public object DecoratedPots {
    public val BLOCK: Identifier = Identifier.vanilla("decorated_pot")

    /** parity: `ContainerSingleItem`, which is exactly one slot. */
    public const val SIZE: Int = 1

    /** parity: `DecoratedPotBlockEntity.WobbleStyle`, which travels as the second byte. */
    private const val WOBBLE_POSITIVE = 0
    private const val WOBBLE_NEGATIVE = 1

    /** parity: `Item.getMaxStackSize`, which this build does not read yet. */
    private const val MAX_STACK = 64

    public fun isPot(path: String?): Boolean = path == BLOCK.path

    /** What is in the pot, or null when it is empty. */
    public fun contents(entity: ChunkBlockEntity): ItemStack? = BlockInventory.read(entity.data, SIZE)[0]

    /**
     * parity: `DecoratedPotBlock.useItemOn`: one item goes in, and only if the pot is empty or
     * already holds that same item with room left. Answers the effects, or null when nothing fit.
     */
    public fun insert(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        item: Identifier,
    ): List<BlockEffect>? {
        val entity = potAt(world, x, y, z) ?: return null
        val slots = BlockInventory.read(entity.data, SIZE)
        val held = slots[0]
        if (held != null && (held.item != item || held.count >= MAX_STACK)) return null
        slots[0] = if (held == null) ItemStack(item, 1) else held.copy(count = held.count + 1)
        BlockInventory.write(entity.data, slots)
        chunkOf(world, x, z)?.markDirty()
        // parity: the pitch rises with how full it is, which is what makes filling one audible.
        val bend = (slots[0]?.count ?: 1).toFloat() / MAX_STACK
        return listOf(
            BlockEffect.Event(b0 = 1, b1 = WOBBLE_POSITIVE),
            BlockEffect.Sound("block.decorated_pot.insert", SoundSource.BLOCKS, 1f, 0.7f + 0.5f * bend),
        )
    }

    /** parity: `DecoratedPotBlock.useWithoutItem`, which is a wobble and a thud and nothing else. */
    public fun rebuff(): List<BlockEffect> =
        listOf(
            BlockEffect.Event(b0 = 1, b1 = WOBBLE_NEGATIVE),
            BlockEffect.Sound("block.decorated_pot.insert_fail", SoundSource.BLOCKS, 1f, 1f),
        )

    /** parity: `RandomizableContainer`, which a hopper empties one item at a time like any other. */
    public fun takeOne(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): ItemStack? {
        val entity = potAt(world, x, y, z) ?: return null
        val slots = BlockInventory.read(entity.data, SIZE)
        val held = slots[0] ?: return null
        slots[0] = if (held.count <= 1) null else held.copy(count = held.count - 1)
        BlockInventory.write(entity.data, slots)
        chunkOf(world, x, z)?.markDirty()
        return ItemStack(held.item, 1)
    }

    private fun potAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): ChunkBlockEntity? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (BlockStateDefinitions.blockOf(stateId) != BLOCK) return null
        val chunk = chunkOf(world, x, z) ?: return null
        return chunk.blockEntityAt(x and 15, y, z and 15)
            ?: ChunkBlockEntity(x and 15, y, z and 15, BLOCK, NbtCompound()).also { chunk.setBlockEntity(it) }
    }

    private fun chunkOf(
        world: LoadedWorld,
        x: Int,
        z: Int,
    ) = world.state.chunkIfLoaded(x shr 4, z shr 4)
}
