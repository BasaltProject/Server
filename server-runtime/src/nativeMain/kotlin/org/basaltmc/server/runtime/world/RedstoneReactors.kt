package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.SoundSource
import kotlin.random.Random

/**
 * The blocks that do something when the power around them changes: doors, lamps, bulbs and the rest.
 *
 * parity: each block's own `neighborChanged`, which vanilla runs the moment a neighbour moves rather
 * than on a scheduled tick. They all ask the same question first (`hasNeighborSignal`) and differ
 * only in what they do with the answer.
 *
 * A redstone lamp is the exception worth naming: it lights at once and takes four ticks to go out,
 * which is what stops it flickering.
 */
public object RedstoneReactors {
    public val LAMP: Identifier = Identifier.vanilla("redstone_lamp")

    /** parity: `RedstoneLampBlock`'s own four ticks before it goes dark. */
    private const val LAMP_OFF_DELAY = 4

    /**
     * Lets every reactive block around a change look again. Answers what moved, so the caller can
     * tell the clients.
     */
    public fun update(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<BlockChange> {
        val changes = mutableListOf<BlockChange>()
        react(world, x, y, z, changes)
        for (face in BlockFace.entries) {
            react(world, x + face.stepX, y + face.stepY, z + face.stepZ, changes)
        }
        return changes
    }

    private fun react(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        when {
            path.endsWith("_door") && properties["half"] != null -> door(world, x, y, z, stateId, properties, changes)
            path.endsWith("_trapdoor") || path.endsWith("_fence_gate") ->
                openable(world, x, y, z, stateId, properties, changes)
            path == "redstone_lamp" -> lamp(world, x, y, z, stateId, properties, changes)
            path.endsWith("copper_bulb") -> bulb(world, x, y, z, stateId, properties, changes)
            path == "note_block" -> noteBlock(world, x, y, z, stateId, properties, changes)
            path == "tnt" -> tnt(world, x, y, z, properties, changes)
            path == "powered_rail" || path == "activator_rail" ->
                PoweredRails.update(world, x, y, z)?.let { changes += it }
            path == "bell" -> bell(world, x, y, z, stateId, properties, changes)
            // parity: `ShelfBlock.neighborChanged`, which joins or breaks its chain with the signal.
            Shelves.isShelf(path) -> Shelves.reactToSignal(world, x, y, z, changes)
            else -> Unit
        }
    }

    /**
     * parity: `DoorBlock.neighborChanged`, which asks about its own half *and* the other one: a
     * door opens from a signal reaching either end of it.
     */
    private fun door(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        val otherHalf = if (properties["half"] == "lower") 1 else -1
        val signal =
            Redstone.bestNeighbourSignal(world, x, y, z) > 0 ||
                Redstone.bestNeighbourSignal(world, x, y + otherHalf, z) > 0
        if (signal == (properties["powered"] == "true")) return
        val wanted = mapOf("powered" to signal.toString(), "open" to signal.toString())
        write(world, x, y, z, stateId, wanted, changes, openSound(stateId, signal))
        // parity: `DoorBlock.updateShape`, where the other half copies this one rather than deciding
        // for itself. Without it a door opens halfway, which is not a state vanilla can be in.
        val twinY = y + otherHalf
        val twin = world.state.getBlockState(x, twinY, z) ?: return
        if (BlockStateDefinitions.blockOf(twin) != BlockStateDefinitions.blockOf(stateId)) return
        write(world, x, twinY, z, twin, wanted, changes)
    }

    /** parity: `TrapDoorBlock` and `FenceGateBlock`, which follow the signal with both properties. */
    private fun openable(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (signal == (properties["powered"] == "true")) return
        val wanted = mapOf("powered" to signal.toString(), "open" to signal.toString())
        write(world, x, y, z, stateId, wanted, changes, openSound(stateId, signal))
    }

    /**
     * parity: `RedstoneLampBlock.neighborChanged`: it lights at once and books four ticks to go out,
     * so a signal that blinks does not make the lamp blink with it.
     */
    private fun lamp(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        val lit = properties["lit"] == "true"
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (lit == signal) return
        if (lit) {
            world.state.scheduleBlockTick(x, y, z, LAMP, LAMP_OFF_DELAY)
            return
        }
        write(world, x, y, z, stateId, mapOf("lit" to "true"), changes)
    }

    /** The scheduled half of the lamp: it only goes dark if the signal is still gone. */
    public fun lampTick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (BlockStateDefinitions.blockOf(stateId)?.path != "redstone_lamp") return null
        if (BlockStateDefinitions.propertiesOf(stateId)["lit"] != "true") return null
        if (Redstone.bestNeighbourSignal(world, x, y, z) > 0) return null
        val changes = mutableListOf<BlockChange>()
        write(world, x, y, z, stateId, mapOf("lit" to "false"), changes)
        return changes.firstOrNull()
    }

    /**
     * parity: `CopperBulbBlock.checkAndFlip`: the bulb keeps its light and flips it on the *rising*
     * edge only, which is what makes it a T flip-flop in one block.
     */
    private fun bulb(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        val powered = properties["powered"] == "true"
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (signal == powered) return
        val wanted = mutableMapOf("powered" to signal.toString())
        if (!powered) wanted["lit"] = (properties["lit"] != "true").toString()
        val flipped = wanted["lit"]
        val effects =
            if (flipped == null) {
                emptyList()
            } else {
                listOf(
                    BlockEffect.Sound(
                        if (flipped == "true") "block.copper_bulb.turn_on" else "block.copper_bulb.turn_off",
                        SoundSource.BLOCKS,
                        volume = 1f,
                        pitch = 1f,
                    ),
                )
            }
        write(world, x, y, z, stateId, wanted, changes, effects)
    }

    /**
     * parity: `NoteBlock.neighborChanged`, which plays the note on the way up only: a signal going
     * away is silent.
     */
    private fun noteBlock(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (signal == (properties["powered"] == "true")) return
        val effects = if (signal) NoteBlocks.playNote(world, x, y, z, stateId) else emptyList()
        write(world, x, y, z, stateId, mapOf("powered" to signal.toString()), changes, effects)
    }

    /** parity: `BellBlock.neighborChanged` into `attemptToRing`, which swings it the way it faces. */
    private fun bell(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (signal == (properties["powered"] == "true")) return
        val effects =
            if (!signal) {
                emptyList()
            } else {
                listOf(
                    BlockEffect.Event(b0 = 1, b1 = facingValue(properties["facing"])),
                    BlockEffect.Sound("block.bell.use", SoundSource.BLOCKS, volume = 2f, pitch = 1f),
                )
            }
        write(world, x, y, z, stateId, mapOf("powered" to signal.toString()), changes, effects)
    }

    /** parity: `Direction.get3DDataValue`, which is what a bell's swing travels as. */
    private fun facingValue(facing: String?): Int =
        when (facing) {
            "down" -> 0
            "up" -> 1
            "south" -> 3
            "west" -> 4
            "east" -> 5
            else -> 2
        }

    /**
     * parity: `DoorBlock.playSound` and its two cousins, which is the same sound a hand makes and
     * the same tenth of a tone of randomness.
     */
    private fun openSound(
        stateId: Int,
        open: Boolean,
    ): List<BlockEffect> {
        val sound = BlockInteractions.redstoneSound(stateId, open) ?: return emptyList()
        return listOf(BlockEffect.Sound(sound, SoundSource.BLOCKS, 1f, Random.nextFloat() * 0.1f + 0.9f))
    }

    /** parity: `NoteBlock.neighborChanged`. The note itself is a sound, which is the client's half. */
    private fun powered(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        val signal = Redstone.bestNeighbourSignal(world, x, y, z) > 0
        if (signal == (properties["powered"] == "true")) return
        write(world, x, y, z, stateId, mapOf("powered" to signal.toString()), changes)
    }

    /**
     * parity: `TntBlock.neighborChanged`, which lights it and takes the block away. What is left is
     * the entity, booked for the tick it lands on rather than the block it stood in: the caller
     * spawns it when it sees the block go.
     */
    private fun tnt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
    ) {
        if (properties["unstable"] == "true") return
        if (Redstone.bestNeighbourSignal(world, x, y, z) <= 0) return
        if (!world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) return
        changes += BlockChange(x, y, z, Blocks.AIR.stateId)
        world.state.primeTnt(x, y, z)
    }

    private fun write(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        properties: Map<String, String>,
        changes: MutableList<BlockChange>,
        effects: List<BlockEffect> = emptyList(),
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val turned = BlockStateDefinitions.stateIdOf(block, properties, stateId) ?: return
        if (turned == stateId) return
        if (!world.state.setBlockState(x, y, z, turned)) return
        changes += BlockChange(x, y, z, turned, effects)
    }
}
