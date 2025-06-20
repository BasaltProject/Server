package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import kotlin.random.Random

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
        // parity: `SnifferEggBlock.onPlace`, which asks for the first of its three turns as soon as
        // the egg is down. Re-scheduling keeps a delay already counting, so a neighbour moving
        // nearby does not put the egg back to the start.
        ScheduledBlocks.scheduleHatch(world, x, y, z, Random.nextLong())
        // parity: the neighbour update every redstone component listens to: the dust settles at
        // once and the components book their own turn, which is what their delay is.
        for (change in Redstone.settle(world, x, y, z)) onReshaped(change.stateId, change.x, change.y, change.z)
        RedstoneComponents.scheduleAround(world, x, y, z)
        // parity: `ObserverBlock.neighborChanged`, which fires only for the block it is watching.
        RedstoneComponents.watchFor(world, x, y, z)
        // parity: every `neighborChanged` that reads the power: doors, lamps, bulbs and the rest.
        for (change in RedstoneReactors.update(world, x, y, z)) onReshaped(change.stateId, change.x, change.y, change.z)
        // parity: `DispenserBlock.neighborChanged`, which remembers it was triggered and books a turn.
        for (face in BlockFace.entries) {
            Dispensers
                .onNeighbourChange(world, x + face.stepX, y + face.stepY, z + face.stepZ)
                ?.let { onReshaped(it.stateId, it.x, it.y, it.z) }
        }
        Dispensers.onNeighbourChange(world, x, y, z)?.let { onReshaped(it.stateId, it.x, it.y, it.z) }
        // parity: `CrafterBlock.neighborChanged`, the same rising edge with its own four ticks.
        Crafters.onNeighbourChange(world, x, y, z)?.let { onReshaped(it.stateId, it.x, it.y, it.z) }
        for (face in BlockFace.entries) {
            Crafters
                .onNeighbourChange(world, x + face.stepX, y + face.stepY, z + face.stepZ)
                ?.let { onReshaped(it.stateId, it.x, it.y, it.z) }
        }
        // parity: `PistonBaseBlock.checkIfExtend`, which books the move rather than making it now.
        Pistons.trigger(world, x, y, z)
        for (face in BlockFace.entries) {
            Pistons.trigger(world, x + face.stepX, y + face.stepY, z + face.stepZ)
        }
        // parity: `SpongeBlock.onPlace`, which drinks before anything else gets a turn.
        Sponges.absorb(world, x, y, z, onReshaped)
        Sponges.dryIfEvaporating(world, x, y, z, onReshaped)
        // parity: `FrogspawnBlock.onPlace`, the same idea on a longer and rougher clock.
        ScheduledBlocks.scheduleFrogspawn(world, x, y, z, Random.nextLong())
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
     * How far these leaves are from a trunk.
     *
     * parity: `LeavesBlock.updateDistance`, six lookups and no more: a trunk counts as zero and a
     * neighbouring leaf as whatever it says it is. That works because the stored value is right to
     * begin with, which is [org.basaltmc.server.world.LeafDistances]' job when the chunk arrives.
     */
    public fun leafDistance(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        var nearest = DECAY_DISTANCE
        for (face in BlockFace.entries) {
            val neighbour = stateAt(world, x + offsetX(face), y + offsetY(face), z + offsetZ(face))
            nearest = minOf(nearest, distanceOf(neighbour) + 1)
            if (nearest == 1) return 1
        }
        return nearest
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

    private fun isFullSnow(stateId: Int): Boolean {
        if (BlockStateDefinitions.blockOf(stateId)?.path != "snow") return false
        return BlockStateDefinitions.propertiesOf(stateId)["layers"] == "8"
    }

    private fun speleothemSurvives(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        path: String,
    ): Boolean {
        val down = BlockStateDefinitions.propertiesOf(stateId)["vertical_direction"] != "up"
        val behind = stateAt(world, x, if (down) y + 1 else y - 1, z)
        val tip = if (down) BlockFace.DOWN else BlockFace.UP
        if (BlockShapes.isFaceSturdy(behind, tip)) return true
        if (BlockStateDefinitions.blockOf(behind)?.path != path) return false
        val theirs = BlockStateDefinitions.propertiesOf(behind)["vertical_direction"] != "up"
        return theirs == down
    }

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

    /** parity: `PitcherCropBlock.canSurvive`, where only the lower half asks about the light. */
    private fun isLowerHalf(stateId: Int): Boolean =
        BlockStateDefinitions.propertiesOf(stateId)["half"].let { it == null || it == "lower" }

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
        // parity: `SpeleothemBlock.isValidSpeleothemPlacement`, which hangs a spike off a sturdy
        // face *or* off another spike pointing the same way. The probe that built the support table
        // tries each block in its default state, and the default spike points up, so a hanging
        // column looked to it like a spike held up by nothing.
        if (path != null && path in SPELEOTHEMS) return speleothemSurvives(world, x, y, z, stateId, path)
        // parity: `SnowLayerBlock.canSurvive`'s last clause, snow standing on a full stack of snow.
        // The probe tries each block in its default state, and a default snow layer is one deep.
        if (path == "snow" && isFullSnow(stateAt(world, x, y - 1, z))) return true
        // parity: `CropBlock.canSurvive`, which asks for light eight on top of its soil. The probe
        // that built the support table answers one neighbour at a time and cannot see the sky, so
        // it was told the light is fifteen and the light half is checked here instead.
        if (path in LIGHT_HUNGRY_CROPS && isLowerHalf(stateId)) {
            if (WorldLight.rawBrightness(world, x, y, z) < CROP_SURVIVAL_LIGHT) return false
        }
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

    /** parity: `SpeleothemBlock`'s two blocks in 26.2. */
    private val SPELEOTHEMS = setOf("pointed_dripstone", "sulfur_spike")

    /** parity: `LeavesBlock.DECAY_DISTANCE`. */
    private const val DECAY_DISTANCE = 7

    /** parity: `CropBlock.hasSufficientLight`, which is eight and not the nine growing needs. */
    private const val CROP_SURVIVAL_LIGHT = 8

    /** The crops whose `canSurvive` is `CropBlock`'s: soil underneath *and* light on top. */
    private val LIGHT_HUNGRY_CROPS =
        setOf("wheat", "carrots", "potatoes", "beetroots", "torchflower_crop", "pitcher_crop")

    private const val INITIAL_QUEUE = 24
    private const val DIRECTIONS = 6
}
