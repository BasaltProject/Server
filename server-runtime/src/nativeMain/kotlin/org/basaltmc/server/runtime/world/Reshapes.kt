package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * The blocks whose `updateShape` changes what they are rather than whether they stay.
 *
 * parity: of the 104 classes that override `updateShape`, 87 only ask whether they can survive or
 * schedule a fluid tick, and both of those already happen elsewhere. These are the rest: a gate
 * that sinks into a wall, a chest that pairs up, a bed that loses its other half.
 *
 * Fences, panes, walls, stairs and leaves are in [BlockConnections] and [NeighbourUpdates], where
 * they went first; redstone wire, repeaters and tripwire are §6.2.
 */
public object Reshapes {
    /** Answers the state this block should now be, or null when it is none of these. */
    public fun of(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val path = block.path
        return when {
            path.endsWith("_fence_gate") -> gate(world, x, y, z, stateId, block)
            path.endsWith("_bed") -> bed(world, x, y, z, stateId, block)
            path.endsWith("_door") -> door(world, x, y, z, stateId, block)
            path == "chest" || path == "trapped_chest" -> chest(world, x, y, z, stateId, block)
            path == "chorus_plant" -> chorusPlant(world, x, y, z, stateId, block)
            path == "campfire" || path == "soul_campfire" -> campfire(world, x, y, z, stateId, block)
            path == "pale_hanging_moss" -> hangingMoss(world, x, y, z, stateId, block)
            path == "pointed_dripstone" -> dripstoneThickness(world, x, y, z, stateId, block)
            path.endsWith("mushroom_block") || path == "mushroom_stem" -> hugeMushroom(world, x, y, z, stateId, block)
            path == "bell" -> bell(world, x, y, z, stateId, block)
            else -> null
        }
    }

    /** parity: `FenceGateBlock.updateShape`, which sinks a gate into a wall beside it. */
    private fun gate(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return null
        // The gate stands across its facing, so the wall it sinks into is on the other axis.
        val sides =
            if (facing == "north" || facing == "south") {
                listOf(BlockFace.WEST, BlockFace.EAST)
            } else {
                listOf(BlockFace.NORTH, BlockFace.SOUTH)
            }
        val inWall =
            sides.any { face ->
                BlockConnections.isWall(stateAt(world, x + offsetX(face), y, z + offsetZ(face)))
            }
        return changed(world, x, y, z, stateId, block, "in_wall", inWall.toString())
    }

    /**
     * parity: `BedBlock.updateShape`: a half whose other half is gone goes with it, and one that
     * still has it copies whether it is occupied.
     */
    private fun bed(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = properties["facing"] ?: return null
        val part = properties["part"] ?: return null
        val towards = if (part == "foot") facing else opposite(facing)
        val face = faceOf(towards)
        val other = stateAt(world, x + offsetX(face), y, z + offsetZ(face))
        val otherProperties = BlockStateDefinitions.propertiesOf(other)
        if (BlockStateDefinitions.blockOf(other) != block || otherProperties["part"] == part) {
            if (!world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) return null
            return Blocks.AIR.stateId
        }
        val occupied = otherProperties["occupied"] ?: return null
        return changed(world, x, y, z, stateId, block, "occupied", occupied)
    }

    /**
     * parity: `DoorBlock.updateShape`: each half copies the other's whole state but for its own
     * half, which is what keeps a door open on both halves and closes it on both.
     */
    private fun door(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val half = BlockStateDefinitions.propertiesOf(stateId)["half"] ?: return null
        val otherY = if (half == "lower") y + 1 else y - 1
        val other = stateAt(world, x, otherY, z)
        val otherProperties = BlockStateDefinitions.propertiesOf(other)
        if (BlockStateDefinitions.blockOf(other) != block || otherProperties["half"] == half) {
            if (!world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) return null
            return Blocks.AIR.stateId
        }
        val wanted = BlockStateDefinitions.stateIdOf(block, mapOf("half" to half), other) ?: return null
        return apply(world, x, y, z, stateId, wanted)
    }

    /**
     * parity: `ChestBlock.updateShape`, which is only half of the pairing: it follows a neighbour
     * that has already claimed the side facing it, and it goes back to single when the chest it was
     * paired with is gone. Two chests becoming a double one is decided when the second is placed.
     */
    private fun chest(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = properties["facing"] ?: return null
        val type = properties["type"] ?: return null
        if (type == "single") {
            for ((side, mine) in listOf(counterClockwise(facing) to "right", clockwise(facing) to "left")) {
                val face = faceOf(side)
                val neighbour = stateAt(world, x + offsetX(face), y, z + offsetZ(face))
                if (BlockStateDefinitions.blockOf(neighbour) != block) continue
                val theirs = BlockStateDefinitions.propertiesOf(neighbour)
                if (theirs["facing"] != facing) continue
                if (theirs["type"] != otherHalf(mine)) continue
                return changed(world, x, y, z, stateId, block, "type", mine)
            }
            return null
        }
        val partnerSide = if (type == "left") clockwise(facing) else counterClockwise(facing)
        val face = faceOf(partnerSide)
        val partner = stateAt(world, x + offsetX(face), y, z + offsetZ(face))
        val theirs = BlockStateDefinitions.propertiesOf(partner)
        val stillThere =
            BlockStateDefinitions.blockOf(partner) == block &&
                theirs["facing"] == facing &&
                theirs["type"] == otherHalf(type)
        if (stillThere) return null
        return changed(world, x, y, z, stateId, block, "type", "single")
    }

    /** The other half of a double chest. */
    private fun otherHalf(type: String): String = if (type == "left") "right" else "left"

    /** parity: `ChorusPlantBlock.updateShape`, six faces of it. */
    private fun chorusPlant(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val wanted = HashMap<String, String>(6)
        for (face in BlockFace.entries) {
            val neighbour = stateAt(world, x + offsetX(face), y + offsetY(face), z + offsetZ(face))
            val path = BlockStateDefinitions.blockOf(neighbour)?.path
            val connects =
                path == "chorus_plant" ||
                    path == "chorus_flower" ||
                    (face == BlockFace.DOWN && path == "end_stone")
            wanted[face.name.lowercase()] = connects.toString()
        }
        val reshaped = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        return apply(world, x, y, z, stateId, reshaped)
    }

    /** parity: `CampfireBlock.updateShape`: a hay bale under it turns it into a signal fire. */
    private fun campfire(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val below = BlockStateDefinitions.blockOf(stateAt(world, x, y - 1, z))?.path
        return changed(world, x, y, z, stateId, block, "signal_fire", (below == "hay_block").toString())
    }

    /** parity: `HangingMossBlock.updateShape`, whose tip is the strand with nothing under it. */
    private fun hangingMoss(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val below = BlockStateDefinitions.blockOf(stateAt(world, x, y - 1, z))
        return changed(world, x, y, z, stateId, block, "tip", (below != block).toString())
    }

    /**
     * parity: `SpeleothemBlock.calculateSpeleothemThickness`, the four shapes a dripstone takes
     * depending on what is above and below it.
     */
    private fun dripstoneThickness(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val down = properties["vertical_direction"] == "down"
        val behind = if (down) stateAt(world, x, y + 1, z) else stateAt(world, x, y - 1, z)
        val ahead = if (down) stateAt(world, x, y - 1, z) else stateAt(world, x, y + 1, z)
        val sameBehind = isDripstone(behind, down)
        val sameAhead = isDripstone(ahead, down)
        val thickness =
            when {
                !sameAhead && !sameBehind -> "tip"
                !sameAhead -> "frustum"
                !sameBehind -> "base"
                else -> "middle"
            }
        return changed(world, x, y, z, stateId, block, "thickness", thickness)
    }

    private fun isDripstone(
        stateId: Int,
        down: Boolean,
    ): Boolean {
        if (BlockStateDefinitions.blockOf(stateId)?.path != "pointed_dripstone") return false
        val theirs = BlockStateDefinitions.propertiesOf(stateId)["vertical_direction"]
        return theirs == if (down) "down" else "up"
    }

    /** parity: `HugeMushroomBlock.updateShape`: a face touching another cap stops being drawn. */
    private fun hugeMushroom(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val wanted = HashMap<String, String>(6)
        for (face in BlockFace.entries) {
            val neighbour = stateAt(world, x + offsetX(face), y + offsetY(face), z + offsetZ(face))
            wanted[face.name.lowercase()] = (BlockStateDefinitions.blockOf(neighbour) != block).toString()
        }
        val reshaped = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        return apply(world, x, y, z, stateId, reshaped)
    }

    /** parity: `BellBlock.updateShape`, which pairs a wall bell with the wall on its other side. */
    private fun bell(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val attachment = properties["attachment"] ?: return null
        if (attachment != "single_wall" && attachment != "double_wall") return null
        val facing = properties["facing"] ?: return null
        val front = faceOf(facing)
        val back = faceOf(opposite(facing))
        val frontSturdy =
            BlockShapes.isFaceSturdy(stateAt(world, x + offsetX(front), y, z + offsetZ(front)), back)
        val backSturdy =
            BlockShapes.isFaceSturdy(stateAt(world, x + offsetX(back), y, z + offsetZ(back)), front)
        val wanted = if (frontSturdy && backSturdy) "double_wall" else "single_wall"
        return changed(world, x, y, z, stateId, block, "attachment", wanted)
    }

    private fun changed(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
        property: String,
        value: String,
    ): Int? {
        val reshaped = BlockStateDefinitions.stateIdOf(block, mapOf(property to value), stateId) ?: return null
        return apply(world, x, y, z, stateId, reshaped)
    }

    private fun apply(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        reshaped: Int,
    ): Int? {
        if (reshaped == stateId) return null
        if (!world.state.setBlockState(x, y, z, reshaped)) return null
        return reshaped
    }

    private fun stateAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId

    private fun faceOf(facing: String): BlockFace =
        when (facing) {
            "north" -> BlockFace.NORTH
            "south" -> BlockFace.SOUTH
            "west" -> BlockFace.WEST
            else -> BlockFace.EAST
        }

    private fun opposite(facing: String): String =
        when (facing) {
            "north" -> "south"
            "south" -> "north"
            "west" -> "east"
            else -> "west"
        }

    private fun clockwise(facing: String): String =
        when (facing) {
            "north" -> "east"
            "east" -> "south"
            "south" -> "west"
            else -> "north"
        }

    private fun counterClockwise(facing: String): String = clockwise(clockwise(clockwise(facing)))

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
}
