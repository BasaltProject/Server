package org.basaltmc.server.registry

/** The six faces, in vanilla's own `Direction` order. */
public enum class BlockFace(
    public val axis: Int,
    public val positive: Boolean,
) {
    DOWN(axis = 1, positive = false),
    UP(axis = 1, positive = true),
    NORTH(axis = 2, positive = false),
    SOUTH(axis = 2, positive = true),
    WEST(axis = 0, positive = false),
    EAST(axis = 0, positive = true),
    ;

    public val opposite: BlockFace get() = OPPOSITES[ordinal]

    private companion object {
        val OPPOSITES = arrayOf(UP, DOWN, SOUTH, NORTH, EAST, WEST)
    }
}

/**
 * The collision shape of every block state, as the boxes vanilla's own classes report.
 *
 * There is no general `VoxelShape` here and there does not need to be: everything this server asks
 * of a shape is whether it fills its cube, whether a face is sturdy, and whether two touching
 * shapes together wall off the face between them. All three answer from boxes in thirty-seconds,
 * which is exact for every vanilla block.
 *
 * parity: `BlockBehaviour.BlockStateBase.getCollisionShape`, `isFaceSturdy` and `getLightDampening`.
 */
public object BlockShapes {
    /** A block is 32 units on a side here, the finest grid any vanilla shape needs. */
    public const val UNITS: Int = 32

    private val dampening: IntArray
    private val motion: BooleanArray
    private val sturdy: IntArray
    private val boxes: Array<IntArray>
    private val full: BooleanArray

    /** Rasterised faces, filled on first use: one 32-bit row mask per unit of the face. */
    private val faces: Array<IntArray?>

    private val byState: IntArray

    init {
        val entries = ALL_BLOCK_SHAPES
        dampening = IntArray(entries.size)
        motion = BooleanArray(entries.size)
        sturdy = IntArray(entries.size)
        boxes = Array(entries.size) { IntArray(0) }
        full = BooleanArray(entries.size)
        faces = arrayOfNulls(entries.size * BlockFace.entries.size)
        for ((index, entry) in entries.withIndex()) {
            val parts = entry.split('|')
            dampening[index] = parts[0].toInt()
            motion[index] = parts[1] == "1"
            var mask = 0
            for ((face, flag) in parts[2].withIndex()) if (flag == '1') mask = mask or (1 shl face)
            sturdy[index] = mask
            val corners = parts[3].split(';').filter { it.isNotEmpty() }
            val flat = IntArray(corners.size * CORNERS)
            for ((box, text) in corners.withIndex()) {
                for ((corner, value) in text.split(',').withIndex()) flat[box * CORNERS + corner] = value.toInt()
            }
            boxes[index] = flat
            full[index] = flat.size == CORNERS && flat.withIndex().all { (i, v) -> v == if (i < 3) 0 else UNITS }
        }
        byState = IntArray(BLOCK_SHAPE_INDEX.length / 2)
        for (state in byState.indices) {
            val high = INDEX_ALPHABET.indexOf(BLOCK_SHAPE_INDEX[state * 2])
            val low = INDEX_ALPHABET.indexOf(BLOCK_SHAPE_INDEX[state * 2 + 1])
            byState[state] = high * INDEX_ALPHABET.length + low
        }
    }

    /** How much light the block takes out of what passes through it: 0, 1 or 15. */
    public fun dampening(stateId: Int): Int = dampening[shapeOf(stateId)]

    /**
     * Whether the block stops something moving through it, which is what decides whether a fluid
     * may enter it at all. parity: `BlockState.blocksMotion`, which a cobweb and a bamboo sapling
     * answer no to despite being solid.
     */
    public fun blocksMotion(stateId: Int): Boolean = motion[shapeOf(stateId)]

    /** Whether the collision shape fills the whole cube, which is what stops a fluid dead. */
    public fun isFullCube(stateId: Int): Boolean = full[shapeOf(stateId)]

    /** Whether the block has no collision at all: air, a torch, a flower. */
    public fun isEmptyShape(stateId: Int): Boolean = boxes[shapeOf(stateId)].isEmpty()

    /** parity: `BlockState.isFaceSturdy`, which is what decides where a fluid looks for a slope. */
    public fun isFaceSturdy(
        stateId: Int,
        face: BlockFace,
    ): Boolean = sturdy[shapeOf(stateId)] and (1 shl face.ordinal) != 0

    /**
     * Whether the two blocks together wall off the face between them, [face] pointing from
     * [sourceStateId] to [targetStateId].
     *
     * parity: `Shapes.mergedFaceOccludes`. Only the layer of each shape touching the shared plane
     * counts, and the two are merged before asking, so two half-slabs facing each other close a
     * gap that neither closes alone.
     */
    public fun mergedFaceOccludes(
        sourceStateId: Int,
        targetStateId: Int,
        face: BlockFace,
    ): Boolean {
        val source = shapeOf(sourceStateId)
        val target = shapeOf(targetStateId)
        if (full[source] || full[target]) return true
        val negative = if (face.positive) source else target
        val positive = if (face.positive) target else source
        val negativeFace = faceMask(negative, faceOf(face.axis, positive = true))
        val positiveFace = faceMask(positive, faceOf(face.axis, positive = false))
        for (row in 0 until UNITS) {
            if (negativeFace[row] or positiveFace[row] != FULL_ROW) return false
        }
        return true
    }

    private fun shapeOf(stateId: Int): Int = if (stateId in byState.indices) byState[stateId] else 0

    private fun faceOf(
        axis: Int,
        positive: Boolean,
    ): BlockFace =
        when (axis) {
            0 -> if (positive) BlockFace.EAST else BlockFace.WEST
            1 -> if (positive) BlockFace.UP else BlockFace.DOWN
            else -> if (positive) BlockFace.SOUTH else BlockFace.NORTH
        }

    /**
     * The face of a shape, as one row mask per unit, cached per shape and face.
     *
     * Only a box actually reaching that side of the cube contributes, the way slicing the last
     * layer of a voxel shape does.
     */
    private fun faceMask(
        shape: Int,
        face: BlockFace,
    ): IntArray {
        val slot = shape * BlockFace.entries.size + face.ordinal
        faces[slot]?.let { return it }
        val mask = IntArray(UNITS)
        val flat = boxes[shape]
        val boundary = if (face.positive) UNITS else 0
        val edge = if (face.positive) face.axis + 3 else face.axis
        for (box in 0 until flat.size / CORNERS) {
            val offset = box * CORNERS
            if (flat[offset + edge] != boundary) continue
            // The two axes that are not the face's own: rows and columns of its rectangle.
            val rowAxis = if (face.axis == 1) 2 else 1
            val columnAxis = if (face.axis == 0) 2 else 0
            val columns = columnBits(flat[offset + columnAxis], flat[offset + columnAxis + 3])
            for (row in flat[offset + rowAxis] until flat[offset + rowAxis + 3]) mask[row] = mask[row] or columns
        }
        faces[slot] = mask
        return mask
    }

    private fun columnBits(
        from: Int,
        to: Int,
    ): Int {
        var bits = 0
        for (column in from until to) bits = bits or (1 shl column)
        return bits
    }

    private const val CORNERS = 6
    private const val FULL_ROW = -1
    private const val INDEX_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
}
