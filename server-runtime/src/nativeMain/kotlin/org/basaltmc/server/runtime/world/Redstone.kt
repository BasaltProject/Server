package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import kotlin.math.floor

/**
 * How strong a redstone signal is at a place, and how far the dust carries it.
 *
 * parity: `Level.getBestNeighborSignal` for what reaches a block, and
 * `DefaultRedstoneWireEvaluator` for what the dust itself settles on: the strongest signal touching
 * it, or one less than the strongest dust beside it, whichever is greater. The step up and down a
 * block is the same rule vanilla uses, conductors included.
 *
 * What each component *does* with that signal (a repeater's delay, a comparator's arithmetic, a
 * piston's push) is its own thing; this is only the wire and the sources that carry their strength
 * in their own state.
 */
public object Redstone {
    /** parity: `Redstone.SIGNAL_MAX`. */
    public const val MAX_SIGNAL: Int = 15

    private val HORIZONTAL = listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST, BlockFace.EAST)

    /**
     * What the block at these coordinates gives off when asked from [from].
     *
     * parity: `BlockState.getSignal`, whose `direction` runs from whoever is asking *to* this block,
     * which is what `SignalGetter.hasNeighborSignal` passes: the block below is asked with `DOWN`.
     * Getting that backwards silently powers everything from the wrong side, so it is named for it.
     *
     * Only the sources whose strength is written in their own state are here; the ones that need
     * their block entity answer zero until that lands.
     */
    public fun signalOf(
        stateId: Int,
        from: BlockFace,
        openers: Int = 0,
    ): Int {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return 0
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        return when {
            path == "redstone_block" -> MAX_SIGNAL
            // parity: `RedstoneTorchBlock.getSignal`, which feeds every side but the one above it.
            path == "redstone_torch" -> if (properties["lit"] == "true" && from != BlockFace.UP) MAX_SIGNAL else 0
            // parity: `RedstoneWallTorchBlock.getSignal`, which does not feed the wall it hangs on.
            path == "redstone_wall_torch" ->
                if (properties["lit"] == "true" && properties["facing"] != from.name.lowercase()) MAX_SIGNAL else 0
            // parity: `DiodeBlock.getSignal`: a repeater and a comparator feed the side their input
            // is *not* on, which is what `FACING == direction` says.
            // parity: `DiodeBlock.getSignal` and `ObserverBlock.getSignal`, both of which feed only
            // the side `FACING` names.
            path == "repeater" || path == "comparator" || path == "observer" ->
                if (properties["powered"] == "true" && properties["facing"] == from.name.lowercase()) {
                    MAX_SIGNAL
                } else {
                    0
                }
            path == "lever" || path.endsWith("_button") -> if (properties["powered"] == "true") MAX_SIGNAL else 0
            path.endsWith("_pressure_plate") -> plateSignal(properties)
            path == "light_weighted_pressure_plate" || path == "heavy_weighted_pressure_plate" ->
                properties["power"]?.toIntOrNull() ?: 0
            path == "redstone_wire" -> if (from == BlockFace.DOWN) 0 else properties["power"]?.toIntOrNull() ?: 0
            // parity: `DaylightDetectorBlock`, which feeds every side with what it reads off the sky.
            path == "daylight_detector" -> properties["power"]?.toIntOrNull() ?: 0
            // parity: `DetectorRailBlock`, `LecternBlock` and `TripWireHookBlock`, all fifteen or
            // nothing. What turns them on is a minecart, a page or a tripwire.
            path == "detector_rail" || path == "lectern" || path == "tripwire_hook" ->
                if (properties["powered"] == "true") MAX_SIGNAL else 0
            // parity: `LightningRodBlock`, which stays powered for the eight ticks after a strike.
            path == "lightning_rod" -> if (properties["powered"] == "true") MAX_SIGNAL else 0
            // parity: `TargetBlock` and `SculkSensorBlock`, both of which carry their strength in
            // `power` and count it down on their own scheduled tick.
            path == "target" || path == "sculk_sensor" || path == "calibrated_sculk_sensor" ->
                properties["power"]?.toIntOrNull() ?: 0
            // parity: `TrappedChestBlock.getSignal`, one per person looking inside, capped at fifteen.
            path == "trapped_chest" -> openers.coerceIn(0, MAX_SIGNAL)
            // parity: `JukeboxBlock.ownSignal`, fifteen while a disc is playing.
            path == "jukebox" -> if (properties["has_record"] == "true") MAX_SIGNAL else 0
            else -> 0
        }
    }

    private fun plateSignal(properties: Map<String, String>): Int =
        properties["power"]?.toIntOrNull() ?: if (properties["powered"] == "true") MAX_SIGNAL else 0

    /**
     * The *strong* signal a block pushes into the one at [from]: what travels through a solid block
     * and out the other side.
     *
     * parity: each block's `getDirectSignal`. A lever, a button and a plate strongly power the block
     * they are attached to, a torch the block above it, and a repeater or comparator the block they
     * feed. Everything else is weak, which is why a block of redstone lights the dust beside it but
     * does not put out a torch on the block next to it.
     */
    public fun directSignalOf(
        stateId: Int,
        from: BlockFace,
        openers: Int = 0,
    ): Int {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return 0
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        return when {
            // parity: `RedstoneTorchBlock.getDirectSignal`, only downwards, which is the block above it.
            path == "redstone_torch" -> if (from == BlockFace.DOWN) signalOf(stateId, from) else 0
            path == "redstone_wall_torch" -> if (from == BlockFace.DOWN) signalOf(stateId, from) else 0
            path == "lever" || path.endsWith("_button") ->
                if (properties["powered"] == "true" && attachedFace(properties) == from) MAX_SIGNAL else 0
            path.endsWith("_pressure_plate") ->
                if (from == BlockFace.UP) plateSignal(properties) else 0
            path == "light_weighted_pressure_plate" || path == "heavy_weighted_pressure_plate" ->
                if (from == BlockFace.UP) properties["power"]?.toIntOrNull() ?: 0 else 0
            // parity: an observer and a diode power the block they feed strongly.
            path == "repeater" || path == "comparator" || path == "observer" -> signalOf(stateId, from)
            // parity: a detector rail, a lectern, a sculk sensor and a trapped chest all feed the
            // block underneath them and nothing else.
            path == "detector_rail" ||
                path == "lectern" ||
                path == "sculk_sensor" ||
                path == "calibrated_sculk_sensor" ||
                path == "trapped_chest" ->
                if (from == BlockFace.UP) signalOf(stateId, from, openers) else 0
            // parity: `TripWireHookBlock` and `LightningRodBlock`, which feed the side they face.
            path == "tripwire_hook" || path == "lightning_rod" ->
                if (properties["facing"] == from.name.lowercase()) signalOf(stateId, from) else 0
            else -> 0
        }
    }

    /** parity: `FaceAttachedHorizontalDirectionalBlock.getConnectedDirection`, the side it hangs on. */
    private fun attachedFace(properties: Map<String, String>): BlockFace? =
        when (properties["face"]) {
            "ceiling" -> BlockFace.DOWN
            "floor" -> BlockFace.UP
            else -> BlockFace.entries.firstOrNull { it.name.lowercase() == properties["facing"] }
        }

    /**
     * What one block hands to another, the way `SignalGetter.getSignal` asks it: its own signal, and
     * for a block that conducts, whatever is strongly powering it from any side.
     */
    public fun signalFrom(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        from: BlockFace,
    ): Int {
        val stateId = world.state.getBlockState(x, y, z) ?: return 0
        val own = signalOf(stateId, from, ContainerOpeners.count(world.name, x, y, z))
        if (!BlockSupport.conductsRedstone(stateId)) return own
        return maxOf(own, directSignalTo(world, x, y, z))
    }

    /** parity: `SignalGetter.getDirectSignalTo`, the strongest strong signal reaching a block. */
    public fun directSignalTo(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        var best = 0
        for (face in BlockFace.entries) {
            val nx = x + face.stepX
            val ny = y + face.stepY
            val nz = z + face.stepZ
            val neighbour = world.state.getBlockState(nx, ny, nz) ?: continue
            best = maxOf(best, directSignalOf(neighbour, face, ContainerOpeners.count(world.name, nx, ny, nz)))
            if (best >= MAX_SIGNAL) return MAX_SIGNAL
        }
        return best
    }

    /**
     * What a comparator reads off a container: how full it is, from zero to fifteen.
     *
     * parity: `AbstractContainerMenu.getRedstoneSignalFromContainer`, which averages the fraction of
     * a full stack in every slot and then walks that fraction up to fifteen. One item in a furnace
     * of three slots is already a one, which is the quirk people build with.
     */
    public fun containerSignal(
        slots: List<Pair<Int, Int>>,
        size: Int,
    ): Int {
        if (size <= 0) return 0
        var total = 0f
        for ((count, maxStack) in slots) {
            if (count <= 0) continue
            total += count.toFloat() / maxStack.coerceAtLeast(1)
        }
        total /= size
        // parity: `Mth.lerpDiscrete(fraction, 0, 15)`, which is `floor(fraction * 14)` plus one for
        // anything at all: a single item already reads one, however big the container is.
        if (total <= 0f) return 0
        return (floor(total * (MAX_SIGNAL - 1)).toInt() + 1).coerceIn(0, MAX_SIGNAL)
    }

    /** parity: `Level.getBestNeighborSignal`: the strongest of the six faces around a block. */
    public fun bestNeighbourSignal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        var best = 0
        for (face in BlockFace.entries) {
            best = maxOf(best, signalFrom(world, x + face.stepX, y + face.stepY, z + face.stepZ, face))
            if (best >= MAX_SIGNAL) return MAX_SIGNAL
        }
        return best
    }

    /**
     * What a piece of dust at these coordinates should be carrying.
     *
     * parity: `DefaultRedstoneWireEvaluator.calculateTargetStrength`: a block signal of fifteen wins
     * outright, and otherwise the dust takes the greater of the block signal and one less than the
     * strongest dust it can see, up over a conductor or down off a block that is not one.
     */
    public fun wireStrength(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val blockSignal = blockSignalFor(world, x, y, z)
        if (blockSignal >= MAX_SIGNAL) return MAX_SIGNAL
        return maxOf(blockSignal, incomingWireSignal(world, x, y, z))
    }

    /**
     * parity: `RedStoneWireBlock.getBlockSignal`, which asks the neighbours while pretending the
     * dust itself gives off nothing: without that, two pieces of dust would hold each other up.
     */
    private fun blockSignalFor(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        var best = 0
        for (face in BlockFace.entries) {
            val neighbour = world.state.getBlockState(x + face.stepX, y + face.stepY, z + face.stepZ) ?: continue
            if (isWire(neighbour)) continue
            best = maxOf(best, signalFrom(world, x + face.stepX, y + face.stepY, z + face.stepZ, face))
            if (best >= MAX_SIGNAL) return MAX_SIGNAL
        }
        return best
    }

    /** parity: `RedstoneWireEvaluator.getIncomingWireSignal`, steps up and down included. */
    private fun incomingWireSignal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        var best = 0
        val aboveIsConductor = conducts(world, x, y + 1, z)
        for (face in HORIZONTAL) {
            val nx = x + face.stepX
            val nz = z + face.stepZ
            best = maxOf(best, wirePowerAt(world, nx, y, nz))
            if (conducts(world, nx, y, nz)) {
                if (!aboveIsConductor) best = maxOf(best, wirePowerAt(world, nx, y + 1, nz))
            } else {
                best = maxOf(best, wirePowerAt(world, nx, y - 1, nz))
            }
        }
        return maxOf(0, best - 1)
    }

    private fun wirePowerAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val stateId = world.state.getBlockState(x, y, z) ?: return 0
        if (!isWire(stateId)) return 0
        return BlockStateDefinitions.propertiesOf(stateId)["power"]?.toIntOrNull() ?: 0
    }

    private fun conducts(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        return BlockSupport.conductsRedstone(stateId)
    }

    public fun isWire(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "redstone_wire"

    /**
     * Settles the dust at these coordinates and every piece it reaches, answering what changed.
     *
     * parity: vanilla walks this out one neighbour update at a time, updating a wire and then the
     * wires around it. The order in which the pieces settle is not observable once the dust has
     * stopped moving, and the strengths it stops at are the same, so this walks the connected dust
     * until nothing moves.
     */
    public fun settle(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<BlockChange> {
        val changes = LinkedHashMap<Long, BlockChange>()
        var pending = mutableListOf(intArrayOf(x, y, z))
        var rounds = 0
        while (pending.isNotEmpty() && rounds < MAX_ROUNDS) {
            rounds++
            val next = mutableListOf<IntArray>()
            for (position in pending) {
                val moved = updateWire(world, position[0], position[1], position[2]) ?: continue
                changes[packed(position[0], position[1], position[2])] = moved
                // The dust that can see this one has to be asked again, which is the same set the
                // signal itself travels over.
                for (face in BlockFace.entries) {
                    val nx = position[0] + face.stepX
                    val ny = position[1] + face.stepY
                    val nz = position[2] + face.stepZ
                    next += intArrayOf(nx, ny, nz)
                    next += intArrayOf(nx, ny + 1, nz)
                    next += intArrayOf(nx, ny - 1, nz)
                }
            }
            pending = next
        }
        return changes.values.toList()
    }

    /** Writes the strength a single piece of dust settles on, or null when it did not move. */
    private fun updateWire(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (!isWire(stateId)) return null
        val was = BlockStateDefinitions.propertiesOf(stateId)["power"]?.toIntOrNull() ?: 0
        val wanted = wireStrength(world, x, y, z)
        if (was == wanted) return null
        val turned =
            BlockStateDefinitions.stateIdOf(
                BlockStateDefinitions.blockOf(stateId) ?: return null,
                mapOf("power" to wanted.toString()),
                stateId,
            ) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    private fun packed(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3FFFFFF shl 38) or (z.toLong() and 0x3FFFFFF shl 12) or (y.toLong() and 0xFFF)

    /** Sixteen strengths cannot take more rounds than that to settle, plus one to notice they stopped. */
    private const val MAX_ROUNDS = 17
}
