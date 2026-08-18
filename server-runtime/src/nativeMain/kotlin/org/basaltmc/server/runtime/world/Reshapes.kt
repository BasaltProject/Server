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
 * Fences, panes, walls, stairs and leaves are in [BlockConnections] and [NeighbourUpdates], and
 * redstone wire and repeaters in [Redstone].
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
            path in Containers.CHESTS -> chest(world, x, y, z, stateId, block)
            path == "chorus_plant" -> chorusPlant(world, x, y, z, stateId, block)
            path == "campfire" || path == "soul_campfire" -> campfire(world, x, y, z, stateId, block)
            path == "pale_hanging_moss" -> hangingMoss(world, x, y, z, stateId, block)
            path == "pointed_dripstone" || path == "sulfur_spike" ->
                dripstoneThickness(world, x, y, z, stateId, block)
            path.endsWith("mushroom_block") || path == "mushroom_stem" -> hugeMushroom(world, x, y, z, stateId, block)
            path == "bell" -> bell(world, x, y, z, stateId, block)
            path.startsWith("attached_") && path.endsWith("_stem") -> detachedStem(world, x, y, z, stateId)
            path == "potent_sulfur" -> Geysers.validState(world, x, y, z, stateId)
            path == "pale_moss_carpet" -> mossCarpet(world, x, y, z, stateId, block)
            else -> null
        }
    }

    /**
     * parity: `AttachedStemBlock.updateShape`: pick the melon and the stem it fed goes back to
     * being a grown stem, pointing nowhere.
     */
    private fun detachedStem(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = properties["facing"] ?: return null
        val fruit = FRUIT_OF[BlockStateDefinitions.blockOf(stateId)?.path] ?: return null
        val offsetX =
            if (facing == "west") {
                -1
            } else if (facing == "east") {
                1
            } else {
                0
            }
        val offsetZ =
            if (facing == "north") {
                -1
            } else if (facing == "south") {
                1
            } else {
                0
            }
        val neighbour = world.state.getBlockState(x + offsetX, y, z + offsetZ) ?: return null
        if (BlockStateDefinitions.blockOf(neighbour)?.path == fruit) return null
        val detached =
            BlockStateDefinitions.stateIdOf(Identifier.vanilla(fruit + "_stem"), mapOf("age" to "7")) ?: return null
        return apply(world, x, y, z, stateId, detached)
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
     * parity: `SpeleothemBlock.calculateSpeleothemThickness`, whose answer depends on what the
     * piece in front already is and not only on whether there is one: a spike hanging off a tip is
     * a frustum, and the one against the ceiling is only the base while nothing is behind it.
     *
     * A tip meeting another tip head on merges, which is the pair that catches a drop of water.
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
        val spike = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        val down = properties["vertical_direction"] != "up"
        val inFront = stateAt(world, x, if (down) y - 1 else y + 1, z)
        val behind = stateAt(world, x, if (down) y + 1 else y - 1, z)
        val merging = properties["thickness"] == "tip_merge"
        val thickness =
            if (isSpeleothem(inFront, spike, pointsDown = !down)) {
                if (!merging && thicknessOf(inFront) != "tip_merge") "tip" else "tip_merge"
            } else if (!isSpeleothem(inFront, spike, pointsDown = down)) {
                "tip"
            } else if (thicknessOf(inFront) != "tip" && thicknessOf(inFront) != "tip_merge") {
                if (!isSpeleothem(behind, spike, pointsDown = down)) "base" else "middle"
            } else {
                "frustum"
            }
        return changed(world, x, y, z, stateId, block, "thickness", thickness)
    }

    private fun thicknessOf(stateId: Int): String? = BlockStateDefinitions.propertiesOf(stateId)["thickness"]

    private fun isSpeleothem(
        stateId: Int,
        spike: String,
        pointsDown: Boolean,
    ): Boolean {
        if (BlockStateDefinitions.blockOf(stateId)?.path != spike) return false
        return (BlockStateDefinitions.propertiesOf(stateId)["vertical_direction"] != "up") == pointsDown
    }

    /**
     * parity: `MossyCarpetBlock.getUpdatedState`: a side keeps standing while there is a wall to
     * hold it, grows tall under another carpet's side, and drops when the carpet below has none.
     * A carpet left with no sides at all and no base is gone.
     */
    private fun mossCarpet(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
    ): Int? {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val base = properties["bottom"] == "true"
        val above = stateAt(world, x, y + 1, z)
        val below = stateAt(world, x, y - 1, z)
        val wanted = HashMap<String, String>(4)
        var standing = base
        for (face in HORIZONTAL) {
            val name = face.name.lowercase()
            val neighbour = stateAt(world, x + offsetX(face), y + offsetY(face), z + offsetZ(face))
            var side = if (BlockShapes.isFaceSturdy(neighbour, face.opposite)) properties[name] ?: "none" else "none"
            if (side == "low") {
                if (isCarpetWith(above, name) && BlockStateDefinitions.propertiesOf(above)["bottom"] != "true") {
                    side = "tall"
                }
                if (!base && isCarpet(below) && BlockStateDefinitions.propertiesOf(below)[name] == "none") {
                    side = "none"
                }
            }
            if (side != "none") standing = true
            wanted[name] = side
        }
        if (!standing) return apply(world, x, y, z, stateId, Blocks.AIR.stateId)
        val reshaped = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        return apply(world, x, y, z, stateId, reshaped)
    }

    private fun isCarpet(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "pale_moss_carpet"

    private fun isCarpetWith(
        stateId: Int,
        face: String,
    ): Boolean = isCarpet(stateId) && BlockStateDefinitions.propertiesOf(stateId)[face] != "none"

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

    private val HORIZONTAL = listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST, BlockFace.EAST)

    /** parity: each `AttachedStemBlock`'s own `fruit`. */
    private val FRUIT_OF = mapOf("attached_melon_stem" to "melon", "attached_pumpkin_stem" to "pumpkin")
}
