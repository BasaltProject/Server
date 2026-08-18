package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/** Where a ray met a block: the point, the block and the face it came in through. */
public class BlockHit(
    public val x: Double,
    public val y: Double,
    public val z: Double,
    public val blockX: Int,
    public val blockY: Int,
    public val blockZ: Int,
    public val face: BlockFace,
)

/**
 * A straight line through the blocks, stopping at the first collision shape it meets.
 *
 * parity: `BlockGetter.clip` over `AABB.clip`. The walk is the same one vanilla does, block by block
 * along the line rather than sampling it: a fast arrow crosses several blocks in a tick and has to
 * hit the first of them, not the one it happens to land in.
 */
public object BlockRay {
    /** How many blocks one cast may cross before it gives up, which is more than a tick can move. */
    private const val MAX_STEPS = 256

    public fun cast(
        world: WorldState,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
        toX: Double,
        toY: Double,
        toZ: Double,
    ): BlockHit? {
        val dx = toX - fromX
        val dy = toY - fromY
        val dz = toZ - fromZ
        if (dx == 0.0 && dy == 0.0 && dz == 0.0) return null

        var blockX = floor(fromX).toInt()
        var blockY = floor(fromY).toInt()
        var blockZ = floor(fromZ).toInt()
        val endX = floor(toX).toInt()
        val endY = floor(toY).toInt()
        val endZ = floor(toZ).toInt()

        val stepX = if (dx > 0) 1 else -1
        val stepY = if (dy > 0) 1 else -1
        val stepZ = if (dz > 0) 1 else -1
        // How far along the line one whole block of each axis is, and how far to the next boundary.
        val deltaX = if (dx == 0.0) Double.MAX_VALUE else abs(1.0 / dx)
        val deltaY = if (dy == 0.0) Double.MAX_VALUE else abs(1.0 / dy)
        val deltaZ = if (dz == 0.0) Double.MAX_VALUE else abs(1.0 / dz)
        var nextX = if (dx == 0.0) Double.MAX_VALUE else boundary(fromX, dx) / dx
        var nextY = if (dy == 0.0) Double.MAX_VALUE else boundary(fromY, dy) / dy
        var nextZ = if (dz == 0.0) Double.MAX_VALUE else boundary(fromZ, dz) / dz

        var steps = 0
        while (steps++ < MAX_STEPS) {
            hitInBlock(world, blockX, blockY, blockZ, fromX, fromY, fromZ, dx, dy, dz)?.let { return it }
            if (blockX == endX && blockY == endY && blockZ == endZ) return null
            when {
                nextX < nextY && nextX < nextZ -> {
                    blockX += stepX
                    nextX += deltaX
                }
                nextY < nextZ -> {
                    blockY += stepY
                    nextY += deltaY
                }
                else -> {
                    blockZ += stepZ
                    nextZ += deltaZ
                }
            }
        }
        return null
    }

    /** How far it is from [from] to the next block boundary in the direction it is going. */
    private fun boundary(
        from: Double,
        delta: Double,
    ): Double {
        val block = floor(from)
        return if (delta > 0) block + 1.0 - from else block - from
    }

    /** parity: `AABB.clip` against each box of the block's shape, keeping the nearest. */
    private fun hitInBlock(
        world: WorldState,
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
        dx: Double,
        dy: Double,
        dz: Double,
    ): BlockHit? {
        val stateId = world.getBlockState(blockX, blockY, blockZ) ?: return null
        if (BlockShapes.isEmptyShape(stateId)) return null
        var best: BlockHit? = null
        var bestScale = Double.MAX_VALUE
        for (shape in BlockShapes.collisionBoxes(stateId)) {
            val minX = blockX + shape[0] / UNITS
            val minY = blockY + shape[1] / UNITS
            val minZ = blockZ + shape[2] / UNITS
            val maxX = blockX + shape[3] / UNITS
            val maxY = blockY + shape[4] / UNITS
            val maxZ = blockZ + shape[5] / UNITS

            var enter = 0.0
            var exit = 1.0
            var face: BlockFace? = null
            for (axis in 0 until 3) {
                val origin =
                    when (axis) {
                        0 -> fromX
                        1 -> fromY
                        else -> fromZ
                    }
                val delta =
                    when (axis) {
                        0 -> dx
                        1 -> dy
                        else -> dz
                    }
                val low =
                    when (axis) {
                        0 -> minX
                        1 -> minY
                        else -> minZ
                    }
                val high =
                    when (axis) {
                        0 -> maxX
                        1 -> maxY
                        else -> maxZ
                    }
                if (delta == 0.0) {
                    if (origin < low || origin > high) {
                        enter = Double.MAX_VALUE
                        break
                    }
                    continue
                }
                val toLow = (low - origin) / delta
                val toHigh = (high - origin) / delta
                val near = min(toLow, toHigh)
                val far = max(toLow, toHigh)
                if (near > enter) {
                    enter = near
                    face = faceOf(axis, delta > 0)
                }
                exit = min(exit, far)
                if (enter > exit) {
                    enter = Double.MAX_VALUE
                    break
                }
            }
            if (enter == Double.MAX_VALUE || enter > 1.0 || enter >= bestScale) continue
            bestScale = enter
            best =
                BlockHit(
                    fromX + dx * enter,
                    fromY + dy * enter,
                    fromZ + dz * enter,
                    blockX,
                    blockY,
                    blockZ,
                    face ?: BlockFace.UP,
                )
        }
        return best
    }

    /** The face a ray going this way enters through, which is the one it is coming at. */
    private fun faceOf(
        axis: Int,
        positive: Boolean,
    ): BlockFace =
        when (axis) {
            0 -> if (positive) BlockFace.WEST else BlockFace.EAST
            1 -> if (positive) BlockFace.DOWN else BlockFace.UP
            else -> if (positive) BlockFace.NORTH else BlockFace.SOUTH
        }

    private const val UNITS = 32.0
}
