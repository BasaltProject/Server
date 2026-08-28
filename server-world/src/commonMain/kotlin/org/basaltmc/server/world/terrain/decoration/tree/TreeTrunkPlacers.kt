package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.IntProvider
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

public sealed class TrunkPlacer(
    protected val baseHeight: Int,
    protected val heightRandA: Int,
    protected val heightRandB: Int,
) {
    public fun getTreeHeight(random: WorldgenRandom): Int =
        baseHeight + random.nextInt(heightRandA + 1) + random.nextInt(heightRandB + 1)

    public abstract fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment>

    protected fun placeLog(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        trunkStateId: Int,
    ): Boolean {
        if (!validTreePos(chunk, x, y, z)) return false
        setBlock(chunk, x, y, z, trunkStateId)
        return true
    }

    /** // parity: TrunkPlacer.validTreePos; specialized placers may extend the accepted blocks. */
    protected open fun validTreePos(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = isValidTreePos(chunk, x, y, z)

    /** // parity: TrunkPlacer.placeBelowTrunkBlock with the default rule provider. */
    protected fun placeBelowTrunkBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
        val stateId = chunk.getDecorationBlockStateId(x, y, z)
        val path = Blocks.byStateId(stateId)?.block?.path
        if (path !in CANNOT_REPLACE_BELOW_TREE_TRUNK_PATHS) setBlock(chunk, x, y, z, Blocks.DIRT.stateId)
    }

    public fun isFree(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        validTreePos(chunk, x, y, z) ||
            TreeFeature.isLogState(chunk.getDecorationBlockStateId(x, y, z))

    /** // parity: TrunkPlacer.placeLogIfFree. */
    protected fun placeLogIfFree(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        trunkStateId: Int,
    ) {
        if (isFree(chunk, x, y, z)) placeLog(chunk, x, y, z, trunkStateId)
    }

    protected fun isAirOrLeaves(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return false
        val stateId = chunk.getDecorationBlockStateId(x, y, z)
        return Blocks.isAir(stateId) || Blocks.byStateId(stateId)?.block?.path in leafBlockPaths
    }

    private companion object {
        val CANNOT_REPLACE_BELOW_TREE_TRUNK_PATHS =
            setOf(
                "dirt",
                "coarse_dirt",
                "rooted_dirt",
                "mud",
                "muddy_mangrove_roots",
                "moss_block",
                "pale_moss_block",
                "podzol",
            )
        val leafBlockPaths: Set<String> =
            setOf(
                "oak_leaves",
                "birch_leaves",
                "spruce_leaves",
                "jungle_leaves",
                "acacia_leaves",
                "dark_oak_leaves",
                "pale_oak_leaves",
                "mangrove_leaves",
                "cherry_leaves",
                "azalea_leaves",
                "flowering_azalea_leaves",
            )
    }
}

/** // parity: StraightTrunkPlacer. */
public class StraightTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment> {
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ)
        for (y in 0 until treeHeight) placeLog(chunk, originX, originY + y, originZ, trunkStateId)
        return listOf(FoliageAttachment(originX, originY + treeHeight, originZ, 0, false))
    }
}

/** // parity: BendingTrunkPlacer, used by the azalea tree grown by RootSystemFeature. */
public class BendingTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
    private val minHeightForLeaves: Int,
    private val bendLength: IntProvider,
    private val belowTrunkStateId: Int,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment> {
        val direction = HORIZONTAL_DIRECTIONS[random.nextInt(HORIZONTAL_DIRECTIONS.size)]
        val logHeight = treeHeight - 1
        var x = originX
        var y = originY
        var z = originZ
        setBlock(chunk, x, y - 1, z, belowTrunkStateId)
        val attachments = mutableListOf<FoliageAttachment>()

        for (level in 0..logHeight) {
            if (level + 1 >= logHeight + random.nextInt(2)) {
                x += direction.first
                z += direction.second
            }
            placeLog(chunk, x, y, z, trunkStateId)
            if (level >= minHeightForLeaves) attachments += FoliageAttachment(x, y, z, 0)
            y++
        }

        repeat(bendLength.sample(random) + 1) {
            placeLog(chunk, x, y, z, trunkStateId)
            attachments += FoliageAttachment(x, y, z, 0)
            x += direction.first
            z += direction.second
        }
        return attachments
    }
}

/** // parity: FancyTrunkPlacer, including its limb collision probes and branch clusters. */
public class FancyTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment> {
        val height = treeHeight + 2
        val trunkHeight = floor(height * TRUNK_HEIGHT_SCALE).toInt()
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ)
        val clustersPerY =
            min(1, floor(CLUSTER_DENSITY_MAGIC + (height.toDouble() / 13.0) * (height.toDouble() / 13.0)).toInt())
        val trunkTop = originY + trunkHeight
        var relativeY = height - ASSUMED_FOLIAGE_HEIGHT
        val foliageCoords =
            mutableListOf(FancyFoliageCoords(FoliageAttachment(originX, originY + relativeY, originZ, 0), trunkTop))

        while (relativeY >= 0) {
            val shape = treeShape(height, relativeY)
            if (shape >= 0.0f) {
                repeat(clustersPerY) {
                    val radius = shape * (random.nextFloat() + BRANCH_LENGTH_MAGIC)
                    val angle = random.nextFloat() * 2.0 * kotlin.math.PI
                    val checkStartX = originX + floor(radius * sin(angle) + 0.5).toInt()
                    val checkStartY = originY + relativeY - 1
                    val checkStartZ = originZ + floor(radius * cos(angle) + 0.5).toInt()
                    if (
                        makeLimb(
                            chunk,
                            checkStartX,
                            checkStartY,
                            checkStartZ,
                            checkStartX,
                            checkStartY + ASSUMED_FOLIAGE_HEIGHT,
                            checkStartZ,
                            checkOnly = true,
                            trunkStateId = trunkStateId,
                        )
                    ) {
                        val dx = originX - checkStartX
                        val dz = originZ - checkStartZ
                        val branchHeight = checkStartY - sqrt((dx * dx + dz * dz).toDouble()) * BRANCH_SLOPE
                        val branchTop = if (branchHeight > trunkTop) trunkTop else branchHeight.toInt()
                        if (
                            makeLimb(
                                chunk,
                                originX,
                                branchTop,
                                originZ,
                                checkStartX,
                                checkStartY,
                                checkStartZ,
                                checkOnly = true,
                                trunkStateId = trunkStateId,
                            )
                        ) {
                            foliageCoords +=
                                FancyFoliageCoords(
                                    FoliageAttachment(checkStartX, checkStartY, checkStartZ, 0),
                                    branchTop,
                                )
                        }
                    }
                }
            }
            relativeY--
        }

        makeLimb(
            chunk,
            originX,
            originY,
            originZ,
            originX,
            originY + trunkHeight,
            originZ,
            checkOnly = false,
            trunkStateId = trunkStateId,
        )
        for (coord in foliageCoords) {
            if (coord.attachment.x == originX &&
                coord.attachment.y == coord.branchBase &&
                coord.attachment.z == originZ
            ) {
                continue
            }
            if (trimBranches(height, coord.branchBase - originY)) {
                makeLimb(
                    chunk,
                    originX,
                    coord.branchBase,
                    originZ,
                    coord.attachment.x,
                    coord.attachment.y,
                    coord.attachment.z,
                    checkOnly = false,
                    trunkStateId = trunkStateId,
                )
            }
        }
        return foliageCoords.filter { trimBranches(height, it.branchBase - originY) }.map { it.attachment }
    }

    private fun makeLimb(
        chunk: Chunk,
        startX: Int,
        startY: Int,
        startZ: Int,
        endX: Int,
        endY: Int,
        endZ: Int,
        checkOnly: Boolean,
        trunkStateId: Int,
    ): Boolean {
        if (checkOnly && startX == endX && startY == endY && startZ == endZ) return true
        val deltaX = endX - startX
        val deltaY = endY - startY
        val deltaZ = endZ - startZ
        val steps = max(abs(deltaX), max(abs(deltaY), abs(deltaZ)))
        if (steps == 0) {
            if (!checkOnly) placeLog(chunk, startX, startY, startZ, trunkStateId)
            return true
        }
        val dx = deltaX.toFloat() / steps
        val dy = deltaY.toFloat() / steps
        val dz = deltaZ.toFloat() / steps
        for (step in 0..steps) {
            val x = startX + floor(0.5f + step * dx).toInt()
            val y = startY + floor(0.5f + step * dy).toInt()
            val z = startZ + floor(0.5f + step * dz).toInt()
            if (checkOnly) {
                if (!isFree(chunk, x, y, z)) return false
            } else {
                placeLog(chunk, x, y, z, axisStateId(trunkStateId, startX, startZ, x, z))
            }
        }
        return true
    }

    private fun axisStateId(
        defaultStateId: Int,
        startX: Int,
        startZ: Int,
        x: Int,
        z: Int,
    ): Int {
        val xDifference = abs(x - startX)
        val zDifference = abs(z - startZ)
        val largest = max(xDifference, zDifference)
        return when {
            largest == 0 -> defaultStateId
            xDifference == largest -> defaultStateId - 1
            else -> defaultStateId + 1
        }
    }

    private fun trimBranches(
        height: Int,
        localY: Int,
    ): Boolean = localY.toDouble() >= height * 0.2

    private fun treeShape(
        height: Int,
        y: Int,
    ): Float {
        if (y < height * 0.3f) return -1.0f
        val radius = height / 2.0f
        val adjacent = radius - y
        if (abs(adjacent) >= radius) return if (adjacent == 0.0f) radius * 0.5f else 0.0f
        val distance = if (adjacent == 0.0f) radius else sqrt(radius * radius - adjacent * adjacent)
        return distance * 0.5f
    }

    private data class FancyFoliageCoords(
        val attachment: FoliageAttachment,
        val branchBase: Int,
    )

    private companion object {
        const val ASSUMED_FOLIAGE_HEIGHT = 5
        const val TRUNK_HEIGHT_SCALE = 0.618
        const val CLUSTER_DENSITY_MAGIC = 1.382
        const val BRANCH_SLOPE = 0.381
        const val BRANCH_LENGTH_MAGIC = 0.328
    }
}

/** // parity: ForkingTrunkPlacer (acacia). */
public class ForkingTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment> {
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ)
        val attachments = mutableListOf<FoliageAttachment>()
        val leanDirection = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
        val leanHeight = treeHeight - random.nextInt(4) - 1
        var leanSteps = 3 - random.nextInt(3)
        var tx = originX
        var tz = originZ
        var ey = Int.MIN_VALUE

        for (yo in 0 until treeHeight) {
            val yy = originY + yo
            if (yo >= leanHeight && leanSteps > 0) {
                tx += leanDirection.first
                tz += leanDirection.second
                leanSteps--
            }
            if (placeLog(chunk, tx, yy, tz, trunkStateId)) ey = yy + 1
        }
        if (ey != Int.MIN_VALUE) attachments.add(FoliageAttachment(tx, ey, tz, 1, false))

        tx = originX
        tz = originZ
        val branchDirection = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
        if (branchDirection != leanDirection) {
            var branchPos = leanHeight - random.nextInt(2) - 1
            var branchSteps = 1 + random.nextInt(3)
            ey = Int.MIN_VALUE
            var yo = branchPos
            while (yo < treeHeight && branchSteps > 0) {
                if (yo >= 1) {
                    val yy = originY + yo
                    tx += branchDirection.first
                    tz += branchDirection.second
                    if (placeLog(chunk, tx, yy, tz, trunkStateId)) ey = yy + 1
                }
                yo++
                branchSteps--
            }
            if (ey != Int.MIN_VALUE) attachments.add(FoliageAttachment(tx, ey, tz, 0, false))
        }
        return attachments
    }
}

/** // parity: DarkOakTrunkPlacer, a 2x2 trunk with random side branches. */
public class DarkOakTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment> {
        val attachments = mutableListOf<FoliageAttachment>()
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ)
        placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ)
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ + 1)
        placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ + 1)
        val leanDirection = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
        val leanHeight = treeHeight - random.nextInt(4)
        var leanSteps = 2 - random.nextInt(3)
        var tx = originX
        var tz = originZ
        val ey = originY + treeHeight - 1

        for (dy in 0 until treeHeight) {
            if (dy >= leanHeight && leanSteps > 0) {
                tx += leanDirection.first
                tz += leanDirection.second
                leanSteps--
            }
            val yy = originY + dy
            if (isAirOrLeaves(chunk, tx, yy, tz)) {
                placeLog(chunk, tx, yy, tz, trunkStateId)
                placeLog(chunk, tx + 1, yy, tz, trunkStateId)
                placeLog(chunk, tx, yy, tz + 1, trunkStateId)
                placeLog(chunk, tx + 1, yy, tz + 1, trunkStateId)
            }
        }
        attachments.add(FoliageAttachment(tx, ey, tz, 0, true))

        for (ox in -1..2) {
            for (oz in -1..2) {
                if ((ox < 0 || ox > 1 || oz < 0 || oz > 1) && random.nextInt(3) <= 0) {
                    val length = random.nextInt(3) + 2
                    for (branchY in 0 until length) {
                        placeLog(chunk, originX + ox, ey - branchY - 1, originZ + oz, trunkStateId)
                    }
                    attachments.add(FoliageAttachment(originX + ox, ey, originZ + oz, 0, false))
                }
            }
        }
        return attachments
    }
}

/** // parity: GiantTrunkPlacer (mega spruce/pine): a solid 2x2 trunk, no lean/branches. */
public open class GiantTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment> {
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ)
        placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ)
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ + 1)
        placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ + 1)
        for (hh in 0 until treeHeight) {
            placeLogIfFree(chunk, originX, originY + hh, originZ, trunkStateId)
            if (hh < treeHeight - 1) {
                placeLogIfFree(chunk, originX + 1, originY + hh, originZ, trunkStateId)
                placeLogIfFree(chunk, originX + 1, originY + hh, originZ + 1, trunkStateId)
                placeLogIfFree(chunk, originX, originY + hh, originZ + 1, trunkStateId)
            }
        }
        return listOf(FoliageAttachment(originX, originY + treeHeight, originZ, 0, true))
    }
}

/** // parity: MegaJungleTrunkPlacer, the giant 2x2 trunk plus high lateral branches. */
public class MegaJungleTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
) : GiantTrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun placeTrunk(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
    ): List<FoliageAttachment> {
        val attachments =
            super
                .placeTrunk(
                    chunk,
                    random,
                    treeHeight,
                    originX,
                    originY,
                    originZ,
                    trunkStateId,
                ).toMutableList()
        var branchHeight = treeHeight - 2 - random.nextInt(4)
        while (branchHeight > treeHeight / 2) {
            val angle = random.nextFloat() * (kotlin.math.PI * 2.0).toFloat()
            var branchX = 0
            var branchZ = 0
            for (step in 0 until 5) {
                branchX = (1.5f + cos(angle) * step).toInt()
                branchZ = (1.5f + sin(angle) * step).toInt()
                placeLog(
                    chunk,
                    originX + branchX,
                    originY + branchHeight - 3 + step / 2,
                    originZ + branchZ,
                    trunkStateId,
                )
            }
            attachments += FoliageAttachment(originX + branchX, originY + branchHeight, originZ + branchZ, -2)
            branchHeight -= 2 + random.nextInt(4)
        }
        return attachments
    }
}

// parity: UpwardsBranchingTrunkPlacer (mangrove).
