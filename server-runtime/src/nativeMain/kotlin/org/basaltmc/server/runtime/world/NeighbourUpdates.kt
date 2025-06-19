package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks

/**
 * What a block change does to the blocks around it.
 *
 * parity: `Level.updateNeighborsAt` reaching each neighbour's `neighborChanged`, where a block that
 * can no longer stand breaks and drops.
 *
 * [BlockSupport] decides what breaks. A state whose rule this build does not model is left alone:
 * a block staying up when vanilla would drop it is far cheaper than one vanishing under a player.
 */
public object NeighbourUpdates {
    /**
     * How far one change is allowed to cascade. A tower of torches comes down in one go and this is
     * never reached; the bound is there so a rule that somehow contradicts itself cannot spin.
     */
    public const val MAX_CASCADE: Int = 1024

    /**
     * Breaks whatever [x], [y], [z] stopped holding up, and whatever that in turn stopped holding.
     *
     * [onBroken] is given each block as it goes, with the state it had, so the caller can drop its
     * loot and tell the clients: this knows about neither.
     */
    public fun afterChange(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        onReshaped: (stateId: Int, x: Int, y: Int, z: Int) -> Unit = { _, _, _, _ -> },
        onBroken: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ) {
        // The block that just arrived reaches out to what was already there, which is the other
        // half of `updateShape`: a fence put down beside a fence joins it, not only the other way.
        val bubbles = mutableListOf<BlockChange>()
        reshape(world, x, y, z, bubbles)?.let { onReshaped(it, x, y, z) }
        for (change in bubbles) onReshaped(change.stateId, change.x, change.y, change.z)
        // parity: `FallingBlock.onPlace` and its `updateShape`, which both do nothing but ask to be
        // looked at again in two ticks. The block that just arrived counts: sand put down in mid
        // air used to sit there for good, because only its neighbours were ever asked.
        FallingBlocks.scheduleIfFalling(world, x, y, z)
        FallingBlocks.scheduleIfFalling(world, x, y + 1, z)
        val pending = ArrayDeque<Int>(INITIAL_QUEUE)
        push(pending, x, y, z)
        var examined = 0
        while (pending.isNotEmpty() && examined < MAX_CASCADE) {
            val px = pending.removeFirst()
            val py = pending.removeFirst()
            val pz = pending.removeFirst()
            examined += DIRECTIONS
            visit(world, pending, px, py - 1, pz, onReshaped, onBroken)
            visit(world, pending, px, py + 1, pz, onReshaped, onBroken)
            visit(world, pending, px, py, pz - 1, onReshaped, onBroken)
            visit(world, pending, px, py, pz + 1, onReshaped, onBroken)
            visit(world, pending, px - 1, py, pz, onReshaped, onBroken)
            visit(world, pending, px + 1, py, pz, onReshaped, onBroken)
        }
    }

    /** Reshapes the block there, breaks it if nothing holds it, and follows on from what it held. */
    private fun visit(
        world: LoadedWorld,
        pending: ArrayDeque<Int>,
        x: Int,
        y: Int,
        z: Int,
        onReshaped: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
        onBroken: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ) {
        FallingBlocks.scheduleIfFalling(world, x, y, z)
        var changed = false
        val bubbles = mutableListOf<BlockChange>()
        reshape(world, x, y, z, bubbles)?.let {
            onReshaped(it, x, y, z)
            changed = true
        }
        for (change in bubbles) {
            onReshaped(change.stateId, change.x, change.y, change.z)
            changed = true
        }
        if (breakIfUnsupported(world, x, y, z, onBroken) || changed) push(pending, x, y, z)
    }

    /**
     * Joins a fence, a gate or a pane to what is now beside it, answering the new state if it moved.
     *
     * parity: `CrossCollisionBlock.updateShape`, which is the half of a neighbour update that
     * changes how a block looks rather than whether it stays.
     */
    public fun reshape(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        bubbles: MutableList<BlockChange> = mutableListOf(),
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        // parity: the scheduled ticks that change a block into something else rather than break it.
        // Vanilla defers these by a tick; the outcome is the same and this is where it is known.
        ScheduledBlocks.coveredSoil(world, x, y, z)?.let { return it }
        ScheduledBlocks.coral(world, x, y, z)?.let { return it }
        ScheduledBlocks.scaffolding(world, x, y, z)?.let { return it }
        // A column is more than one block, so it reports through the caller rather than as one state.
        ScheduledBlocks.bubbleColumn(world, x, y, z, bubbles)
        Reshapes.of(world, x, y, z, stateId)?.let { return it }
        if (BlockConnections.isLeaves(stateId)) return retreeLeaves(world, x, y, z, stateId)
        if (BlockConnections.isStairs(stateId)) return turnStairs(world, x, y, z, stateId)
        if (BlockConnections.isWall(stateId)) return rebuildWall(world, x, y, z, stateId)
        if (!BlockConnections.connects(stateId)) return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val wanted = HashMap<String, String>(4)
        for (face in HORIZONTAL) {
            val neighbour =
                when (face) {
                    BlockFace.NORTH -> stateAt(world, x, y, z - 1)
                    BlockFace.SOUTH -> stateAt(world, x, y, z + 1)
                    BlockFace.WEST -> stateAt(world, x - 1, y, z)
                    else -> stateAt(world, x + 1, y, z)
                }
            wanted[face.name.lowercase()] = BlockConnections.connectsTo(stateId, neighbour, face).toString()
        }
        val reshaped = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        if (reshaped == stateId) return null
        if (!world.state.setBlockState(x, y, z, reshaped)) return null
        return reshaped
    }

    /**
     * How far these leaves are from a trunk, recounted from their neighbours.
     *
     * parity: `LeavesBlock.updateDistance`: a log counts as zero, any other leaves as their own
     * distance, and anything else as seven. At seven the leaves are decaying, and a random tick is
     * what actually drops them.
     */
    private fun retreeLeaves(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val distance = leafDistance(world, x, y, z)
        val reshaped =
            BlockStateDefinitions.stateIdOf(block, mapOf("distance" to distance.toString()), stateId) ?: return null
        if (reshaped == stateId) return null
        if (!world.state.setBlockState(x, y, z, reshaped)) return null
        return reshaped
    }

    /** parity: `WallBlock.updateShape`, sides and post together. */
    private fun rebuildWall(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val wanted =
            Walls.shapeOf(
                stateId,
                north = stateAt(world, x, y, z - 1),
                south = stateAt(world, x, y, z + 1),
                west = stateAt(world, x - 1, y, z),
                east = stateAt(world, x + 1, y, z),
                above = stateAt(world, x, y + 1, z),
            ) ?: return null
        val rebuilt = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        if (rebuilt == stateId) return null
        if (!world.state.setBlockState(x, y, z, rebuilt)) return null
        return rebuilt
    }

    /** parity: `StairBlock.updateShape`, which is what turns a staircase's corners. */
    private fun turnStairs(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val shape =
            BlockConnections.stairShape(
                stateId,
                north = stateAt(world, x, y, z - 1),
                south = stateAt(world, x, y, z + 1),
                west = stateAt(world, x - 1, y, z),
                east = stateAt(world, x + 1, y, z),
            ) ?: return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("shape" to shape), stateId) ?: return null
        if (turned == stateId) return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return turned
    }

    /**
     * How far these leaves are from a trunk, walked out through the leaves themselves.
     *
     * parity: the answer `LeavesBlock.updateDistance` converges on, reached differently. Vanilla
     * reads its neighbours' stored `distance`, which works because worldgen writes it and a
     * scheduled tick keeps it right. Worldgen here writes leaves in their default state, whose
     * distance is seven, so reading the neighbours would call every canopy in the world dead. This
     * walks out to the trunk instead, which is right whatever the stored value says.
     */
    public fun leafDistance(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val visited = HashSet<Long>()
        var frontier = mutableListOf(packed(x, y, z))
        visited += frontier[0]
        for (step in 1..DECAY_DISTANCE) {
            val next = mutableListOf<Long>()
            for (position in frontier) {
                val px = unpackX(position)
                val py = unpackY(position)
                val pz = unpackZ(position)
                for (face in BlockFace.entries) {
                    val nx = px + offsetX(face)
                    val ny = py + offsetY(face)
                    val nz = pz + offsetZ(face)
                    val neighbour = stateAt(world, nx, ny, nz)
                    if (BlockConnections.preventsLeafDecay(neighbour)) return step
                    if (step == DECAY_DISTANCE || !BlockConnections.isLeaves(neighbour)) continue
                    val key = packed(nx, ny, nz)
                    if (visited.add(key)) next += key
                }
            }
            if (next.isEmpty()) break
            frontier = next
        }
        return DECAY_DISTANCE
    }

    private fun offsetX(face: BlockFace): Int =
        if (face == BlockFace.WEST) {
            -1
        } else if (face == BlockFace.EAST) {
            1
        } else {
            0
        }

    private fun offsetY(face: BlockFace): Int =
        if (face == BlockFace.DOWN) {
            -1
        } else if (face == BlockFace.UP) {
            1
        } else {
            0
        }

    private fun offsetZ(face: BlockFace): Int =
        if (face == BlockFace.NORTH) {
            -1
        } else if (face == BlockFace.SOUTH) {
            1
        } else {
            0
        }

    private fun packed(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3FFFFFF shl 38) or (z.toLong() and 0x3FFFFFF shl 12) or (y.toLong() and 0xFFF)

    private fun unpackX(packed: Long): Int = (packed shr 38).toInt() shl 6 shr 6

    private fun unpackZ(packed: Long): Int = (packed shl 26 shr 38).toInt()

    private fun unpackY(packed: Long): Int = (packed shl 52 shr 52).toInt()

    /**
     * parity: `CactusBlock.canSurvive`, written out here because it is one of the rules a probe of
     * one neighbour at a time cannot express: it asks what must *not* be beside it.
     */
    private fun cactusSurvives(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        for (face in HORIZONTAL) {
            val beside =
                when (face) {
                    BlockFace.NORTH -> stateAt(world, x, y, z - 1)
                    BlockFace.SOUTH -> stateAt(world, x, y, z + 1)
                    BlockFace.WEST -> stateAt(world, x - 1, y, z)
                    else -> stateAt(world, x + 1, y, z)
                }
            if (BlockShapes.isSolidState(beside)) return false
        }
        val below = stateAt(world, x, y - 1, z)
        val supported = BlockStateDefinitions.blockOf(below)?.path == "cactus" || BlockConnections.supportsCactus(below)
        return supported && !BlockSupport.holdsWater(stateAt(world, x, y + 1, z))
    }

    /** parity: `LeavesBlock.getOptionalDistanceAt`, where a trunk is zero and anything unknown is seven. */
    private fun distanceOf(stateId: Int): Int {
        if (BlockConnections.preventsLeafDecay(stateId)) return 0
        return BlockStateDefinitions.propertiesOf(stateId)["distance"]?.toIntOrNull() ?: DECAY_DISTANCE
    }

    /** Whether the block at these coordinates would stay where it is. */
    public fun isSupported(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int = world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId,
    ): Boolean {
        val path = BlockStateDefinitions.blockOf(stateId)?.path
        if (path == "cactus") return cactusSurvives(world, x, y, z)
        // parity: `ScaffoldingBlock.canSurvive`, which is `getDistance(level, pos) < 7`: a number
        // worked out from the whole row, not something a single neighbour can answer. The probe
        // that built the support table asks one neighbour at a time, so it cannot see this one.
        if (path == "scaffolding") return !ScheduledBlocks.scaffoldingHasFallen(world, x, y, z)
        if (!BlockSupport.needsSupport(stateId) || !BlockSupport.isModelled(stateId)) return true
        val down = stateAt(world, x, y - 1, z)
        val up = stateAt(world, x, y + 1, z)
        val north = stateAt(world, x, y, z - 1)
        val south = stateAt(world, x, y, z + 1)
        val west = stateAt(world, x - 1, y, z)
        val east = stateAt(world, x + 1, y, z)
        return BlockSupport.canSurvive(stateId, down, up, north, south, west, east, wet = waterAbout(world, x, y, z))
    }

    private fun breakIfUnsupported(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        onBroken: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ): Boolean {
        if (y < world.minY || y > world.maxY) return false
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        if (stateId == Blocks.AIR.stateId) return false
        if (isSupported(world, x, y, z, stateId)) return false
        world.state.setBlockState(x, y, z, Blocks.AIR.stateId)
        onBroken(stateId, x, y, z)
        return true
    }

    /**
     * Whether there is water at this position, beside it, or beside the block under it.
     *
     * Sugar cane asks about the last of those and kelp about the first, and neither can be answered
     * from the block alone. Asking generously is deliberate: it can only keep a block standing.
     */
    private fun waterAbout(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (BlockSupport.holdsWater(stateAt(world, x, y, z))) return true
        for (level in intArrayOf(y, y - 1)) {
            if (BlockSupport.holdsWater(stateAt(world, x - 1, level, z))) return true
            if (BlockSupport.holdsWater(stateAt(world, x + 1, level, z))) return true
            if (BlockSupport.holdsWater(stateAt(world, x, level, z - 1))) return true
            if (BlockSupport.holdsWater(stateAt(world, x, level, z + 1))) return true
        }
        return false
    }

    private fun stateAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId

    private fun push(
        queue: ArrayDeque<Int>,
        x: Int,
        y: Int,
        z: Int,
    ) {
        queue.addLast(x)
        queue.addLast(y)
        queue.addLast(z)
    }

    private val HORIZONTAL = listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST, BlockFace.EAST)

    /** parity: `LeavesBlock.DECAY_DISTANCE`. */
    private const val DECAY_DISTANCE = 7

    private const val INITIAL_QUEUE = 24
    private const val DIRECTIONS = 6
}
