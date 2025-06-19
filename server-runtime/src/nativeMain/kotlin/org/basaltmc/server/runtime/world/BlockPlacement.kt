package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.Direction
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.random.Random

/**
 * Which state a block goes down as: facing somewhere, on its side, upside down.
 *
 * parity: each block's own `getStateForPlacement`, told apart by the properties the block really
 * has plus the lists below for the ones that break their family's rule.
 *
 * Nothing here reads the world back, so no block refuses a spot it cannot hold on to.
 */
public object BlockPlacement {
    /** Six-way blocks that point at the face that was clicked rather than away from the player. */
    private val CLICKED_FACE_BLOCKS =
        setOf("lightning_rod", "end_rod", "shulker_box", "amethyst_cluster", "amethyst_bud")

    /** Horizontal blocks that face the same way as the player instead of back at them. */
    /**
     * The blocks whose `getStateForPlacement` uses `getHorizontalDirection()` as it comes, taken
     * from the jar rather than guessed: everything else takes its opposite, because a furnace or a
     * chest faces whoever put it down while a bed or a stair runs away from them.
     *
     * The suffixes cover the families: `_bed`, `_door`, `_stairs` and `_fence_gate`.
     */
    private val PLAYER_FACING_BLOCKS = setOf("campfire", "bell", "decorated_pot", "calibrated_sculk_sensor")

    private val PLAYER_FACING_SUFFIXES = listOf("_stairs", "_door", "_bed", "_fence_gate")

    /**
     * The state [block] should be placed in, or [defaultStateId] when nothing about it orients.
     *
     * [clickY] is where in the clicked block the cursor was, 0 at its floor and 1 at its ceiling.
     */
    public fun stateFor(
        block: Identifier,
        defaultStateId: Int,
        face: Direction,
        clickY: Float,
        yaw: Float,
        pitch: Float,
        replacing: Int,
        world: LoadedWorld? = null,
        x: Int = 0,
        y: Int = 0,
        z: Int = 0,
        clickX: Float = 0.5f,
        clickZ: Float = 0.5f,
    ): Int {
        val definition = BlockStateDefinitions.of(block) ?: return defaultStateId
        val path = block.path
        val properties = mutableMapOf<String, String>()

        if (definition.valuesOf("axis").size == 3) properties["axis"] = axisOf(face)

        // parity: `FaceAttachedHorizontalDirectionalBlock`, the buttons, levers and grindstones:
        // what they hang on comes from the face that was clicked, and only then which way they look.
        val attachment = definition.valuesOf("face")
        if (attachment.isNotEmpty()) {
            properties["face"] =
                when (face) {
                    Direction.UP -> "floor"
                    Direction.DOWN -> "ceiling"
                    else -> "wall"
                }
        }

        val facing = definition.valuesOf("facing")
        when {
            facing.size == 6 ->
                properties["facing"] =
                    if (isClickedFaceBlock(block)) name(face) else name(opposite(nearestLooking(yaw, pitch)))
            facing.size == 4 && path.contains("wall") && face != Direction.UP && face != Direction.DOWN ->
                properties["facing"] = name(face)
            facing.size == 4 && attachment.isNotEmpty() ->
                properties["facing"] =
                    if (properties["face"] == "wall") name(face) else name(playerDirection(yaw))
            // parity: `AnvilBlock`, which lands sideways to whoever put it down.
            facing.size == 4 && block.path.endsWith("anvil") ->
                properties["facing"] =
                    name(clockwise(playerDirection(yaw)))
            facing.size == 4 ->
                properties["facing"] =
                    if (isPlayerFacingBlock(block)) name(playerDirection(yaw)) else name(opposite(playerDirection(yaw)))
        }

        // parity: the same test in `StairBlock`, `SlabBlock` and `TrapDoorBlock`: the top half only
        // when the click was on a ceiling, or above the middle of a side.
        val upsideDown = face == Direction.DOWN || (face != Direction.UP && clickY > HALF)
        if (definition.valuesOf("half").contains("top")) properties["half"] = if (upsideDown) "top" else "bottom"
        val type = definition.valuesOf("type")
        if (type.contains("double")) {
            properties["type"] =
                when {
                    BlockStateDefinitions.blockOf(replacing) == block -> "double"
                    upsideDown -> "top"
                    else -> "bottom"
                }
        }

        // parity: every `SimpleWaterloggedBlock`'s `getStateForPlacement`, which is the same line in
        // all twenty-odd of them: the block keeps the water it was put into instead of eating it.
        if (definition.valuesOf("waterlogged").isNotEmpty()) {
            properties["waterlogged"] = isWater(replacing).toString()
        }

        // parity: `BaseRailBlock.getStateForPlacement`: a rail lies along the axis the player is
        // looking down, and the connection pass afterwards is what bends it towards its neighbours.
        if (definition.valuesOf("shape").contains("north_south")) {
            val looking = playerDirection(yaw)
            properties["shape"] =
                if (looking == Direction.EAST || looking == Direction.WEST) "east_west" else "north_south"
        }

        // parity: `LeavesBlock.getStateForPlacement`. Leaves a player puts down never decay, which
        // is the whole reason the property exists.
        if (definition.valuesOf("persistent").isNotEmpty()) properties["persistent"] = "true"

        // parity: sixteen segments of the yaw. `StandingSignBlock` and `BannerBlock` turn it around
        // so the face looks back at whoever placed it; `SkullBlock` does not, because a head looks
        // the same way the player does.
        if (definition.valuesOf("rotation").size == SEGMENTS) {
            val turned = if (path.endsWith("_head") || path.endsWith("_skull")) yaw else yaw + HALF_TURN.toFloat()
            properties["rotation"] = segmentOf(turned).toString()
        }

        // parity: `LanternBlock.getStateForPlacement`, which hangs one put against a ceiling.
        if (definition.valuesOf("hanging").isNotEmpty()) {
            properties["hanging"] = (face == Direction.DOWN).toString()
        }

        // parity: `CandleBlock`, `SeaPickleBlock` and `TurtleEggBlock`, which stack another one into
        // the block that is already there rather than refusing the placement.
        if (BlockStateDefinitions.blockOf(replacing) == block) {
            for (name in STACKING) {
                val values = definition.valuesOf(name)
                if (values.isEmpty()) continue
                val had = BlockStateDefinitions.propertiesOf(replacing)[name]?.toIntOrNull() ?: continue
                properties[name] = minOf(had + 1, values.size).toString()
            }
        }

        // parity: `DoorBlock.getHinge`, which counts the solid blocks either side and falls back to
        // which half of the block was clicked.
        if (world != null && definition.valuesOf("hinge").isNotEmpty()) {
            val towards = properties["facing"]?.let { fromName(it) } ?: playerDirection(yaw)
            properties["hinge"] = hingeOf(world, x, y, z, towards, clickX, clickZ)
        }

        // The blocks whose own `getStateForPlacement` does something the rules above cannot say.
        special(block, definition, properties, face, yaw, pitch, world, x, y, z)?.let { return it }

        if (properties.isEmpty()) return defaultStateId
        return BlockStateDefinitions.stateIdOf(block, properties, fromStateId = defaultStateId) ?: defaultStateId
    }

    /**
     * The per-block half of placement: what each `getStateForPlacement` sets that no general rule
     * covers. Answers a state id only when the block turns into a different one (bamboo), and
     * otherwise writes into [properties].
     */
    private fun special(
        block: Identifier,
        definition: BlockStateDefinitions.Definition,
        properties: MutableMap<String, String>,
        face: Direction,
        yaw: Float,
        pitch: Float,
        world: LoadedWorld?,
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val path = block.path
        // parity: `SnowyBlock.getStateForPlacement`, grass and mycelium put down under snow.
        if (definition.valuesOf("snowy").isNotEmpty() && world != null) {
            properties["snowy"] = isSnow(world.state.getBlockState(x, y + 1, z) ?: 0).toString()
        }
        // parity: `CampfireBlock.getStateForPlacement`: hay under it makes the tall smoke, and a
        // campfire placed in water goes down unlit.
        if (path.endsWith("campfire") && world != null) {
            val below = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y - 1, z) ?: 0)?.path
            properties["signal_fire"] = (below == "hay_block").toString()
            properties["lit"] = (properties["waterlogged"] != "true").toString()
        }
        // parity: `ScaffoldingBlock.getStateForPlacement`, which counts its distance as it lands.
        if (path == "scaffolding" && world != null) {
            val distance = scaffoldDistance(world, x, y, z)
            properties["distance"] = distance.toString()
            val below = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y - 1, z) ?: 0)?.path
            properties["bottom"] = (distance > 0 && below != "scaffolding").toString()
        }
        // parity: `BellBlock.getStateForPlacement`: hung, stood or bolted to one wall or two.
        if (path == "bell") {
            val onWall = face != Direction.UP && face != Direction.DOWN
            properties["attachment"] =
                when {
                    face == Direction.DOWN -> "ceiling"
                    !onWall -> "floor"
                    world != null && bothWalls(world, x, y, z, face) -> "double_wall"
                    else -> "single_wall"
                }
            if (onWall) properties["facing"] = name(opposite(face))
        }
        // parity: `MangrovePropaguleBlock`, which a player always plants grown.
        if (path == "mangrove_propagule") properties["age"] = "4"
        // parity: `GrowingPlantHeadBlock.getStateForPlacement`, a random age out of twenty-five.
        if (path in GROWING_HEADS) properties["age"] = Random.nextInt(GROWING_AGES).toString()
        // parity: `SpeleothemBlock.getStateForPlacement`: it points away from the face it was hung
        // on, and the reshape that follows works out how thick it is.
        if (definition.valuesOf("vertical_direction").isNotEmpty()) {
            properties["vertical_direction"] = if (face == Direction.DOWN) "down" else "up"
            properties["thickness"] = "tip"
        }
        // parity: `CrafterBlock.getStateForPlacement`, whose front is where the player is not.
        if (definition.valuesOf("orientation").isNotEmpty()) {
            val front = opposite(nearestLooking(yaw, pitch))
            val top =
                when (front) {
                    Direction.DOWN -> opposite(playerDirection(yaw))
                    Direction.UP -> playerDirection(yaw)
                    else -> Direction.UP
                }
            properties["orientation"] = "${name(front)}_${name(top)}"
        }
        // parity: `BambooStalkBlock.getStateForPlacement`, the one that answers a different block:
        // bamboo planted on the ground is a sapling, and on bamboo it copies how grown that is.
        if (path == "bamboo" && world != null) {
            val below = world.state.getBlockState(x, y - 1, z) ?: 0
            val belowPath = BlockStateDefinitions.blockOf(below)?.path
            val age =
                when (belowPath) {
                    "bamboo_sapling" -> 0
                    "bamboo" -> if ((BlockStateDefinitions.propertiesOf(below)["age"]?.toIntOrNull() ?: 0) > 0) 1 else 0
                    else -> {
                        val above = world.state.getBlockState(x, y + 1, z) ?: 0
                        if (BlockStateDefinitions.blockOf(above)?.path == "bamboo") {
                            BlockStateDefinitions.propertiesOf(above)["age"]?.toIntOrNull() ?: 0
                        } else {
                            return BlockStateDefinitions.stateIdOf(Identifier.vanilla("bamboo_sapling"), emptyMap())
                        }
                    }
                }
            properties["age"] = age.toString()
        }
        return null
    }

    private fun isSnow(stateId: Int): Boolean {
        val path = BlockStateDefinitions.blockOf(stateId)?.path
        return path == "snow" || path == "snow_block"
    }

    /** parity: `BellBlock`'s own test: a sturdy face on both sides bolts the bell to the pair. */
    private fun bothWalls(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        face: Direction,
    ): Boolean {
        val sides =
            if (face == Direction.WEST || face == Direction.EAST) {
                listOf(Direction.WEST to BlockFace.EAST, Direction.EAST to BlockFace.WEST)
            } else {
                listOf(Direction.NORTH to BlockFace.SOUTH, Direction.SOUTH to BlockFace.NORTH)
            }
        return sides.all { (side, towards) ->
            val neighbour = world.state.getBlockState(x + stepX(side), y, z + stepZ(side)) ?: 0
            BlockShapes.isFaceSturdy(neighbour, towards)
        }
    }

    /** parity: `ScaffoldingBlock.getDistance`, the same count its scheduled tick keeps. */
    private fun scaffoldDistance(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val below = world.state.getBlockState(x, y - 1, z) ?: 0
        if (BlockShapes.isFaceSturdy(below, BlockFace.UP)) return 0
        var nearest = SCAFFOLD_REACH
        if (BlockStateDefinitions.blockOf(below)?.path == "scaffolding") {
            nearest =
                minOf(nearest, BlockStateDefinitions.propertiesOf(below)["distance"]?.toIntOrNull() ?: SCAFFOLD_REACH)
        }
        for (side in listOf(Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST)) {
            val neighbour = world.state.getBlockState(x + stepX(side), y, z + stepZ(side)) ?: 0
            if (BlockStateDefinitions.blockOf(neighbour)?.path != "scaffolding") continue
            val theirs = BlockStateDefinitions.propertiesOf(neighbour)["distance"]?.toIntOrNull() ?: SCAFFOLD_REACH
            nearest = minOf(nearest, theirs + 1)
        }
        return minOf(nearest, SCAFFOLD_REACH)
    }

    /**
     * parity: `DoorBlock.getHinge`: a door beside another door, or with solid blocks on one side,
     * hangs away from them; with nothing to go by it hangs on the side that was clicked.
     */
    private fun hingeOf(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        towards: Direction,
        clickX: Float,
        clickZ: Float,
    ): String {
        val left = counterClockwise(towards)
        val right = clockwise(towards)
        val leftLower = world.state.getBlockState(x + stepX(left), y, z + stepZ(left)) ?: 0
        val leftUpper = world.state.getBlockState(x + stepX(left), y + 1, z + stepZ(left)) ?: 0
        val rightLower = world.state.getBlockState(x + stepX(right), y, z + stepZ(right)) ?: 0
        val rightUpper = world.state.getBlockState(x + stepX(right), y + 1, z + stepZ(right)) ?: 0
        val balance =
            (if (BlockShapes.isFullCube(leftLower)) -1 else 0) +
                (if (BlockShapes.isFullCube(leftUpper)) -1 else 0) +
                (if (BlockShapes.isFullCube(rightLower)) 1 else 0) +
                (if (BlockShapes.isFullCube(rightUpper)) 1 else 0)
        val doorLeft = isLowerDoor(leftLower)
        val doorRight = isLowerDoor(rightLower)
        if ((doorLeft && !doorRight) || balance > 0) return "right"
        if ((doorRight && !doorLeft) || balance < 0) return "left"
        val stepX = stepX(towards)
        val stepZ = stepZ(towards)
        val clicked =
            (stepX >= 0 || clickZ >= 0.5f) &&
                (stepX <= 0 || clickZ <= 0.5f) &&
                (stepZ >= 0 || clickX <= 0.5f) &&
                (stepZ <= 0 || clickX >= 0.5f)
        return if (clicked) "left" else "right"
    }

    private fun isLowerDoor(stateId: Int): Boolean {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        if (!path.endsWith("_door")) return false
        return BlockStateDefinitions.propertiesOf(stateId)["half"] == "lower"
    }

    /** parity: `SegmentedAnglePrecision`'s sixteen, which is what a sign, a banner and a skull use. */
    private fun segmentOf(degrees: Float): Int = ((degrees * SEGMENTS / FULL_TURN).roundToInt()) and (SEGMENTS - 1)

    private fun stepX(direction: Direction): Int =
        when (direction) {
            Direction.WEST -> -1
            Direction.EAST -> 1
            else -> 0
        }

    private fun stepZ(direction: Direction): Int =
        when (direction) {
            Direction.NORTH -> -1
            Direction.SOUTH -> 1
            else -> 0
        }

    /** parity: `Level.getFluidState`, which answers water for water and for anything logged with it. */
    private fun isWater(stateId: Int): Boolean {
        if (BlockStateDefinitions.blockOf(stateId)?.path == "water") return true
        return BlockStateDefinitions.propertiesOf(stateId)["waterlogged"] == "true"
    }

    /**
     * Whether the block would be placed inside somebody.
     *
     * parity: `BlockItem.canPlace` over `Level.isUnobstructed`. Touching is not overlapping, which
     * is why a block at your own feet still goes down, and an empty shape never obstructs anyone.
     */
    public fun isObstructed(
        stateId: Int,
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        boxes: List<DoubleArray>,
    ): Boolean {
        if (boxes.isEmpty() || BlockShapes.isEmptyShape(stateId)) return false
        for (box in BlockShapes.collisionBoxes(stateId)) {
            val minX = blockX + box[0] / BlockShapes.UNITS.toDouble()
            val minY = blockY + box[1] / BlockShapes.UNITS.toDouble()
            val minZ = blockZ + box[2] / BlockShapes.UNITS.toDouble()
            val maxX = blockX + box[3] / BlockShapes.UNITS.toDouble()
            val maxY = blockY + box[4] / BlockShapes.UNITS.toDouble()
            val maxZ = blockZ + box[5] / BlockShapes.UNITS.toDouble()
            for (other in boxes) {
                if (minX < other[3] &&
                    maxX > other[0] &&
                    minY < other[4] &&
                    maxY > other[1] &&
                    minZ < other[5] &&
                    maxZ > other[2]
                ) {
                    return true
                }
            }
        }
        return false
    }

    /** parity: `Player.getDimensions`, which is what the box around a player measures. */
    public fun playerBox(
        x: Double,
        y: Double,
        z: Double,
        sneaking: Boolean,
    ): DoubleArray {
        val half = PLAYER_WIDTH / 2
        return doubleArrayOf(
            x - half,
            y,
            z - half,
            x + half,
            y + if (sneaking) SNEAKING_HEIGHT else PLAYER_HEIGHT,
            z + half,
        )
    }

    private fun isClickedFaceBlock(block: Identifier): Boolean =
        CLICKED_FACE_BLOCKS.any { block.path == it || block.path.endsWith("_$it") }

    private fun isPlayerFacingBlock(block: Identifier): Boolean =
        PLAYER_FACING_SUFFIXES.any { block.path.endsWith(it) } ||
            PLAYER_FACING_BLOCKS.any { block.path == it || block.path.endsWith("_$it") }

    /** parity: `Direction.fromYRot`, the quarter turn the player's yaw falls in. */
    public fun playerDirection(yaw: Float): Direction = HORIZONTAL[floor(yaw / QUARTER_TURN + 0.5f).toInt() and 3]

    /** parity: `Direction.orderedByNearest`'s first entry: whichever face the player looks most along. */
    public fun nearestLooking(
        yaw: Float,
        pitch: Float,
    ): Direction {
        val yawRadians = yaw * PI / HALF_TURN
        val pitchRadians = pitch * PI / HALF_TURN
        val x = -sin(yawRadians) * cos(pitchRadians)
        val y = -sin(pitchRadians)
        val z = cos(yawRadians) * cos(pitchRadians)
        return when {
            abs(y) >= abs(x) && abs(y) >= abs(z) -> if (y > 0) Direction.UP else Direction.DOWN
            abs(x) >= abs(z) -> if (x > 0) Direction.EAST else Direction.WEST
            else -> if (z > 0) Direction.SOUTH else Direction.NORTH
        }
    }

    private fun clockwise(face: Direction): Direction =
        when (face) {
            Direction.NORTH -> Direction.EAST
            Direction.EAST -> Direction.SOUTH
            Direction.SOUTH -> Direction.WEST
            Direction.WEST -> Direction.NORTH
            else -> face
        }

    private fun counterClockwise(face: Direction): Direction = clockwise(clockwise(clockwise(face)))

    private fun fromName(name: String): Direction? = Direction.entries.firstOrNull { it.name.lowercase() == name }

    private fun axisOf(face: Direction): String =
        when (face) {
            Direction.UP, Direction.DOWN -> "y"
            Direction.NORTH, Direction.SOUTH -> "z"
            else -> "x"
        }

    private fun name(face: Direction): String = face.name.lowercase()

    private fun opposite(face: Direction): Direction =
        when (face) {
            Direction.DOWN -> Direction.UP
            Direction.UP -> Direction.DOWN
            Direction.NORTH -> Direction.SOUTH
            Direction.SOUTH -> Direction.NORTH
            Direction.WEST -> Direction.EAST
            Direction.EAST -> Direction.WEST
        }

    /** South first, the way vanilla numbers the quarters of a yaw. */
    private val HORIZONTAL = arrayOf(Direction.SOUTH, Direction.WEST, Direction.NORTH, Direction.EAST)

    /** parity: `Player`'s own dimensions: 0.6 wide, 1.8 tall standing and 1.5 crouching. */
    private const val PLAYER_WIDTH = 0.6
    private const val PLAYER_HEIGHT = 1.8
    private const val SNEAKING_HEIGHT = 1.5

    private const val QUARTER_TURN = 90f
    private const val HALF_TURN = 180.0

    /** parity: `RotationSegment`'s sixteen. */
    private const val SEGMENTS = 16
    private const val FULL_TURN = 360f

    /** The blocks that pile another of themselves into the same space. */
    private val STACKING = listOf("candles", "pickles", "eggs", "layers", "flower_amount", "segment_amount")

    /** parity: `GrowingPlantHeadBlock.getStateForPlacement`'s twenty-five. */
    private const val GROWING_AGES = 25

    /** parity: `ScaffoldingBlock.getDistance`'s own seven. */
    private const val SCAFFOLD_REACH = 7

    private val GROWING_HEADS =
        setOf("kelp", "twisting_vines", "weeping_vines", "cave_vines")
    private const val HALF = 0.5f
}

/**
 * The block that goes down when the standing one is put against a wall.
 *
 * parity: `StandingAndWallBlockItem`, `SignItem` and `HangingSignItem`, three item classes that all
 * do the same thing: hold a pair of blocks and pick the wall one when the face clicked is a side.
 * Every pair in 26.2 follows the same naming, so the registry answers instead of a hardcoded table.
 */
public object WallVariants {
    private val cache = HashMap<String, String?>()

    /** The wall form of [block], or null when it has none. */
    public fun of(block: Identifier): Identifier? {
        val wall =
            cache.getOrPut(block.path) {
                candidates(block.path).firstOrNull { Blocks.REGISTRY.idOf(Identifier.vanilla(it)) != null }
            }
        return wall?.let { Identifier.vanilla(it) }
    }

    private fun candidates(path: String): List<String> {
        // A hanging sign puts the wall in front of the pair of words, not before the last one.
        if (path.endsWith("_hanging_sign")) return listOf(path.replace("_hanging_sign", "_wall_hanging_sign"))
        val cut = path.lastIndexOf('_')
        if (cut < 0) return listOf("wall_$path")
        return listOf(path.substring(0, cut) + "_wall_" + path.substring(cut + 1))
    }
}

/**
 * Where a scaffolding really lands.
 *
 * parity: `ScaffoldingBlockItem.updatePlacementContext`, which is the one placement rule that lives
 * in the item and not in the block: clicking an existing scaffolding walks along the tower until it
 * finds room, up instead of sideways unless the player is sneaking, and refuses altogether when the
 * column has run its seven blocks out.
 */
public object ScaffoldingPlacement {
    private const val REACH = 7

    /** The position to build at, or null when the click puts nothing anywhere. */
    public fun target(
        world: LoadedWorld,
        clickedX: Int,
        clickedY: Int,
        clickedZ: Int,
        face: Direction,
        yaw: Float,
        sneaking: Boolean,
        inside: Boolean,
    ): Triple<Int, Int, Int>? {
        val clicked = world.state.getBlockState(clickedX, clickedY, clickedZ) ?: return null
        if (!isScaffolding(clicked)) {
            // Off the ground the tower is already as long as it goes, and nothing is placed.
            val distance = BlockStateDefinitions.propertiesOf(clicked)["distance"]?.toIntOrNull()
            if (isScaffolding(clicked) && distance == REACH) return null
            return null
        }
        val direction =
            when {
                sneaking -> if (inside) opposite(face) else face
                face == Direction.UP -> BlockPlacement.playerDirection(yaw)
                else -> Direction.UP
            }
        var x = clickedX + stepX(direction)
        var y = clickedY + stepY(direction)
        var z = clickedZ + stepZ(direction)
        var walked = 0
        while (walked < REACH) {
            if (y < world.minY || y > world.maxY) return null
            val here = world.state.getBlockState(x, y, z) ?: return null
            if (!isScaffolding(here)) {
                return if (BlockShapes.isReplaceable(here)) Triple(x, y, z) else null
            }
            x += stepX(direction)
            y += stepY(direction)
            z += stepZ(direction)
            if (direction != Direction.UP && direction != Direction.DOWN) walked++
        }
        return null
    }

    private fun isScaffolding(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "scaffolding"

    private fun opposite(face: Direction): Direction =
        when (face) {
            Direction.DOWN -> Direction.UP
            Direction.UP -> Direction.DOWN
            Direction.NORTH -> Direction.SOUTH
            Direction.SOUTH -> Direction.NORTH
            Direction.WEST -> Direction.EAST
            Direction.EAST -> Direction.WEST
        }

    private fun stepX(direction: Direction): Int =
        if (direction == Direction.WEST) {
            -1
        } else if (direction == Direction.EAST) {
            1
        } else {
            0
        }

    private fun stepY(direction: Direction): Int =
        if (direction == Direction.DOWN) {
            -1
        } else if (direction == Direction.UP) {
            1
        } else {
            0
        }

    private fun stepZ(direction: Direction): Int =
        if (direction == Direction.NORTH) {
            -1
        } else if (direction == Direction.SOUTH) {
            1
        } else {
            0
        }
}

/**
 * parity: `MossyCarpetBlock.setPlacedBy`, which grows a second carpet over the one just placed with
 * a coin toss for each side that could stand there.
 */
public object MossCarpetTopper {
    /** The carpet to write above [x], [y], [z], or null when nothing grows there. */
    public fun above(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        coin: () -> Boolean,
    ): Int? {
        val above = world.state.getBlockState(x, y + 1, z) ?: return null
        val isCarpet = BlockStateDefinitions.blockOf(above)?.path == "pale_moss_carpet"
        if (isCarpet && BlockStateDefinitions.propertiesOf(above)["bottom"] == "true") return null
        if (!isCarpet && !BlockShapes.isReplaceable(above)) return null
        val wanted = HashMap<String, String>(5)
        wanted["bottom"] = "false"
        var standing = false
        for (face in listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST, BlockFace.EAST)) {
            val name = face.name.lowercase()
            val stepX =
                if (face == BlockFace.WEST) {
                    -1
                } else if (face == BlockFace.EAST) {
                    1
                } else {
                    0
                }
            val stepZ =
                if (face == BlockFace.NORTH) {
                    -1
                } else if (face == BlockFace.SOUTH) {
                    1
                } else {
                    0
                }
            val beside = world.state.getBlockState(x + stepX, y + 1, z + stepZ) ?: 0
            val side = if (BlockShapes.isFaceSturdy(beside, face.opposite) && coin()) "low" else "none"
            if (side != "none") standing = true
            wanted[name] = side
        }
        if (!standing) return null
        val topper =
            BlockStateDefinitions.stateIdOf(Identifier.vanilla("pale_moss_carpet"), wanted) ?: return null
        return if (topper == above) null else topper
    }
}

/** Where a block's second half goes, and what state it is in. */
public class SecondHalf(
    public val x: Int,
    public val y: Int,
    public val z: Int,
    public val stateId: Int,
)

/**
 * The other block a two-block one needs.
 *
 * parity: `setPlacedBy` on `DoorBlock`, `BedBlock` and `DoublePlantBlock`, which is what turns the
 * one block the client asked for into the pair the game really has. Without it the half that was
 * placed has no partner, and `updateShape` takes it straight back out.
 */
public object DoubleBlocks {
    /** The second half of [stateId] placed at these coordinates, or null for an ordinary block. */
    public fun secondHalf(
        stateId: Int,
        x: Int,
        y: Int,
        z: Int,
    ): SecondHalf? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["half"] == "lower") {
            val upper = BlockStateDefinitions.stateIdOf(block, mapOf("half" to "upper"), stateId) ?: return null
            return SecondHalf(x, y + 1, z, upper)
        }
        if (properties["part"] == "foot") {
            val facing = properties["facing"] ?: return null
            val head = BlockStateDefinitions.stateIdOf(block, mapOf("part" to "head"), stateId) ?: return null
            return SecondHalf(x + offsetX(facing), y, z + offsetZ(facing), head)
        }
        return null
    }

    /** The state a two-block one starts as: the lower half or the foot, never the other one. */
    public fun firstHalf(stateId: Int): Int {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return stateId
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if ("half" in properties &&
            properties["half"] != "lower" &&
            "upper" in (
                BlockStateDefinitions.of(block)?.valuesOf("half") ?: emptyList()
            )
        ) {
            return BlockStateDefinitions.stateIdOf(block, mapOf("half" to "lower"), stateId) ?: stateId
        }
        if ("part" in properties && properties["part"] != "foot") {
            return BlockStateDefinitions.stateIdOf(block, mapOf("part" to "foot"), stateId) ?: stateId
        }
        return stateId
    }

    private fun offsetX(facing: String): Int =
        if (facing == "west") {
            -1
        } else if (facing == "east") {
            1
        } else {
            0
        }

    private fun offsetZ(facing: String): Int =
        if (facing == "north") {
            -1
        } else if (facing == "south") {
            1
        } else {
            0
        }
}
