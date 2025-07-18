package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.DecorationPos
import org.basaltmc.server.world.terrain.decoration.IntProvider
import org.basaltmc.server.world.terrain.decoration.PaleMossDecoration
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * // parity: net.minecraft.world.level.levelgen.feature.TreeFeature and every trunk placer,
 * foliage placer and `FeatureSize` the Overworld trees drive, decompiled from the jar.
 *
 * Coordinates are world block coordinates, as [DecorationPos] has them. Writes outside the source
 * chunk remain at their true world position through [Chunk.setDecorationBlockStateId] and are
 * delivered when the runtime installs the target chunk.
 */
public data class FoliageAttachment(
    val x: Int,
    val y: Int,
    val z: Int,
    val radiusOffset: Int,
    val doubleTrunk: Boolean = false,
)

public sealed interface FeatureSize {
    public val minClippedHeight: Int?
        get() = null

    public fun getSizeAtHeight(
        treeHeight: Int,
        yo: Int,
    ): Int
}

/** // parity: TwoLayersFeatureSize. */
public class FeatureSizeTwoLayers(
    private val limit: Int,
    private val lowerSize: Int,
    private val upperSize: Int,
    override val minClippedHeight: Int? = null,
) : FeatureSize {
    override fun getSizeAtHeight(
        treeHeight: Int,
        yo: Int,
    ): Int = if (yo < limit) lowerSize else upperSize
}

/** // parity: ThreeLayersFeatureSize (dark_oak). */
public class FeatureSizeThreeLayers(
    private val limit: Int,
    private val upperLimit: Int,
    private val lowerSize: Int,
    private val middleSize: Int,
    private val upperSize: Int,
) : FeatureSize {
    override fun getSizeAtHeight(
        treeHeight: Int,
        yo: Int,
    ): Int =
        if (yo < limit) {
            lowerSize
        } else if (yo >= treeHeight - upperLimit) {
            upperSize
        } else {
            middleSize
        }
}

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

/**
 * // parity: UpwardsBranchingTrunkPlacer (mangrove): a straight trunk with random horizontal
 * branches rolled at every log. Its `can_grow_through` block set extends both the preflight and the
 * actual log placement predicate.
 */
public class UpwardsBranchingTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
    private val extraBranchSteps: IntProvider,
    private val placeBranchPerLogProbability: Float,
    private val extraBranchLength: IntProvider,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    override fun validTreePos(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (super.validTreePos(chunk, x, y, z)) return true
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        return Blocks.byStateId(chunk.getDecorationBlockStateId(x, y, z))?.block?.path in canGrowThroughPaths
    }

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
        for (heightPos in 0 until treeHeight) {
            val currentHeight = originY + heightPos
            val placed = placeLog(chunk, originX, currentHeight, originZ, trunkStateId)
            if (placed && heightPos < treeHeight - 1 && random.nextFloat() < placeBranchPerLogProbability) {
                val branchDir = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
                val branchLen = extraBranchLength.sample(random)
                val branchPos = max(0, branchLen - extraBranchLength.sample(random) - 1)
                var branchSteps = extraBranchSteps.sample(random)
                placeBranch(
                    chunk,
                    random,
                    treeHeight,
                    trunkStateId,
                    attachments,
                    originX,
                    originZ,
                    currentHeight,
                    branchDir,
                    branchPos,
                    branchSteps,
                )
            }
            if (heightPos ==
                treeHeight - 1
            ) {
                attachments.add(FoliageAttachment(originX, currentHeight + 1, originZ, 0, false))
            }
        }
        return attachments
    }

    private fun placeBranch(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        trunkStateId: Int,
        attachments: MutableList<FoliageAttachment>,
        originX: Int,
        originZ: Int,
        currentHeight: Int,
        branchDir: Pair<Int, Int>,
        branchPos: Int,
        branchStepsInitial: Int,
    ) {
        var branchSteps = branchStepsInitial
        var heightAlongBranch = currentHeight + branchPos
        var logX = originX
        var logZ = originZ
        var branchPlacementIndex = branchPos
        while (branchPlacementIndex < treeHeight && branchSteps > 0) {
            if (branchPlacementIndex >= 1) {
                val placementHeight = currentHeight + branchPlacementIndex
                logX += branchDir.first
                logZ += branchDir.second
                heightAlongBranch = placementHeight
                if (placeLog(chunk, logX, placementHeight, logZ, trunkStateId)) heightAlongBranch++
                attachments.add(FoliageAttachment(logX, placementHeight, logZ, 0, false))
            }
            branchPlacementIndex++
            branchSteps--
        }
        if (heightAlongBranch - currentHeight > 1) {
            attachments.add(FoliageAttachment(logX, heightAlongBranch, logZ, 0, false))
            attachments.add(FoliageAttachment(logX, heightAlongBranch - 2, logZ, 0, false))
        }
    }

    private companion object {
        // parity: block tag mangrove_logs_can_grow_through.
        val canGrowThroughPaths: Set<String> =
            setOf(
                "mud",
                "muddy_mangrove_roots",
                "mangrove_roots",
                "mangrove_leaves",
                "mangrove_log",
                "mangrove_propagule",
                "moss_carpet",
                "vine",
            )
    }
}

/**
 * // parity: CherryTrunkPlacer: a trunk with one or two horizontal side branches, plus a middle
 * foliage attachment when the branch-count roll lands on 3.
 */
public class CherryTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
    private val branchCount: IntProvider,
    private val branchHorizontalLength: IntProvider,
    private val branchStartOffsetFromTop: IntProvider.Uniform,
    private val branchEndOffsetFromTop: IntProvider,
) : TrunkPlacer(baseHeight, heightRandA, heightRandB) {
    // parity: the real jar derives this in its own constructor as UniformInt.of(min, max - 1).
    private val secondBranchStartOffsetFromTop: IntProvider =
        IntProvider.uniform(branchStartOffsetFromTop.min, branchStartOffsetFromTop.max - 1)

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
        val firstBranchOffset = max(0, treeHeight - 1 + branchStartOffsetFromTop.sample(random))
        var secondBranchOffset = max(0, treeHeight - 1 + secondBranchStartOffsetFromTop.sample(random))
        if (secondBranchOffset >= firstBranchOffset) secondBranchOffset++

        val branchCount = branchCount.sample(random)
        val hasMiddleBranch = branchCount == 3
        val hasBothSideBranches = branchCount >= 2
        val trunkHeight =
            when {
                hasMiddleBranch -> treeHeight
                hasBothSideBranches -> max(firstBranchOffset, secondBranchOffset) + 1
                else -> firstBranchOffset + 1
            }
        for (y in 0 until trunkHeight) placeLog(chunk, originX, originY + y, originZ, trunkStateId)

        val attachments = mutableListOf<FoliageAttachment>()
        if (hasMiddleBranch) attachments.add(FoliageAttachment(originX, originY + trunkHeight, originZ, 0, false))
        val treeDirection = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
        val sidewaysStateId = horizontalAxisStateId(trunkStateId, treeDirection)
        attachments.add(
            generateBranch(
                chunk,
                random,
                treeHeight,
                originX,
                originY,
                originZ,
                trunkStateId,
                sidewaysStateId,
                treeDirection,
                firstBranchOffset,
                firstBranchOffset < trunkHeight - 1,
            ),
        )
        if (hasBothSideBranches) {
            val opposite = -treeDirection.first to -treeDirection.second
            attachments.add(
                generateBranch(
                    chunk,
                    random,
                    treeHeight,
                    originX,
                    originY,
                    originZ,
                    trunkStateId,
                    sidewaysStateId,
                    opposite,
                    secondBranchOffset,
                    secondBranchOffset < trunkHeight - 1,
                ),
            )
        }
        return attachments
    }

    private fun generateBranch(
        chunk: Chunk,
        random: WorldgenRandom,
        treeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkStateId: Int,
        sidewaysStateId: Int,
        branchDirection: Pair<Int, Int>,
        offsetFromOrigin: Int,
        middleContinuesUpwards: Boolean,
    ): FoliageAttachment {
        var logX = originX
        var logY = originY + offsetFromOrigin
        var logZ = originZ
        val branchEndOffset = treeHeight - 1 + branchEndOffsetFromTop.sample(random)
        val extendAway = middleContinuesUpwards || branchEndOffset < offsetFromOrigin
        val distanceToTrunk = branchHorizontalLength.sample(random) + if (extendAway) 1 else 0
        val endX = originX + branchDirection.first * distanceToTrunk
        val endY = originY + branchEndOffset
        val endZ = originZ + branchDirection.second * distanceToTrunk
        repeat(if (extendAway) 2 else 1) {
            logX += branchDirection.first
            logZ += branchDirection.second
            placeLog(chunk, logX, logY, logZ, sidewaysStateId)
        }
        val verticalDirection = if (endY > logY) 1 else -1
        while (true) {
            val distance = abs(logX - endX) + abs(logY - endY) + abs(logZ - endZ)
            if (distance == 0) return FoliageAttachment(endX, endY + 1, endZ, 0, false)
            val growVertically = random.nextFloat() < abs(endY - logY).toFloat() / distance
            if (growVertically) {
                logY += verticalDirection
            } else {
                logX += branchDirection.first
                logZ += branchDirection.second
            }
            placeLog(chunk, logX, logY, logZ, if (growVertically) trunkStateId else sidewaysStateId)
        }
    }

    private fun horizontalAxisStateId(
        stateId: Int,
        direction: Pair<Int, Int>,
    ): Int {
        val state = Blocks.byStateId(stateId) ?: return stateId
        val axis = if (direction.first != 0) "x" else "z"
        return Blocks.stateIdOf(state.block, state.properties + ("axis" to axis)) ?: stateId
    }
}

/**
 * // parity: RootPlacer + MangroveRootPlacer (the only root placer real vanilla has), decompiled
 * from the real jar: an elevated trunk start plus a recursive root simulation per horizontal
 * direction, including potentially-waterlogged root states and the real
 * `mangrove_roots_can_grow_through` tag.
 */
public class MangroveRootPlacer(
    private val trunkOffsetY: IntProvider,
) {
    /** // parity: RootPlacer.getTrunkOrigin. */
    public fun trunkOriginY(
        random: WorldgenRandom,
        originY: Int,
    ): Int = originY + trunkOffsetY.sample(random)

    public fun placeRoots(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkOriginY: Int,
    ): Boolean {
        var columnY = originY
        while (columnY < trunkOriginY) {
            if (!canPlaceRoot(chunk, originX, columnY, originZ)) return false
            columnY++
        }
        val rootPositions = mutableListOf<Triple<Int, Int, Int>>()
        rootPositions.add(Triple(originX, trunkOriginY - 1, originZ))
        for (dir in HORIZONTAL_DIRECTIONS) {
            val posX = originX + dir.first
            val posZ = originZ + dir.second
            val positionsInDirection = mutableListOf<Triple<Int, Int, Int>>()
            if (!simulateRoots(
                    chunk,
                    random,
                    posX,
                    trunkOriginY,
                    posZ,
                    dir,
                    originX,
                    trunkOriginY,
                    originZ,
                    positionsInDirection,
                    0,
                )
            ) {
                return false
            }
            rootPositions.addAll(positionsInDirection)
            rootPositions.add(Triple(posX, trunkOriginY, posZ))
        }
        for ((x, y, z) in rootPositions) placeRoot(chunk, random, x, y, z)
        return true
    }

    private fun simulateRoots(
        chunk: Chunk,
        random: WorldgenRandom,
        rootX: Int,
        rootY: Int,
        rootZ: Int,
        dir: Pair<Int, Int>,
        originX: Int,
        originY: Int,
        originZ: Int,
        rootPositions: MutableList<Triple<Int, Int, Int>>,
        layer: Int,
    ): Boolean {
        if (layer == MAX_ROOT_LENGTH || rootPositions.size > MAX_ROOT_LENGTH) return false
        for (pos in potentialRootPositions(random, rootX, rootY, rootZ, dir, originX, originY, originZ)) {
            if (canPlaceRoot(chunk, pos.first, pos.second, pos.third)) {
                rootPositions.add(pos)
                if (!simulateRoots(
                        chunk,
                        random,
                        pos.first,
                        pos.second,
                        pos.third,
                        dir,
                        originX,
                        originY,
                        originZ,
                        rootPositions,
                        layer + 1,
                    )
                ) {
                    return false
                }
            }
        }
        return true
    }

    private fun potentialRootPositions(
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        dir: Pair<Int, Int>,
        originX: Int,
        originY: Int,
        originZ: Int,
    ): List<Triple<Int, Int, Int>> {
        val below = Triple(x, y - 1, z)
        val nextTo = Triple(x + dir.first, y, z + dir.second)
        val width = abs(x - originX) + abs(y - originY) + abs(z - originZ)
        return if (width > MAX_ROOT_WIDTH - 3 && width <= MAX_ROOT_WIDTH) {
            if (random.nextFloat() <
                RANDOM_SKEW_CHANCE
            ) {
                listOf(below, Triple(nextTo.first, nextTo.second - 1, nextTo.third))
            } else {
                listOf(below)
            }
        } else if (width > MAX_ROOT_WIDTH) {
            listOf(below)
        } else if (random.nextFloat() < RANDOM_SKEW_CHANCE) {
            listOf(below)
        } else {
            if (random.nextBoolean()) listOf(nextTo) else listOf(below)
        }
    }

    private fun canPlaceRoot(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return false
        val id = chunk.getDecorationBlockStateId(x, y, z)
        return isValidTreePos(chunk, x, y, z) || Blocks.byStateId(id)?.block?.path in canGrowThroughPaths
    }

    private fun placeRoot(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return
        val current = chunk.getDecorationBlockStateId(x, y, z)
        // parity: MangroveRootPlacer.placeRoot: mud/muddy roots become muddy_mangrove_roots
        // regardless of canPlaceRoot; everything else goes through the real RootPlacer.placeRoot.
        if (Blocks.byStateId(current)?.block?.path in muddyRootsInPaths) {
            chunk.setDecorationBlockStateId(x, y, z, potentiallyWaterlogged(muddyMangroveRootsStateId, current))
        } else if (canPlaceRoot(chunk, x, y, z)) {
            chunk.setDecorationBlockStateId(x, y, z, potentiallyWaterlogged(mangroveRootsStateId, current))
            if (random.nextFloat() < ABOVE_ROOT_CHANCE &&
                y + 1 <= WorldHeight.MAX_Y &&
                Blocks.isAir(chunk.getDecorationBlockStateId(x, y + 1, z))
            ) {
                chunk.setDecorationBlockStateId(x, y + 1, z, mossCarpetStateId)
            }
        }
    }

    private fun potentiallyWaterlogged(
        stateId: Int,
        replacedStateId: Int,
    ): Int {
        val state = Blocks.byStateId(stateId) ?: return stateId
        if (!state.properties.containsKey("waterlogged")) return stateId
        return Blocks.stateIdOf(
            state.block,
            state.properties + ("waterlogged" to Blocks.hasWaterFluid(replacedStateId).toString()),
        ) ?: stateId
    }

    private companion object {
        // parity: MangroveRootPlacement(8, 15, 0.2) and AboveRootPlacement(moss_carpet, 0.5), the
        // exact real values both mangrove tree configs use.
        const val MAX_ROOT_WIDTH = 8
        const val MAX_ROOT_LENGTH = 15
        const val RANDOM_SKEW_CHANCE = 0.2f
        const val ABOVE_ROOT_CHANCE = 0.5f

        fun state(name: String): Int =
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla(name),
                )?.stateId
                ?: error("unknown block '$name'")

        val muddyMangroveRootsStateId = state("muddy_mangrove_roots")
        val mangroveRootsStateId = state("mangrove_roots")
        val mossCarpetStateId = state("moss_carpet")

        // parity: block tag mangrove_roots_can_grow_through.
        val canGrowThroughPaths: Set<String> =
            setOf("mud", "muddy_mangrove_roots", "mangrove_roots", "moss_carpet", "vine", "mangrove_propagule", "snow")
        val muddyRootsInPaths: Set<String> = setOf("mud", "muddy_mangrove_roots")
    }
}

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
public class PineFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val height: IntProvider,
) : FoliagePlacer(radius, offset) {
    override fun foliageRadius(
        random: WorldgenRandom,
        trunkHeight: Int,
    ): Int = super.foliageRadius(random, trunkHeight) + random.nextInt(max(trunkHeight + 1, 1))

    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = height.sample(random)

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
        var currentRadius = 0
        var yo = offset
        while (yo >= offset - foliageHeight) {
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
            if (currentRadius >= 1 && yo == offset - foliageHeight + 1) {
                currentRadius--
            } else if (currentRadius < leafRadius + attachment.radiusOffset) {
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

/** // parity: DarkOakFoliagePlacer. */
public class DarkOakFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = 4

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
        val doubleTrunk = attachment.doubleTrunk
        if (doubleTrunk) {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, -1, doubleTrunk)
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 3, 0, doubleTrunk)
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, 1, doubleTrunk)
            if (random.nextBoolean()) {
                placeLeavesRow(
                    chunk,
                    random,
                    foliageStateId,
                    fx,
                    fy,
                    fz,
                    leafRadius,
                    2,
                    doubleTrunk,
                )
            }
        } else {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, -1, doubleTrunk)
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 1, 0, doubleTrunk)
        }
    }

    override fun shouldSkipLocationSigned(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean {
        if (y == 0 &&
            doubleTrunk &&
            (dx == -currentRadius || dx >= currentRadius) &&
            (dz == -currentRadius || dz >= currentRadius)
        ) {
            return true
        }
        return super.shouldSkipLocationSigned(random, dx, y, dz, currentRadius, doubleTrunk)
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean {
        if (y == -1 && !doubleTrunk) return dx == currentRadius && dz == currentRadius
        return if (y == 1) dx + dz > currentRadius * 2 - 2 else false
    }
}

/** // parity: MegaPineFoliagePlacer (mega spruce/pine): a tapered conical crown with a jagged-edge alternation every other row. */
public class MegaPineFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val crownHeight: IntProvider,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = crownHeight.sample(random)

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
        var prevRadius = 0
        val topY = attachment.y + offset
        val bottomY = attachment.y - foliageHeight + offset
        for (yy in bottomY..topY) {
            val yo = attachment.y - yy
            val smoothRadius =
                leafRadius + attachment.radiusOffset + kotlin.math.floor(yo.toFloat() / foliageHeight * 3.5f).toInt()
            val jaggedRadius =
                if (yo > 0 &&
                    smoothRadius == prevRadius &&
                    yy and 1 == 0
                ) {
                    smoothRadius + 1
                } else {
                    smoothRadius
                }
            placeLeavesRow(
                chunk,
                random,
                foliageStateId,
                attachment.x,
                yy,
                attachment.z,
                jaggedRadius,
                0,
                attachment.doubleTrunk,
            )
            prevRadius = smoothRadius
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = if (dx + dz >= 7) true else dx * dx + dz * dz > currentRadius * currentRadius
}

/** // parity: RandomSpreadFoliagePlacer (mangrove): random-attempt leaf scatter within a box, no row structure. */
public class RandomSpreadFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val foliageHeightProvider: IntProvider,
    private val leafPlacementAttempts: Int,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = foliageHeightProvider.sample(random)

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
        repeat(leafPlacementAttempts) {
            val dx = random.nextInt(leafRadius) - random.nextInt(leafRadius)
            val dy = random.nextInt(foliageHeight) - random.nextInt(foliageHeight)
            val dz = random.nextInt(leafRadius) - random.nextInt(leafRadius)
            tryPlaceLeaf(chunk, foliageStateId, attachment.x + dx, attachment.y + dy, attachment.z + dz)
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = false
}

/** RandomSpreadFoliagePlacer with the azalea tree's 3:1 weighted foliage provider. */
public class WeightedRandomSpreadFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val foliageHeightProvider: IntProvider,
    private val leafPlacementAttempts: Int,
    private val weightedStates: List<Pair<Int, Int>>,
) : FoliagePlacer(radius, offset) {
    private val totalWeight = weightedStates.sumOf(Pair<Int, Int>::second)

    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = foliageHeightProvider.sample(random)

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
        repeat(leafPlacementAttempts) {
            val x = attachment.x + random.nextInt(leafRadius) - random.nextInt(leafRadius)
            val y = attachment.y + random.nextInt(foliageHeight) - random.nextInt(foliageHeight)
            val z = attachment.z + random.nextInt(leafRadius) - random.nextInt(leafRadius)
            if (!canPlaceLeaf(chunk, x, y, z)) return@repeat
            var roll = random.nextInt(totalWeight)
            val stateId =
                weightedStates
                    .first { (_, weight) ->
                        roll -= weight
                        roll < 0
                    }.first
            tryPlaceLeaf(chunk, stateId, x, y, z)
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = false
}

/** // parity: CherryFoliagePlacer: a wide layered canopy with random holes and hanging leaves below the two bottom rows. */
public class CherryFoliagePlacer(
    radius: IntProvider,
    offset: IntProvider,
    private val height: IntProvider,
    private val wideBottomLayerHoleChance: Float,
    private val cornerHoleChance: Float,
    private val hangingLeavesChance: Float,
    private val hangingLeavesExtensionChance: Float,
) : FoliagePlacer(radius, offset) {
    override fun foliageHeight(
        random: WorldgenRandom,
        treeHeight: Int,
    ): Int = height.sample(random)

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
        val doubleTrunk = attachment.doubleTrunk
        val fx = attachment.x
        val fy = attachment.y + offset
        val fz = attachment.z
        val currentRadius = leafRadius + attachment.radiusOffset - 1
        placeLeavesRow(
            chunk,
            random,
            foliageStateId,
            fx,
            fy,
            fz,
            currentRadius - 2,
            foliageHeight - 3,
            doubleTrunk,
            foliagePositions,
        )
        placeLeavesRow(
            chunk,
            random,
            foliageStateId,
            fx,
            fy,
            fz,
            currentRadius - 1,
            foliageHeight - 4,
            doubleTrunk,
            foliagePositions,
        )
        for (y in foliageHeight - 5 downTo 0) {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, currentRadius, y, doubleTrunk, foliagePositions)
        }
        placeLeavesRowWithHangingLeavesBelow(
            chunk,
            random,
            foliageStateId,
            fx,
            fy,
            fz,
            currentRadius,
            -1,
            doubleTrunk,
            hangingLeavesChance,
            hangingLeavesExtensionChance,
            foliagePositions,
        )
        placeLeavesRowWithHangingLeavesBelow(
            chunk,
            random,
            foliageStateId,
            fx,
            fy,
            fz,
            currentRadius - 1,
            -2,
            doubleTrunk,
            hangingLeavesChance,
            hangingLeavesExtensionChance,
            foliagePositions,
        )
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean {
        if (y == -1 &&
            (dx == currentRadius || dz == currentRadius) &&
            random.nextFloat() < wideBottomLayerHoleChance
        ) {
            return true
        }
        val corner = dx == currentRadius && dz == currentRadius
        val wideLayer = currentRadius > 2
        return if (wideLayer) {
            corner || (dx + dz > currentRadius * 2 - 2 && random.nextFloat() < cornerHoleChance)
        } else {
            corner && random.nextFloat() < cornerHoleChance
        }
    }
}

public class TreeConfiguration(
    public val trunkStateId: Int,
    public val trunkPlacer: TrunkPlacer,
    public val foliageStateId: Int,
    public val foliagePlacer: FoliagePlacer,
    public val minimumSize: FeatureSize,
    public val rootPlacer: MangroveRootPlacer? = null,
    public val decorators: List<TreeDecoratorConfiguration> = emptyList(),
    public val ignoreVines: Boolean = false,
)

public sealed interface TreeDecoratorConfiguration {
    public data object AlterGroundToPodzol : TreeDecoratorConfiguration

    public data object TrunkVines : TreeDecoratorConfiguration

    public data class LeafVines(
        val probability: Float,
    ) : TreeDecoratorConfiguration

    public data class Cocoa(
        val probability: Float,
    ) : TreeDecoratorConfiguration

    public data class PaleMoss(
        val leavesProbability: Float,
        val trunkProbability: Float,
        val groundProbability: Float,
    ) : TreeDecoratorConfiguration

    public data class CreakingHeart(
        val probability: Float,
    ) : TreeDecoratorConfiguration

    public data class MangrovePropagules(
        val probability: Float,
        val exclusionRadiusXZ: Int,
        val exclusionRadiusY: Int,
        val requiredEmptyBlocks: Int,
    ) : TreeDecoratorConfiguration

    public data class GroundLeafLitter(
        val tries: Int,
        val radius: Int,
        val height: Int,
        val maxSegmentAmount: Int,
    ) : TreeDecoratorConfiguration

    public data class Beehive(
        val probability: Float,
    ) : TreeDecoratorConfiguration
}

/** // parity: TreeFeature.place/doPlace/getMaxFreeTreeHeight, narrowed per this file's own KDoc. */
public object TreeFeature {
    internal fun isLogState(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path in LOG_BLOCK_PATHS

    /** // parity: TreeFeature.validTreePos and the complete replaceable_by_trees tag below. */
    internal fun validTreePos(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = isValidTreePos(chunk, x, y, z)

    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: TreeConfiguration,
        mossyCarpetRandom: BasaltRandom = random,
    ): Boolean {
        val treeHeight = config.trunkPlacer.getTreeHeight(random)
        val foliageHeight = config.foliagePlacer.foliageHeight(random, treeHeight)
        val trunkHeight = treeHeight - foliageHeight
        val leafRadius = config.foliagePlacer.foliageRadius(random, trunkHeight)
        // parity: the trunk origin rises above the origin when a root placer is present, sampled
        // after the foliage radius exactly like the real jar; both origins feed the bounds check.
        val trunkOriginY = config.rootPlacer?.trunkOriginY(random, originY) ?: originY
        val minY = min(originY, trunkOriginY)
        val maxY = max(originY, trunkOriginY) + treeHeight + 1
        if (minY < WorldHeight.MIN_Y + 1 || maxY > WorldHeight.MAX_Y + 1) return false

        val clippedTreeHeight = maxFreeTreeHeight(chunk, treeHeight, originX, trunkOriginY, originZ, config)
        val minClippedHeight = config.minimumSize.minClippedHeight
        if (clippedTreeHeight < treeHeight &&
            (minClippedHeight == null || clippedTreeHeight < minClippedHeight)
        ) {
            return false
        }
        val recorder = TreePlacementRecorder(chunk)
        val generated =
            chunk.observeBlockWrites(recorder::record) {
                generateTree(
                    chunk,
                    random,
                    originX,
                    originY,
                    originZ,
                    trunkOriginY,
                    clippedTreeHeight,
                    foliageHeight,
                    leafRadius,
                    config,
                    recorder,
                )
            }
        if (!generated) return false
        val decoratorContext = recorder.context()
        if (decoratorContext.logs.isEmpty() && decoratorContext.leaves.isEmpty()) return false
        if (config.decorators.isNotEmpty()) {
            recorder.phase = PlacementPhase.DECORATION
            chunk.observeBlockWrites(recorder::record) {
                for (decorator in config.decorators) {
                    decorate(chunk, random, mossyCarpetRandom, decoratorContext, decorator)
                }
            }
        }
        updateLeaves(chunk, recorder.context())
        return true
    }

    private fun generateTree(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        trunkOriginY: Int,
        clippedTreeHeight: Int,
        foliageHeight: Int,
        leafRadius: Int,
        config: TreeConfiguration,
        recorder: TreePlacementRecorder?,
    ): Boolean {
        recorder?.phase = PlacementPhase.ROOT
        if (config.rootPlacer != null &&
            !config.rootPlacer.placeRoots(chunk, random, originX, originY, originZ, trunkOriginY)
        ) {
            return false
        }
        recorder?.phase = PlacementPhase.LOG
        val attachments =
            config.trunkPlacer.placeTrunk(
                chunk,
                random,
                clippedTreeHeight,
                originX,
                trunkOriginY,
                originZ,
                config.trunkStateId,
            )
        val foliagePositions = mutableSetOf<Triple<Int, Int, Int>>()
        recorder?.phase = PlacementPhase.LEAF
        for (attachment in attachments) {
            val offset = config.foliagePlacer.offset(random)
            config.foliagePlacer.createFoliage(
                chunk,
                random,
                config.foliageStateId,
                attachment,
                foliageHeight,
                leafRadius,
                offset,
                foliagePositions,
            )
        }
        return true
    }

    private fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
        mossyCarpetRandom: BasaltRandom,
        context: TreeDecoratorContext,
        decorator: TreeDecoratorConfiguration,
    ) {
        when (decorator) {
            TreeDecoratorConfiguration.AlterGroundToPodzol ->
                alterGroundToPodzol(
                    chunk,
                    random,
                    context.logs,
                    context.roots,
                )
            TreeDecoratorConfiguration.TrunkVines -> decorateTrunkVines(chunk, random, context.logs)
            is TreeDecoratorConfiguration.LeafVines ->
                decorateLeafVines(
                    chunk,
                    random,
                    context.leaves,
                    decorator.probability,
                )
            is TreeDecoratorConfiguration.Cocoa -> decorateCocoa(chunk, random, context.logs, decorator.probability)
            is TreeDecoratorConfiguration.PaleMoss ->
                decoratePaleMoss(chunk, random, mossyCarpetRandom, context, decorator)
            is TreeDecoratorConfiguration.CreakingHeart ->
                decorateCreakingHeart(
                    chunk,
                    random,
                    context.logs,
                    decorator.probability,
                )
            is TreeDecoratorConfiguration.MangrovePropagules ->
                decorateMangrovePropagules(
                    chunk,
                    random,
                    context.leaves,
                    decorator,
                )
            is TreeDecoratorConfiguration.GroundLeafLitter ->
                decorateGroundLeafLitter(
                    chunk,
                    random,
                    context,
                    decorator,
                )
            is TreeDecoratorConfiguration.Beehive -> decorateBeehive(chunk, random, context, decorator.probability)
        }
    }

    /** // parity: TreeFeature.updateLeaves, distance buckets 0 through 6 and six-direction propagation. */
    private fun updateLeaves(
        chunk: Chunk,
        context: TreeDecoratorContext,
    ) {
        val boundsPositions = context.roots + context.logs + context.leaves + context.decorations
        if (context.logs.isEmpty() || boundsPositions.isEmpty()) return
        val minX = boundsPositions.minOf(BlockPos::x)
        val maxX = boundsPositions.maxOf(BlockPos::x)
        val minY = boundsPositions.minOf(BlockPos::y)
        val maxY = boundsPositions.maxOf(BlockPos::y)
        val minZ = boundsPositions.minOf(BlockPos::z)
        val maxZ = boundsPositions.maxOf(BlockPos::z)

        fun isInside(pos: BlockPos): Boolean = pos.x in minX..maxX && pos.y in minY..maxY && pos.z in minZ..maxZ

        val toCheck = Array(7) { LinkedHashSet<BlockPos>() }
        toCheck[0].addAll(context.logs)
        val filled = (context.roots + context.decorations).toMutableSet()
        var smallestDistance = 0
        while (smallestDistance < 7) {
            while (smallestDistance < 7 && toCheck[smallestDistance].isEmpty()) smallestDistance++
            if (smallestDistance >= 7) return
            val iterator = toCheck[smallestDistance].iterator()
            val pos = iterator.next()
            iterator.remove()
            if (!isInside(pos) || !filled.add(pos)) continue

            if (smallestDistance != 0) {
                val currentStateId = chunk.getDecorationBlockStateId(pos.x, pos.y, pos.z)
                val updatedStateId = Blocks.withLeafDistance(currentStateId, smallestDistance) ?: continue
                chunk.setDecorationBlockStateId(pos.x, pos.y, pos.z, updatedStateId)
            }

            for ((dx, dy, dz) in BLOCK_NEIGHBOURS) {
                val neighbor = BlockPos(pos.x + dx, pos.y + dy, pos.z + dz)
                if (!isInside(neighbor) || neighbor in filled) continue
                val stateId = chunk.getDecorationBlockStateId(neighbor.x, neighbor.y, neighbor.z)
                val currentDistance = Blocks.leafDistance(stateId) ?: continue
                val newDistance = min(currentDistance, smallestDistance + 1)
                if (newDistance < 7) {
                    toCheck[newDistance] += neighbor
                    smallestDistance = min(smallestDistance, newDistance)
                }
            }
        }
    }

    /** // parity: AlterGroundDecorator with the beneath_tree_podzol_replaceable rule provider. */
    private fun alterGroundToPodzol(
        chunk: Chunk,
        random: WorldgenRandom,
        logs: List<BlockPos>,
        roots: List<BlockPos>,
    ) {
        val blockPositions =
            when {
                roots.isEmpty() -> logs
                logs.isNotEmpty() && roots.first().y == logs.first().y -> logs + roots
                else -> roots
            }
        if (blockPositions.isEmpty()) return
        val minY = blockPositions.first().y
        for (log in blockPositions.filter { it.y == minY }) {
            placePodzolCircle(chunk, log.x - 1, log.y, log.z - 1)
            placePodzolCircle(chunk, log.x + 2, log.y, log.z - 1)
            placePodzolCircle(chunk, log.x - 1, log.y, log.z + 2)
            placePodzolCircle(chunk, log.x + 2, log.y, log.z + 2)
            repeat(5) {
                val placement = random.nextInt(64)
                val x = placement % 8
                val z = placement / 8
                if (x == 0 || x == 7 || z == 0 || z == 7) {
                    placePodzolCircle(chunk, log.x - 3 + x, log.y, log.z - 3 + z)
                }
            }
        }
    }

    private fun decorateTrunkVines(
        chunk: Chunk,
        random: WorldgenRandom,
        logs: List<BlockPos>,
    ) {
        for (log in logs) {
            for (vine in TRUNK_VINES) {
                if (random.nextInt(3) > 0 && isAir(chunk, log.x + vine.dx, log.y, log.z + vine.dz)) {
                    setBlock(chunk, log.x + vine.dx, log.y, log.z + vine.dz, defaultVineStateId - vine.faceBit)
                }
            }
        }
    }

    private fun decorateLeafVines(
        chunk: Chunk,
        random: WorldgenRandom,
        leaves: List<BlockPos>,
        probability: Float,
    ) {
        for (leaf in leaves) {
            for (vine in TRUNK_VINES) {
                if (random.nextFloat() < probability && isAir(chunk, leaf.x + vine.dx, leaf.y, leaf.z + vine.dz)) {
                    addHangingVine(chunk, leaf.x + vine.dx, leaf.y, leaf.z + vine.dz, vine.faceBit)
                }
            }
        }
    }

    private fun decorateCocoa(
        chunk: Chunk,
        random: WorldgenRandom,
        logs: List<BlockPos>,
        probability: Float,
    ) {
        if (random.nextFloat() >= probability || logs.isEmpty()) return
        val treeY = logs.first().y
        for (log in logs) {
            if (log.y - treeY > 2) continue
            for (cocoa in COCOA_DIRECTIONS) {
                if (random.nextFloat() <= 0.25f) {
                    val x = log.x + cocoa.dx
                    val z = log.z + cocoa.dz
                    if (isAir(chunk, x, log.y, z)) {
                        val age = random.nextInt(3)
                        setBlock(chunk, x, log.y, z, defaultCocoaStateId + age * 4 + cocoa.facingOffset)
                    }
                }
            }
        }
    }

    private fun decoratePaleMoss(
        chunk: Chunk,
        random: WorldgenRandom,
        mossyCarpetRandom: BasaltRandom,
        context: TreeDecoratorContext,
        decorator: TreeDecoratorConfiguration.PaleMoss,
    ) {
        val shuffledLogs = context.logs.toMutableList()
        shuffle(shuffledLogs, random)
        if (shuffledLogs.isEmpty()) return
        val origin = shuffledLogs.minBy(BlockPos::y)
        if (random.nextFloat() < decorator.groundProbability) {
            PaleMossDecoration.placePatchAt(
                chunk,
                random,
                DecorationPos(origin.x, origin.y + 1, origin.z),
                mossyCarpetRandom,
            )
        }
        for (log in context.logs) {
            if (random.nextFloat() < decorator.trunkProbability && isAir(chunk, log.x, log.y - 1, log.z)) {
                addPaleMossHanger(chunk, random, log.x, log.y - 1, log.z)
            }
        }
        for (leaf in context.leaves) {
            if (random.nextFloat() < decorator.leavesProbability && isAir(chunk, leaf.x, leaf.y - 1, leaf.z)) {
                addPaleMossHanger(chunk, random, leaf.x, leaf.y - 1, leaf.z)
            }
        }
    }

    private fun addPaleMossHanger(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        startY: Int,
        z: Int,
    ) {
        var y = startY
        while (isAir(chunk, x, y - 1, z) && random.nextFloat() >= 0.5f) {
            setBlock(chunk, x, y, z, paleHangingMossTipFalseStateId)
            y--
        }
        setBlock(chunk, x, y, z, paleHangingMossTipTrueStateId)
    }

    private fun decorateCreakingHeart(
        chunk: Chunk,
        random: WorldgenRandom,
        logs: List<BlockPos>,
        probability: Float,
    ) {
        if (logs.isEmpty() || random.nextFloat() >= probability) return
        val candidates = logs.toMutableList()
        shuffle(candidates, random)
        val target =
            candidates.firstOrNull { position ->
                BLOCK_NEIGHBOURS.all { (dx, dy, dz) ->
                    val y = position.y + dy
                    y in WorldHeight.MIN_Y..WorldHeight.MAX_Y &&
                        Blocks
                            .byStateId(chunk.getDecorationBlockStateId(position.x + dx, y, position.z + dz))
                            ?.block
                            ?.path
                            ?.let(LOG_BLOCK_PATHS::contains) == true
                }
            } ?: return
        setBlock(chunk, target.x, target.y, target.z, naturalDormantCreakingHeartStateId)
        chunk.setDecorationBlockEntity(
            target.x,
            target.y,
            target.z,
            naturalDormantCreakingHeartStateId,
            Identifier.vanilla("creaking_heart"),
            NbtCompound(),
        )
    }

    /** // parity: AttachedToLeavesDecorator used by both mangrove tree configurations. */
    private fun decorateMangrovePropagules(
        chunk: Chunk,
        random: WorldgenRandom,
        leaves: List<BlockPos>,
        decorator: TreeDecoratorConfiguration.MangrovePropagules,
    ) {
        val blacklist = mutableSetOf<BlockPos>()
        val candidates = leaves.toMutableList()
        shuffle(candidates, random)
        for (leaf in candidates) {
            random.nextInt(1) // Util.getRandom over the single DOWN direction.
            val placement = BlockPos(leaf.x, leaf.y - 1, leaf.z)
            if (placement in blacklist || random.nextFloat() >= decorator.probability) continue
            if ((1..decorator.requiredEmptyBlocks).any { !isAir(chunk, leaf.x, leaf.y - it, leaf.z) }) continue
            for (x in placement.x - decorator.exclusionRadiusXZ..placement.x + decorator.exclusionRadiusXZ) {
                for (y in placement.y - decorator.exclusionRadiusY..placement.y + decorator.exclusionRadiusY) {
                    for (z in placement.z - decorator.exclusionRadiusXZ..placement.z + decorator.exclusionRadiusXZ) {
                        blacklist += BlockPos(x, y, z)
                    }
                }
            }
            val age = random.nextInt(5)
            setBlock(chunk, placement.x, placement.y, placement.z, hangingMangrovePropaguleAge0StateId + age * 8)
        }
    }

    /** // parity: PlaceOnGroundDecorator with VegetationFeatures.leafLitterPatchBuilder. */
    private fun decorateGroundLeafLitter(
        chunk: Chunk,
        random: WorldgenRandom,
        context: TreeDecoratorContext,
        decorator: TreeDecoratorConfiguration.GroundLeafLitter,
    ) {
        val supportPositions = lowestTrunkOrRootPositions(context)
        if (supportPositions.isEmpty()) return
        val minY = supportPositions.first().y
        var minX = supportPositions.first().x
        var maxX = minX
        var minZ = supportPositions.first().z
        var maxZ = minZ
        for (position in supportPositions) {
            if (position.y != minY) continue
            minX = min(minX, position.x)
            maxX = max(maxX, position.x)
            minZ = min(minZ, position.z)
            maxZ = max(maxZ, position.z)
        }
        minX -= decorator.radius
        maxX += decorator.radius
        minZ -= decorator.radius
        maxZ += decorator.radius
        val minSampleY = minY - decorator.height
        val maxSampleY = minY + decorator.height

        repeat(decorator.tries) {
            val x = minX + random.nextInt(maxX - minX + 1)
            val y = minSampleY + random.nextInt(maxSampleY - minSampleY + 1)
            val z = minZ + random.nextInt(maxZ - minZ + 1)
            val aboveY = y + 1
            if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y ||
                aboveY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y
            ) {
                return@repeat
            }
            val aboveStateId = chunk.getDecorationBlockStateId(x, aboveY, z)
            if (!Blocks.isAir(aboveStateId) && !Blocks.isVineState(aboveStateId)) return@repeat
            val supportStateId = chunk.getDecorationBlockStateId(x, y, z)
            if (!BlockShapes.isSolidRender(supportStateId)) return@repeat
            if (decorationHeightAt(chunk, x, z, Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES) >
                aboveY
            ) {
                return@repeat
            }
            val choice = random.nextInt(decorator.maxSegmentAmount * 4)
            val segmentOffset = choice / 4
            val facingOffset = LEAF_LITTER_FACING_STATE_OFFSETS[choice % 4]
            setBlock(chunk, x, aboveY, z, defaultLeafLitterStateId + facingOffset + segmentOffset)
        }
    }

    /** // parity: BeehiveDecorator placement, occupant data, and RNG. */
    private fun decorateBeehive(
        chunk: Chunk,
        random: WorldgenRandom,
        context: TreeDecoratorContext,
        probability: Float,
    ) {
        if (context.logs.isEmpty() || random.nextFloat() >= probability) return
        val hiveY =
            if (context.leaves.isNotEmpty()) {
                max(context.leaves.first().y - 1, context.logs.first().y + 1)
            } else {
                min(context.logs.first().y + 1 + random.nextInt(3), context.logs.last().y)
            }
        val placements = mutableListOf<BlockPos>()
        for (log in context.logs) {
            if (log.y != hiveY) continue
            for ((dx, dz) in BEEHIVE_SPAWN_DIRECTIONS) placements += BlockPos(log.x + dx, hiveY, log.z + dz)
        }
        if (placements.isEmpty()) return
        shuffle(placements, random)
        val hive =
            placements.firstOrNull { isAir(chunk, it.x, it.y, it.z) && isAir(chunk, it.x, it.y, it.z + 1) } ?: return
        setBlock(chunk, hive.x, hive.y, hive.z, southEmptyBeeNestStateId)
        val beeCount = 2 + random.nextInt(2)
        val bees = mutableListOf<NbtTag>()
        repeat(beeCount) {
            val entityData = NbtCompound()
            entityData.putString("id", "minecraft:bee")
            val occupant = NbtCompound()
            occupant["entity_data"] = entityData
            occupant.putInt("ticks_in_hive", random.nextInt(599))
            occupant.putInt("min_ticks_in_hive", 600)
            bees += occupant
        }
        val data = NbtCompound()
        data["bees"] = NbtList(NbtType.COMPOUND, bees)
        chunk.setDecorationBlockEntity(
            hive.x,
            hive.y,
            hive.z,
            southEmptyBeeNestStateId,
            Identifier.vanilla("beehive"),
            data,
        )
    }

    private fun lowestTrunkOrRootPositions(context: TreeDecoratorContext): List<BlockPos> =
        when {
            context.roots.isEmpty() -> context.logs
            context.logs.isNotEmpty() && context.roots.first().y == context.logs.first().y ->
                context.logs +
                    context.roots
            else -> context.roots
        }

    private fun <T> shuffle(
        values: MutableList<T>,
        random: WorldgenRandom,
    ) {
        for (size in values.size downTo 2) {
            val swapTo = random.nextInt(size)
            val last = values[size - 1]
            values[size - 1] = values[swapTo]
            values[swapTo] = last
        }
    }

    private fun addHangingVine(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        faceBit: Int,
    ) {
        val stateId = defaultVineStateId - faceBit
        setBlock(chunk, x, y, z, stateId)
        var cursorY = y - 1
        var remaining = 4
        while (remaining > 0 && isAir(chunk, x, cursorY, z)) {
            setBlock(chunk, x, cursorY, z, stateId)
            cursorY--
            remaining--
        }
    }

    private fun placePodzolCircle(
        chunk: Chunk,
        centerX: Int,
        centerY: Int,
        centerZ: Int,
    ) {
        for (dx in -2..2) {
            for (dz in -2..2) {
                if (abs(dx) != 2 || abs(dz) != 2) placePodzolAt(chunk, centerX + dx, centerY, centerZ + dz)
            }
        }
    }

    private fun placePodzolAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ) {
        for (dy in 2 downTo -3) {
            val cursorY = y + dy
            if (cursorY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) continue
            val stateId = chunk.getDecorationBlockStateId(x, cursorY, z)
            if (Blocks.byStateId(stateId)?.block?.path in beneathTreePodzolReplaceablePaths) {
                chunk.setDecorationBlockStateId(x, cursorY, z, Blocks.PODZOL.stateId)
                return
            }
            if (!Blocks.isAir(stateId) && dy < 0) return
        }
    }

    private fun javaHashSetOrder(positions: List<BlockPos>): Sequence<BlockPos> =
        sequence {
            var capacity = 16
            while (positions.size > capacity * 3 / 4) capacity *= 2
            for (bucket in 0 until capacity) {
                for (position in positions) {
                    val hash = (position.y + position.z * 31) * 31 + position.x
                    val spread = hash xor (hash ushr 16)
                    if (spread and (capacity - 1) == bucket) yield(position)
                }
            }
        }

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        y in WorldHeight.MIN_Y..WorldHeight.MAX_Y &&
            Blocks.isAir(chunk.getDecorationBlockStateId(x, y, z))

    private data class BlockPos(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private data class VinePlacement(
        val dx: Int,
        val dz: Int,
        val faceBit: Int,
    )

    private data class CocoaPlacement(
        val dx: Int,
        val dz: Int,
        val facingOffset: Int,
    )

    private enum class PlacementPhase { ROOT, LOG, LEAF, DECORATION }

    private data class TreeDecoratorContext(
        val logs: List<BlockPos>,
        val leaves: List<BlockPos>,
        val roots: List<BlockPos>,
        val decorations: List<BlockPos>,
    )

    private class TreePlacementRecorder(
        private val chunk: Chunk,
    ) {
        var phase: PlacementPhase = PlacementPhase.ROOT
        private val logs = LinkedHashSet<BlockPos>()
        private val leaves = LinkedHashSet<BlockPos>()
        private val roots = LinkedHashSet<BlockPos>()
        private val decorations = LinkedHashSet<BlockPos>()

        fun record(
            x: Int,
            y: Int,
            z: Int,
            _oldStateId: Int,
            _newStateId: Int,
        ) {
            val position = BlockPos(chunk.chunkX * 16 + x, y, chunk.chunkZ * 16 + z)
            when (phase) {
                PlacementPhase.ROOT -> roots += position
                PlacementPhase.LOG -> logs += position
                PlacementPhase.LEAF -> leaves += position
                PlacementPhase.DECORATION -> decorations += position
            }
        }

        fun context(): TreeDecoratorContext =
            TreeDecoratorContext(sorted(logs), sorted(leaves), sorted(roots), sorted(decorations))

        private fun sorted(positions: Set<BlockPos>): List<BlockPos> =
            javaHashSetOrder(positions.toList()).toList().sortedBy(BlockPos::y)
    }

    private val beneathTreePodzolReplaceablePaths: Set<String> =
        setOf(
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
        )

    private val TRUNK_VINES =
        listOf(
            VinePlacement(-1, 0, 16),
            VinePlacement(1, 0, 1),
            VinePlacement(0, -1, 4),
            VinePlacement(0, 1, 8),
        )
    private val defaultVineStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("vine"),
            )?.stateId
            ?: error("unknown block 'vine'")
    private val COCOA_DIRECTIONS =
        listOf(
            CocoaPlacement(0, 1, 0),
            CocoaPlacement(-1, 0, 3),
            CocoaPlacement(0, -1, 1),
            CocoaPlacement(1, 0, 2),
        )
    private val defaultCocoaStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("cocoa"),
            )?.stateId
            ?: error("unknown block 'cocoa'")
    private val paleHangingMossTipTrueStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("pale_hanging_moss"),
            )?.stateId
            ?: error("unknown block 'pale_hanging_moss'")
    private val paleHangingMossTipFalseStateId = paleHangingMossTipTrueStateId + 1
    private val naturalDormantCreakingHeartStateId =
        (
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("creaking_heart"),
                )?.stateId
                ?: error("unknown block 'creaking_heart'")
        ) + 1
    private val hangingMangrovePropaguleAge0StateId =
        (
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("mangrove_propagule"),
                )?.stateId
                ?: error("unknown block 'mangrove_propagule'")
        ) - 4
    private val defaultLeafLitterStateId =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("leaf_litter"),
            )?.stateId
            ?: error("unknown block 'leaf_litter'")
    private val southEmptyBeeNestStateId =
        (
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("bee_nest"),
                )?.stateId
                ?: error("unknown block 'bee_nest'")
        ) + 6
    private val BEEHIVE_SPAWN_DIRECTIONS = listOf(1 to 0, 0 to 1, -1 to 0)
    private val LEAF_LITTER_FACING_STATE_OFFSETS = intArrayOf(0, 12, 4, 8)

    // parity: BlockTags.LOGS, expanded from the 26.2 server data pack.
    private val LOG_BLOCK_PATHS: Set<String> =
        buildSet {
            for (wood in listOf(
                "dark_oak",
                "pale_oak",
                "oak",
                "acacia",
                "birch",
                "jungle",
                "spruce",
                "mangrove",
                "cherry",
            )) {
                add("${wood}_log")
                add("${wood}_wood")
                add("stripped_${wood}_log")
                add("stripped_${wood}_wood")
            }
            addAll(
                listOf(
                    "crimson_stem",
                    "stripped_crimson_stem",
                    "crimson_hyphae",
                    "stripped_crimson_hyphae",
                    "warped_stem",
                    "stripped_warped_stem",
                    "warped_hyphae",
                    "stripped_warped_hyphae",
                ),
            )
        }
    private val BLOCK_NEIGHBOURS =
        listOf(
            Triple(0, -1, 0),
            Triple(0, 1, 0),
            Triple(0, 0, -1),
            Triple(0, 0, 1),
            Triple(-1, 0, 0),
            Triple(1, 0, 0),
        )

    private fun maxFreeTreeHeight(
        chunk: Chunk,
        maxTreeHeight: Int,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: TreeConfiguration,
    ): Int {
        for (y in 0..maxTreeHeight + 1) {
            val r = config.minimumSize.getSizeAtHeight(maxTreeHeight, y)
            for (x in -r..r) {
                for (z in -r..r) {
                    val stateId = chunk.getDecorationBlockStateId(originX + x, originY + y, originZ + z)
                    val blockedByTreeShape = !config.trunkPlacer.isFree(chunk, originX + x, originY + y, originZ + z)
                    val blockedByVine = !config.ignoreVines && Blocks.isVineState(stateId)
                    if (blockedByTreeShape || blockedByVine) {
                        return y - 2
                    }
                }
            }
        }
        return maxTreeHeight
    }
}

private fun isValidTreePos(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Boolean {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return false
    val stateId = chunk.getDecorationBlockStateId(x, y, z)
    return Blocks.isAir(stateId) || Blocks.byStateId(stateId)?.block?.path in REPLACEABLE_BY_TREES_PATHS
}

private fun setBlock(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
    stateId: Int,
) {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return
    chunk.setDecorationBlockStateId(x, y, z, stateId)
}

private fun tryPlaceLeaf(
    chunk: Chunk,
    foliageStateId: Int,
    x: Int,
    y: Int,
    z: Int,
): Boolean {
    if (canPlaceLeaf(chunk, x, y, z)) {
        val current = chunk.getDecorationBlockStateId(x, y, z)
        val selected = Blocks.byStateId(foliageStateId)
        val placedStateId =
            if (selected?.properties?.containsKey("waterlogged") == true && hasSourceWaterFluid(current)) {
                Blocks.stateIdOf(selected.block, selected.properties + ("waterlogged" to "true")) ?: foliageStateId
            } else {
                foliageStateId
            }
        chunk.setDecorationBlockStateId(x, y, z, placedStateId)
        return true
    }
    return false
}

/** // parity: FluidState.isSourceOfType(WATER), unlike RootPlacer's any-water-fluid check. */
private fun hasSourceWaterFluid(stateId: Int): Boolean =
    Blocks.hasWaterFluid(stateId) && stateId !in Blocks.WATER.stateId + 1..Blocks.WATER.stateId + 15

private fun canPlaceLeaf(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Boolean {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return false
    val stateId = chunk.getDecorationBlockStateId(x, y, z)
    if (Blocks.byStateId(stateId)?.properties?.get("persistent") == "true") return false
    return isValidTreePos(chunk, x, y, z)
}

private fun decorationHeightAt(
    chunk: Chunk,
    worldX: Int,
    worldZ: Int,
    type: Chunk.HeightmapType,
): Int {
    if (worldX shr 4 == chunk.chunkX && worldZ shr 4 == chunk.chunkZ) {
        return chunk.heightAt(worldX and 15, worldZ and 15, type)
    }
    for (y in chunk.verticalBounds.maxY downTo chunk.verticalBounds.minY) {
        if (type.matches(chunk.getDecorationBlockStateId(worldX, y, worldZ))) return y + 1
    }
    return chunk.verticalBounds.minY
}

// parity: real Direction.Plane.HORIZONTAL faces order (NORTH, EAST, SOUTH, WEST); index 0-3 of a
// single nextInt(4) draw, matching Util.getRandom(faces, random).
private val HORIZONTAL_DIRECTIONS = arrayOf(0 to -1, 1 to 0, 0 to 1, -1 to 0)

// parity: minecraft:replaceable_by_trees, flattened from its leaf/flower tags.
private val REPLACEABLE_BY_TREES_PATHS =
    setOf(
        "jungle_leaves",
        "oak_leaves",
        "spruce_leaves",
        "pale_oak_leaves",
        "dark_oak_leaves",
        "acacia_leaves",
        "birch_leaves",
        "azalea_leaves",
        "flowering_azalea_leaves",
        "mangrove_leaves",
        "cherry_leaves",
        "dandelion",
        "open_eyeblossom",
        "poppy",
        "blue_orchid",
        "allium",
        "azure_bluet",
        "red_tulip",
        "orange_tulip",
        "white_tulip",
        "pink_tulip",
        "oxeye_daisy",
        "cornflower",
        "lily_of_the_valley",
        "wither_rose",
        "torchflower",
        "closed_eyeblossom",
        "golden_dandelion",
        "pale_moss_carpet",
        "short_grass",
        "fern",
        "dead_bush",
        "vine",
        "glow_lichen",
        "sunflower",
        "lilac",
        "rose_bush",
        "peony",
        "tall_grass",
        "large_fern",
        "hanging_roots",
        "pitcher_plant",
        "water",
        "seagrass",
        "tall_seagrass",
        "bush",
        "firefly_bush",
        "warped_roots",
        "nether_sprouts",
        "crimson_roots",
        "leaf_litter",
        "short_dry_grass",
        "tall_dry_grass",
    )
