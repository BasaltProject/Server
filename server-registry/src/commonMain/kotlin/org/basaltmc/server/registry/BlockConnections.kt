package org.basaltmc.server.registry

/**
 * Which blocks reach out to what is beside them: fences, gates, panes and bars.
 *
 * parity: `FenceBlock.connectsTo` and `IronBarsBlock.attachsTo`. What each block *is* comes from
 * the jar (a `FenceBlock`, the `#minecraft:fences` tag and the rest are in the generated table), so
 * this is the rule and not the data.
 *
 * Walls also connect and are not here: their sides are none, low or tall depending on what stands
 * above them, which is a different question from this one.
 */
public object BlockConnections {
    private val families: Map<String, ByteArray> = BLOCK_FAMILIES.associate { it.first to decode(it.second) }

    /** Whether this block connects at all, and so has north/south/east/west to keep up to date. */
    public fun connects(stateId: Int): Boolean = isFence(stateId) || isPanes(stateId)

    public fun isFence(stateId: Int): Boolean = inFamily("fence", stateId)

    public fun isPanes(stateId: Int): Boolean = inFamily("bars", stateId)

    public fun isWall(stateId: Int): Boolean = inFamily("wall", stateId)

    public fun isLeaves(stateId: Int): Boolean = inFamily("leaves", stateId)

    /** parity: `#minecraft:prevents_nearby_leaf_decay`, which is what keeps a canopy on a trunk. */
    public fun preventsLeafDecay(stateId: Int): Boolean = inFamily("prevents_decay", stateId)

    /** parity: `#minecraft:maintains_farmland`, the crops that keep tilled soil from reverting. */
    public fun maintainsFarmland(stateId: Int): Boolean = inFamily("maintains_farmland", stateId)

    /** parity: `#minecraft:supports_cactus`, which is sand and little else. */
    public fun supportsCactus(stateId: Int): Boolean = inFamily("supports_cactus", stateId)

    /** parity: `FallingBlock`, the sand, gravel, powders, anvils and dripstone that come down. */
    public fun isFalling(stateId: Int): Boolean = inFamily("falling", stateId)

    public fun isStairs(stateId: Int): Boolean = inFamily("stairs", stateId)

    /** parity: `#minecraft:soul_fire_base_blocks`, the ground that burns blue. */
    public fun isSoulFireBase(stateId: Int): Boolean = inFamily("soul_fire_base", stateId)

    /**
     * The corner a flight of stairs turns, from the stairs in front of and behind it.
     *
     * parity: `StairBlock.getStairsShape`. Answers the `shape` value, or null when the block is not
     * stairs at all. Neighbours are given as the states around it, clockwise from north.
     */
    public fun stairShape(
        stateId: Int,
        north: Int,
        south: Int,
        west: Int,
        east: Int,
    ): String? {
        if (!isStairs(stateId)) return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = properties["facing"] ?: return null
        val half = properties["half"] ?: return null

        fun toward(direction: String) =
            when (direction) {
                "north" -> north
                "south" -> south
                "west" -> west
                else -> east
            }

        val behind = toward(facing)
        if (isStairs(behind)) {
            val behindProperties = BlockStateDefinitions.propertiesOf(behind)
            val behindFacing = behindProperties["facing"]
            if (behindProperties["half"] == half &&
                behindFacing != null &&
                !sameAxis(behindFacing, facing) &&
                canTakeShape(facing, half, toward(opposite(behindFacing)))
            ) {
                return if (behindFacing == counterClockwise(facing)) "outer_left" else "outer_right"
            }
        }
        val front = toward(opposite(facing))
        if (isStairs(front)) {
            val frontProperties = BlockStateDefinitions.propertiesOf(front)
            val frontFacing = frontProperties["facing"]
            if (frontProperties["half"] == half &&
                frontFacing != null &&
                !sameAxis(frontFacing, facing) &&
                canTakeShape(facing, half, toward(frontFacing))
            ) {
                return if (frontFacing == counterClockwise(facing)) "inner_left" else "inner_right"
            }
        }
        return "straight"
    }

    /** parity: `StairBlock.canTakeShape`: stairs pointing the same way block the corner. */
    private fun canTakeShape(
        facing: String,
        half: String,
        neighbour: Int,
    ): Boolean {
        if (!isStairs(neighbour)) return true
        val properties = BlockStateDefinitions.propertiesOf(neighbour)
        return properties["facing"] != facing || properties["half"] != half
    }

    private fun sameAxis(
        first: String,
        second: String,
    ): Boolean = (first == "north" || first == "south") == (second == "north" || second == "south")

    private fun opposite(facing: String): String =
        when (facing) {
            "north" -> "south"
            "south" -> "north"
            "west" -> "east"
            else -> "west"
        }

    private fun counterClockwise(facing: String): String =
        when (facing) {
            "north" -> "west"
            "west" -> "south"
            "south" -> "east"
            else -> "north"
        }

    /**
     * Whether [stateId] reaches towards its neighbour [neighbourStateId], which lies in [face].
     *
     * parity: the two rules are nearly the same, and both start with a sturdy face on the
     * neighbour, which is why a fence grabs a wall of stone and not a wall of leaves.
     */
    public fun connectsTo(
        stateId: Int,
        neighbourStateId: Int,
        face: BlockFace,
    ): Boolean {
        val sturdy =
            !inFamily("exception", neighbourStateId) &&
                BlockShapes.isFaceSturdy(neighbourStateId, face.opposite)
        return when {
            isFence(stateId) ->
                sturdy || sameFence(stateId, neighbourStateId) || gateFaces(neighbourStateId, face)
            isPanes(stateId) ->
                sturdy || isPanes(neighbourStateId) || inFamily("walls_tag", neighbourStateId)
            else -> false
        }
    }

    /**
     * parity: `FenceBlock.isSameFence`, which is not "another fence": a wooden fence joins wooden
     * fences and a nether brick fence joins nether brick ones, and the two ignore each other.
     */
    private fun sameFence(
        stateId: Int,
        neighbourStateId: Int,
    ): Boolean =
        inFamily("fences_tag", neighbourStateId) &&
            inFamily("wooden_fences_tag", neighbourStateId) == inFamily("wooden_fences_tag", stateId)

    /** parity: `FenceGateBlock.connectsToDirection`: a gate joins the fences along its own line. */
    private fun gateFaces(
        stateId: Int,
        face: BlockFace,
    ): Boolean {
        if (!inFamily("gate", stateId)) return false
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return false
        val gateAxis = if (facing == "north" || facing == "south") AXIS_Z else AXIS_X
        // The gate stands across the fence line, so it joins the axis its facing turns into.
        return gateAxis != face.axis
    }

    private fun inFamily(
        family: String,
        stateId: Int,
    ): Boolean {
        val set = families[family] ?: return false
        val blockId = BlockSupport.blockRegistryIdOf(stateId)
        if (blockId < 0) return false
        val index = blockId / 8
        return index in set.indices && (set[index].toInt() shr (blockId % 8)) and 1 != 0
    }

    private fun decode(text: String): ByteArray {
        val trimmed = text.trimEnd('=')
        val bytes = ByteArray(trimmed.length * 6 / 8)
        var buffer = 0
        var bits = 0
        var written = 0
        for (character in trimmed) {
            buffer = (buffer shl 6) or BASE64.indexOf(character)
            bits += 6
            if (bits >= 8) {
                bits -= 8
                bytes[written++] = ((buffer shr bits) and 0xFF).toByte()
            }
        }
        return bytes
    }

    private const val AXIS_X = 0
    private const val AXIS_Z = 2
    private const val BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
}
