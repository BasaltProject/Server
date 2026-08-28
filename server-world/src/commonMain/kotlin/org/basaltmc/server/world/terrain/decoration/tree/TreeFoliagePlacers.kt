package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.IntProvider
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

public sealed class FoliagePlacer(
    protected val radius: IntProvider,
    protected val offset: IntProvider,
) {
    public open fun foliageRadius(
        random: WorldgenRandom,
        trunkHeight: Int,
    ): Int = radius.sample(random)

    public abstract fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int

    public abstract fun createFoliage(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        attachment: FoliageAttachment,
        foliageHeight: Int,
        leafRadius: Int,
        offset: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    )

    protected abstract fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean

    /** // parity: FoliagePlacer.shouldSkipLocationSigned. Open so DarkOakFoliagePlacer can override it directly, matching the real jar. */
    protected open fun shouldSkipLocationSigned(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean {
        val minDx = if (doubleTrunk) min(abs(dx), abs(dx - 1)) else abs(dx)
        val minDz = if (doubleTrunk) min(abs(dz), abs(dz - 1)) else abs(dz)
        return shouldSkipLocation(random, minDx, y, minDz, currentRadius, doubleTrunk)
    }

    public fun offset(random: WorldgenRandom): Int = offset.sample(random)

    protected fun placeLeavesRow(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        currentRadius: Int,
        y: Int,
        doubleTrunk: Boolean,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>? = null,
    ) {
        val extra = if (doubleTrunk) 1 else 0
        for (dx in -currentRadius..currentRadius + extra) {
            for (dz in -currentRadius..currentRadius + extra) {
                if (!shouldSkipLocationSigned(random, dx, y, dz, currentRadius, doubleTrunk)) {
                    val x = originX + dx
                    val leafY = originY + y
                    val z = originZ + dz
                    if (tryPlaceLeaf(chunk, foliageStateId, x, leafY, z)) {
                        foliagePositions?.add(Triple(x, leafY, z))
                    }
                }
            }
        }
    }

    /**
     * // parity: FoliagePlacer.placeLeavesRowWithHangingLeavesBelow. [foliagePositions] is the real
     * tree-scoped `foliageSetter.isSet`: existing leaves from an earlier tree do not qualify.
     */
    protected fun placeLeavesRowWithHangingLeavesBelow(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        currentRadius: Int,
        y: Int,
        doubleTrunk: Boolean,
        hangingLeavesChance: Float,
        hangingLeavesExtensionChance: Float,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ) {
        placeLeavesRow(
            chunk,
            random,
            foliageStateId,
            originX,
            originY,
            originZ,
            currentRadius,
            y,
            doubleTrunk,
            foliagePositions,
        )
        val offset = if (doubleTrunk) 1 else 0
        val logY = originY - 1
        // parity: real Direction.Plane.HORIZONTAL faces order (NORTH, EAST, SOUTH, WEST) with each
        // direction's own getClockWise as toEdge (EAST, SOUTH, WEST, NORTH); positive axis
        // directions are EAST and SOUTH.
        val alongEdges = arrayOf(0 to -1, 1 to 0, 0 to 1, -1 to 0)
        val toEdges = arrayOf(1 to 0, 0 to 1, -1 to 0, 0 to -1)
        val toEdgePositive = booleanArrayOf(true, true, false, false)
        for (edge in 0 until 4) {
            val alongEdge = alongEdges[edge]
            val toEdge = toEdges[edge]
            val offsetToEdge = if (toEdgePositive[edge]) currentRadius + offset else currentRadius
            var posX = originX + toEdge.first * offsetToEdge - alongEdge.first * currentRadius
            val posY = originY + y - 1
            var posZ = originZ + toEdge.second * offsetToEdge - alongEdge.second * currentRadius
            var offsetAlongEdge = -currentRadius
            while (offsetAlongEdge < currentRadius + offset) {
                val leavesAbove =
                    posY + 1 in WorldHeight.MIN_Y..WorldHeight.MAX_Y &&
                        Triple(posX, posY + 1, posZ) in foliagePositions
                if (leavesAbove &&
                    tryPlaceExtension(
                        chunk,
                        random,
                        foliageStateId,
                        hangingLeavesChance,
                        originX,
                        logY,
                        originZ,
                        posX,
                        posY,
                        posZ,
                        foliagePositions,
                    )
                ) {
                    tryPlaceExtension(
                        chunk,
                        random,
                        foliageStateId,
                        hangingLeavesExtensionChance,
                        originX,
                        logY,
                        originZ,
                        posX,
                        posY - 1,
                        posZ,
                        foliagePositions,
                    )
                }
                offsetAlongEdge++
                posX += alongEdge.first
                posZ += alongEdge.second
            }
        }
    }

    /** // parity: FoliagePlacer.tryPlaceExtension. */
    private fun tryPlaceExtension(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        chance: Float,
        logX: Int,
        logY: Int,
        logZ: Int,
        x: Int,
        y: Int,
        z: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ): Boolean {
        if (abs(x - logX) + abs(y - logY) + abs(z - logZ) >= 7) return false
        if (random.nextFloat() > chance) return false
        if (!tryPlaceLeaf(chunk, foliageStateId, x, y, z)) return false
        foliagePositions += Triple(x, y, z)
        return true
    }
}

/** // parity: BlobFoliagePlacer (oak, birch, jungle_tree). */
public class BlobFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val height: Int,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = height

    override fun createFoliage(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        attachment: FoliageAttachment,
        foliageHeight: Int,
        leafRadius: Int,
        offset: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ) {
        var yo = offset
        while (yo >= offset - foliageHeight) {
            val currentRadius = max(leafRadius + attachment.radiusOffset - 1 - yo / 2, 0)
            placeLeavesRow(
                chunk,
                random,
                foliageStateId,
                attachment.x,
                attachment.y,
                attachment.z,
                currentRadius,
                yo,
                attachment.doubleTrunk,
            )
            yo--
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = dx == currentRadius && dz == currentRadius && (random.nextInt(2) == 0 || y == 0)
}

/** // parity: BushFoliagePlacer. */
public class BushFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val height: Int,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = height

    override fun createFoliage(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        attachment: FoliageAttachment,
        foliageHeight: Int,
        leafRadius: Int,
        offset: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ) {
        for (y in offset downTo offset - foliageHeight) {
            val currentRadius = leafRadius + attachment.radiusOffset - 1 - y
            placeLeavesRow(
                chunk,
                random,
                foliageStateId,
                attachment.x,
                attachment.y,
                attachment.z,
                currentRadius,
                y,
                attachment.doubleTrunk,
            )
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = dx == currentRadius && dz == currentRadius && random.nextInt(2) == 0
}

/** // parity: MegaJungleFoliagePlacer. */
public class MegaJungleFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val height: Int,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = height

    override fun createFoliage(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        attachment: FoliageAttachment,
        foliageHeight: Int,
        leafRadius: Int,
        offset: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ) {
        val leafHeight = if (attachment.doubleTrunk) foliageHeight else 1 + random.nextInt(2)
        for (y in offset downTo offset - leafHeight) {
            val currentRadius = leafRadius + attachment.radiusOffset + 1 - y
            placeLeavesRow(
                chunk,
                random,
                foliageStateId,
                attachment.x,
                attachment.y,
                attachment.z,
                currentRadius,
                y,
                attachment.doubleTrunk,
            )
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = dx + dz >= 7 || dx * dx + dz * dz > currentRadius * currentRadius
}

/** // parity: FancyFoliagePlacer. */
public class FancyFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val height: Int,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = height

    override fun createFoliage(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        attachment: FoliageAttachment,
        foliageHeight: Int,
        leafRadius: Int,
        offset: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ) {
        for (y in offset downTo offset - foliageHeight) {
            val currentRadius = leafRadius + if (y != offset && y != offset - foliageHeight) 1 else 0
            placeLeavesRow(
                chunk,
                random,
                foliageStateId,
                attachment.x,
                attachment.y,
                attachment.z,
                currentRadius,
                y,
                attachment.doubleTrunk,
            )
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = (dx + 0.5f) * (dx + 0.5f) + (dz + 0.5f) * (dz + 0.5f) > currentRadius * currentRadius
}

/** // parity: AcaciaFoliagePlacer. */
public class AcaciaFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = 0

    override fun createFoliage(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        attachment: FoliageAttachment,
        foliageHeight: Int,
        leafRadius: Int,
        offset: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ) {
        val fx = attachment.x
        val fy = attachment.y + offset
        val fz = attachment.z
        placeLeavesRow(
            chunk,
            random,
            foliageStateId,
            fx,
            fy,
            fz,
            leafRadius + attachment.radiusOffset,
            -1 - foliageHeight,
            attachment.doubleTrunk,
        )
        placeLeavesRow(
            chunk,
            random,
            foliageStateId,
            fx,
            fy,
            fz,
            leafRadius - 1,
            -foliageHeight,
            attachment.doubleTrunk,
        )
        placeLeavesRow(
            chunk,
            random,
            foliageStateId,
            fx,
            fy,
            fz,
            leafRadius + attachment.radiusOffset - 1,
            0,
            attachment.doubleTrunk,
        )
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean =
        if (y ==
            0
        ) {
            (dx > 1 || dz > 1) && dx != 0 && dz != 0
        } else {
            dx == currentRadius && dz == currentRadius && currentRadius > 0
        }
}

/** // parity: SpruceFoliagePlacer. */
public class SpruceFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val trunkHeight: IntProvider,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = max(4, treeHeight - trunkHeight.sample(random))

    override fun createFoliage(
        chunk: Chunk,
        random: WorldgenRandom,
        foliageStateId: Int,
        attachment: FoliageAttachment,
        foliageHeight: Int,
        leafRadius: Int,
        offset: Int,
        foliagePositions: MutableSet<Triple<Int, Int, Int>>,
    ) {
        var currentRadius = random.nextInt(2)
        var maxRadius = 1
        var minRadius = 0
        var yo = offset
        while (yo >= -foliageHeight) {
            placeLeavesRow(
                chunk,
                random,
                foliageStateId,
                attachment.x,
                attachment.y,
                attachment.z,
                currentRadius,
                yo,
                attachment.doubleTrunk,
            )
            if (currentRadius >= maxRadius) {
                currentRadius = minRadius
                minRadius = 1
                maxRadius = min(maxRadius + 1, leafRadius + attachment.radiusOffset)
            } else {
                currentRadius++
            }
            yo--
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = dx == currentRadius && dz == currentRadius && currentRadius > 0
}

/** // parity: PineFoliagePlacer. */
