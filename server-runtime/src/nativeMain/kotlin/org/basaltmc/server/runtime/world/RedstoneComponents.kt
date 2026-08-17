package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier

/**
 * The pieces that read a signal and answer with one of their own: the torch that inverts and the
 * repeater that delays.
 *
 * parity: `RedstoneTorchBlock.tick` and `DiodeBlock.tick`, both of which act on a scheduled tick
 * rather than the moment their neighbour moves. That delay is not a detail: it is what makes a
 * repeater a repeater and what stops a torch from oscillating within one tick.
 *
 * The burn-out of a torch flicked more than eight times in sixty ticks is not modelled yet, and is
 * marked as such where it belongs.
 */
public object RedstoneComponents {
    public val TORCH: Identifier = Identifier.vanilla("redstone_torch")
    public val REPEATER: Identifier = Identifier.vanilla("repeater")

    public val COMPARATOR: Identifier = Identifier.vanilla("comparator")

    /** parity: `ComparatorBlock`'s own two-tick turn. */
    private const val COMPARATOR_DELAY = 2

    /** parity: `RedstoneTorchBlock`'s own one-tick delay. */
    private const val TORCH_DELAY = 2

    /** Every block this knows how to tick, so a caller can ask before scheduling. */
    public fun isComponent(stateId: Int): Boolean = pathOf(stateId) in COMPONENTS

    /**
     * Asks each component around a change to look again, which is how vanilla's neighbour updates
     * reach them. parity: `DiodeBlock.checkTickOnNeighbor` and the torch's own scheduling.
     */
    public fun scheduleAround(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ) {
        schedule(world, x, y, z)
        for (face in BlockFace.entries) {
            schedule(world, x + face.stepX, y + face.stepY, z + face.stepZ)
        }
    }

    private fun schedule(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val path = pathOf(stateId) ?: return
        when (path) {
            "redstone_torch", "redstone_wall_torch" ->
                world.state.scheduleBlockTick(x, y, z, TORCH, TORCH_DELAY)
            "repeater" -> {
                // A repeater already on its way to turning off keeps that appointment, which is what
                // `hasScheduledTick` guards in vanilla.
                val delay = (BlockStateDefinitions.propertiesOf(stateId)["delay"]?.toIntOrNull() ?: 1) * 2
                world.state.scheduleBlockTick(x, y, z, REPEATER, delay)
            }
            "comparator" -> world.state.scheduleBlockTick(x, y, z, COMPARATOR, COMPARATOR_DELAY)
            else -> Unit
        }
    }

    /**
     * One component's turn. Answers the new state when it moved, so the caller can tell the clients
     * and carry the change on to the dust around it.
     */
    public fun tick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? =
        when (pathOf(world.state.getBlockState(x, y, z) ?: return null)) {
            "redstone_torch", "redstone_wall_torch" -> torchTick(world, x, y, z)
            "repeater" -> repeaterTick(world, x, y, z)
            "comparator" -> comparatorTick(world, x, y, z)
            else -> null
        }

    /**
     * parity: `RedstoneTorchBlock.tick`: lit while the block it hangs on is not powered, out while
     * it is. The burn-out after eight flicks in sixty ticks is not modelled.
     */
    private fun torchTick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val lit = properties["lit"] == "true"
        val fed = torchIsFed(world, x, y, z, properties)
        if (lit != fed) return null
        return turn(world, x, y, z, stateId, "lit", (!lit).toString())
    }

    /**
     * parity: `RedstoneTorchBlock.hasNeighborSignal`, which asks the block below, and
     * `RedstoneWallTorchBlock`'s own, which asks the wall behind it.
     */
    private fun torchIsFed(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        properties: Map<String, String>,
    ): Boolean {
        val facing = properties["facing"]
        val behind =
            if (facing == null) {
                BlockFace.DOWN
            } else {
                BlockFace.entries.firstOrNull { it.name.lowercase() == facing }?.opposite ?: BlockFace.DOWN
            }
        return Redstone.signalFrom(world, x + behind.stepX, y + behind.stepY, z + behind.stepZ, behind) > 0
    }

    /**
     * parity: `DiodeBlock.tick`: a repeater that should be off turns off, and one that should be on
     * turns on and books another turn to check whether the input has already gone.
     */
    private fun repeaterTick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (isLocked(world, x, y, z, properties)) return null
        val powered = properties["powered"] == "true"
        val shouldPower = repeaterInput(world, x, y, z, properties) > 0
        if (powered == shouldPower) return null
        val delay = (properties["delay"]?.toIntOrNull() ?: 1) * 2
        if (!shouldPower) {
            // Turning off is booked one delay later in vanilla, which this reaches by scheduling.
            world.state.scheduleBlockTick(x, y, z, REPEATER, delay)
        }
        return turn(world, x, y, z, stateId, "powered", shouldPower.toString())
    }

    /**
     * parity: `ComparatorBlock.tick` and its `shouldTurnOn`: a comparator is on while its rear input
     * beats both sides, or ties them while it is comparing rather than subtracting.
     *
     * The strength it puts out lives in vanilla's own block entity; here the state's `powered` says
     * whether it is on, and how strong is worked out when somebody asks.
     */
    private fun comparatorTick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val input = comparatorInput(world, x, y, z, properties)
        val sides = alternateSignal(world, x, y, z, properties)
        val subtracting = properties["mode"] == "subtract"
        val shouldPower =
            when {
                input == 0 -> false
                input > sides -> true
                else -> input == sides && !subtracting
            }
        if ((properties["powered"] == "true") == shouldPower) return null
        return turn(world, x, y, z, stateId, "powered", shouldPower.toString())
    }

    /**
     * What a comparator reads behind itself: a container's fullness first, and otherwise the plain
     * signal. parity: `ComparatorBlock.getInputSignal`.
     */
    public fun comparatorInput(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        properties: Map<String, String>,
    ): Int {
        val facing = properties["facing"] ?: return 0
        val face = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return 0
        val bx = x + face.stepX
        val by = y + face.stepY
        val bz = z + face.stepZ
        Containers.analogSignal(world, bx, by, bz)?.let { return it }
        val plain = Redstone.signalFrom(world, bx, by, bz, face)
        val target = world.state.getBlockState(bx, by, bz) ?: return plain
        // parity: a comparator reads a container one block further when a solid block is in the way.
        if (plain < Redstone.MAX_SIGNAL &&
            org.basaltmc.server.registry.BlockSupport
                .conductsRedstone(target)
        ) {
            Containers.analogSignal(world, bx + face.stepX, by + face.stepY, bz + face.stepZ)?.let { return it }
        }
        return plain
    }

    /** parity: `DiodeBlock.getAlternateSignal`, the two sides a diode listens to. */
    private fun alternateSignal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        properties: Map<String, String>,
    ): Int {
        val facing = properties["facing"] ?: return 0
        val face = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return 0
        var best = 0
        for (side in listOf(clockwise(face), clockwise(clockwise(clockwise(face))))) {
            val neighbour = world.state.getBlockState(x + side.stepX, y + side.stepY, z + side.stepZ) ?: continue
            val path = pathOf(neighbour)
            // parity: `getControlInputSignal` with `sideInputDiodesOnly`: only a diode or dust on the
            // side counts, which is why a lever beside a comparator does nothing to it.
            val counts = path == "repeater" || path == "comparator" || Redstone.isWire(neighbour)
            if (!counts) continue
            best = maxOf(best, Redstone.signalOf(neighbour, side))
        }
        return best
    }

    /** parity: `DiodeBlock.getInputSignal`, which reads the block the repeater faces. */
    private fun repeaterInput(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        properties: Map<String, String>,
    ): Int {
        val facing = properties["facing"] ?: return 0
        val face = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return 0
        val target = world.state.getBlockState(x + face.stepX, y + face.stepY, z + face.stepZ) ?: return 0
        val signal = Redstone.signalFrom(world, x + face.stepX, y + face.stepY, z + face.stepZ, face)
        if (signal >= Redstone.MAX_SIGNAL) return signal
        if (!Redstone.isWire(target)) return signal
        return maxOf(signal, BlockStateDefinitions.propertiesOf(target)["power"]?.toIntOrNull() ?: 0)
    }

    /**
     * parity: `RepeaterBlock.isLocked` over `DiodeBlock.getAlternateSignal`: a repeater pointed at
     * from either side by another powered repeater is held where it is.
     */
    private fun isLocked(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        properties: Map<String, String>,
    ): Boolean {
        val facing = properties["facing"] ?: return false
        val face = BlockFace.entries.firstOrNull { it.name.lowercase() == facing } ?: return false
        for (side in listOf(clockwise(face), clockwise(clockwise(clockwise(face))))) {
            val neighbour =
                world.state.getBlockState(x + side.stepX, y + side.stepY, z + side.stepZ) ?: continue
            val neighbourProperties = BlockStateDefinitions.propertiesOf(neighbour)
            val path = pathOf(neighbour)
            if (path != "repeater" && path != "comparator") continue
            if (neighbourProperties["powered"] != "true") continue
            // Only a diode pointing *at* this one locks it.
            if (neighbourProperties["facing"] == side.opposite.name.lowercase()) return true
        }
        return false
    }

    private fun clockwise(face: BlockFace): BlockFace =
        when (face) {
            BlockFace.NORTH -> BlockFace.EAST
            BlockFace.EAST -> BlockFace.SOUTH
            BlockFace.SOUTH -> BlockFace.WEST
            BlockFace.WEST -> BlockFace.NORTH
            else -> face
        }

    private fun turn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        property: String,
        value: String,
    ): BlockChange? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf(property to value), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    private fun pathOf(stateId: Int): String? = BlockStateDefinitions.blockOf(stateId)?.path

    private val COMPONENTS = setOf("redstone_torch", "redstone_wall_torch", "repeater", "comparator")
}
