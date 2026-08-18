package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/** A box in world space, the way an entity carries one. parity: `AABB`. */
public class Aabb(
    public val minX: Double,
    public val minY: Double,
    public val minZ: Double,
    public val maxX: Double,
    public val maxY: Double,
    public val maxZ: Double,
) {
    public fun move(
        dx: Double,
        dy: Double,
        dz: Double,
    ): Aabb = Aabb(minX + dx, minY + dy, minZ + dz, maxX + dx, maxY + dy, maxZ + dz)

    /** parity: `AABB.expandTowards`, which grows the box along the movement rather than both ways. */
    public fun expandTowards(
        dx: Double,
        dy: Double,
        dz: Double,
    ): Aabb =
        Aabb(
            if (dx < 0) minX + dx else minX,
            if (dy < 0) minY + dy else minY,
            if (dz < 0) minZ + dz else minZ,
            if (dx > 0) maxX + dx else maxX,
            if (dy > 0) maxY + dy else maxY,
            if (dz > 0) maxZ + dz else maxZ,
        )

    public fun inflate(amount: Double): Aabb =
        Aabb(minX - amount, minY - amount, minZ - amount, maxX + amount, maxY + amount, maxZ + amount)

    public fun intersects(other: Aabb): Boolean =
        minX < other.maxX &&
            maxX > other.minX &&
            minY < other.maxY &&
            maxY > other.minY &&
            minZ < other.maxZ &&
            maxZ > other.minZ

    public fun contains(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean = x in minX..maxX && y in minY..maxY && z in minZ..maxZ

    public companion object {
        /** The box an entity of this width and height standing at these feet coordinates fills. */
        public fun of(
            x: Double,
            y: Double,
            z: Double,
            width: Double,
            height: Double,
        ): Aabb {
            val half = width / 2
            return Aabb(x - half, y, z - half, x + half, y + height, z + half)
        }
    }
}

/**
 * Moving a box through the blocks, one axis at a time.
 *
 * parity: `Entity.collide` over `Shapes.collide`. The order is not free: Y first, then the smaller
 * of the two horizontal moves, then the larger (`Direction.axisStepOrder`). Resolving X before Y
 * lets an entity walk into a wall it should have landed on top of, which is exactly the kind of
 * difference that shows up as a mob standing in the air.
 */
public object EntityCollision {
    /** parity: the 1.0E-7 vanilla treats as no movement at all, and as the slack on a shared edge. */
    private const val EPSILON = 1.0e-7

    /** How far a swept box reaches before this gives up gathering blocks, as a sanity bound. */
    private const val MAX_SPAN = 8

    /**
     * The movement [dx]/[dy]/[dz] that [box] can actually make, written into [out] as x, y, z.
     *
     * Answers what is left of the movement after the blocks in the way take their share.
     */
    public fun collide(
        world: WorldState,
        box: Aabb,
        dx: Double,
        dy: Double,
        dz: Double,
        out: DoubleArray,
    ) {
        out[0] = dx
        out[1] = dy
        out[2] = dz
        if (dx == 0.0 && dy == 0.0 && dz == 0.0) return
        val boxes = collidingBoxes(world, box.expandTowards(dx, dy, dz))
        if (boxes.isEmpty()) return

        var movedX = 0.0
        var movedY = 0.0
        var movedZ = 0.0
        // parity: `Direction.axisStepOrder`, Y first and the bigger horizontal move last.
        if (dy != 0.0) movedY = clip(AXIS_Y, box.move(movedX, movedY, movedZ), boxes, dy)
        val xFirst = abs(dx) >= abs(dz)
        if (xFirst) {
            if (dz != 0.0) movedZ = clip(AXIS_Z, box.move(movedX, movedY, movedZ), boxes, dz)
            if (dx != 0.0) movedX = clip(AXIS_X, box.move(movedX, movedY, movedZ), boxes, dx)
        } else {
            if (dx != 0.0) movedX = clip(AXIS_X, box.move(movedX, movedY, movedZ), boxes, dx)
            if (dz != 0.0) movedZ = clip(AXIS_Z, box.move(movedX, movedY, movedZ), boxes, dz)
        }
        out[0] = movedX
        out[1] = movedY
        out[2] = movedZ
    }

    /** Whether anything solid overlaps [box]. parity: `Level.noCollision`, inverted. */
    public fun collides(
        world: WorldState,
        box: Aabb,
    ): Boolean {
        forEachBox(world, box) { collider ->
            if (collider.intersects(box)) return true
        }
        return false
    }

    /** The blocks whose collision shape overlaps [box], as boxes in world space. */
    public fun collidingBoxes(
        world: WorldState,
        box: Aabb,
    ): List<Aabb> {
        val found = mutableListOf<Aabb>()
        forEachBox(world, box) { found += it }
        return found
    }

    private inline fun forEachBox(
        world: WorldState,
        box: Aabb,
        action: (Aabb) -> Unit,
    ) {
        val minX = floor(box.minX).toInt()
        val maxX = ceil(box.maxX).toInt()
        val minY = floor(box.minY).toInt()
        val maxY = ceil(box.maxY).toInt()
        val minZ = floor(box.minZ).toInt()
        val maxZ = ceil(box.maxZ).toInt()
        if (maxX - minX > MAX_SPAN || maxY - minY > MAX_SPAN || maxZ - minZ > MAX_SPAN) return
        for (y in minY..maxY) {
            for (z in minZ..maxZ) {
                for (x in minX..maxX) {
                    val stateId = world.getBlockState(x, y, z) ?: continue
                    if (BlockShapes.isEmptyShape(stateId)) continue
                    for (shape in BlockShapes.collisionBoxes(stateId)) {
                        action(
                            Aabb(
                                x + shape[0] / UNITS,
                                y + shape[1] / UNITS,
                                z + shape[2] / UNITS,
                                x + shape[3] / UNITS,
                                y + shape[4] / UNITS,
                                z + shape[5] / UNITS,
                            ),
                        )
                    }
                }
            }
        }
    }

    /**
     * parity: `AABB.collide`: how far [box] can go along [axis] before [boxes] stop it. A box only
     * counts when it overlaps on the other two axes by more than the epsilon, so sliding along a
     * flush wall is not a collision.
     */
    private fun clip(
        axis: Int,
        box: Aabb,
        boxes: List<Aabb>,
        distance: Double,
    ): Double {
        var left = distance
        for (collider in boxes) {
            if (abs(left) < EPSILON) return 0.0
            left = clipOne(axis, box, collider, left)
        }
        return left
    }

    private fun clipOne(
        axis: Int,
        box: Aabb,
        collider: Aabb,
        distance: Double,
    ): Double {
        val (movingMin, movingMax) =
            when (axis) {
                AXIS_X -> box.minX to box.maxX
                AXIS_Y -> box.minY to box.maxY
                else -> box.minZ to box.maxZ
            }
        val (colliderMin, colliderMax) =
            when (axis) {
                AXIS_X -> collider.minX to collider.maxX
                AXIS_Y -> collider.minY to collider.maxY
                else -> collider.minZ to collider.maxZ
            }
        if (!overlapsOtherAxes(axis, box, collider)) return distance
        if (distance > 0.0 && colliderMin >= movingMax - EPSILON) {
            return min(distance, colliderMin - movingMax)
        }
        if (distance < 0.0 && colliderMax <= movingMin + EPSILON) {
            return max(distance, colliderMax - movingMin)
        }
        return distance
    }

    private fun overlapsOtherAxes(
        axis: Int,
        box: Aabb,
        collider: Aabb,
    ): Boolean {
        if (axis != AXIS_X && !(box.minX < collider.maxX - EPSILON && box.maxX > collider.minX + EPSILON)) return false
        if (axis != AXIS_Y && !(box.minY < collider.maxY - EPSILON && box.maxY > collider.minY + EPSILON)) return false
        if (axis != AXIS_Z && !(box.minZ < collider.maxZ - EPSILON && box.maxZ > collider.minZ + EPSILON)) return false
        return true
    }

    private const val AXIS_X = 0
    private const val AXIS_Y = 1
    private const val AXIS_Z = 2

    /** The shape table rasterises a block into 32 units a side, which is what the boxes count in. */
    private const val UNITS = BlockShapes.UNITS.toDouble()
}
