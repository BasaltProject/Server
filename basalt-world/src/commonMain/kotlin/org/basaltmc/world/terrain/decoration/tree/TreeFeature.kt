package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.decoration.IntProvider
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * // parity: net.minecraft.world.level.levelgen.feature.TreeFeature plus the real trunk/foliage
 * placer and `FeatureSize` types it drives (`TrunkPlacer`/`StraightTrunkPlacer`/`ForkingTrunkPlacer`/
 * `FancyTrunkPlacer`/`DarkOakTrunkPlacer`/`GiantTrunkPlacer`/`MegaJungleTrunkPlacer`/
 * `UpwardsBranchingTrunkPlacer`/`CherryTrunkPlacer`, `FoliagePlacer`/`BlobFoliagePlacer`/
 * `BushFoliagePlacer`/`MegaJungleFoliagePlacer`/`FancyFoliagePlacer`/`AcaciaFoliagePlacer`/`SpruceFoliagePlacer`/`PineFoliagePlacer`/
 * `DarkOakFoliagePlacer`/`MegaPineFoliagePlacer`/`RandomSpreadFoliagePlacer`/`CherryFoliagePlacer`,
 * `TwoLayersFeatureSize`/`ThreeLayersFeatureSize`), all decompiled from the real jar. Coordinates are world block
 * coordinates throughout, matching [org.basaltmc.world.terrain.decoration.DecorationPos]'s own
 * convention; only [Chunk] reads/writes mask X/Z down to chunk-local `and 15`, the same established
 * simplification [org.basaltmc.world.terrain.decoration.PlainsDecoration]'s own pipeline already
 * uses (this project generates one chunk at a time with no cross-chunk write support yet, so a
 * canopy that would reach a neighboring chunk wraps back into this one instead of spilling over,
 * exactly like every other decoration feature here already does).
 *
 * **Documented gaps**, narrowing real vanilla's own `TreeFeature.place`:
 * - No tree decorators (beehives, vines, cocoa, leaf litter), no below-trunk-block provider
 *   (podzol/coarse dirt swaps under certain biomes' trees).
 * - No leaf `distance` blockstate post-processing (the real jar's own `updateLeaves` BFS): leaves
 *   get whatever default `distance` their block state starts with, so persistence/decay-from-a-cut-log
 *   behavior isn't modeled. Doesn't affect initial worldgen shape, only later griefing behavior.
 */
public data class FoliageAttachment(
    val x: Int,
    val y: Int,
    val z: Int,
    val radiusOffset: Int,
    val doubleTrunk: Boolean = false,
)

public sealed interface FeatureSize {
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
        if (!isValidTreePos(chunk, x, y, z)) return false
        setBlock(chunk, x, y, z, trunkStateId)
        return true
    }

    public fun isFree(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = isValidTreePos(chunk, x, y, z)

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
        val stateId = chunk.getBlockStateId(x and 15, y, z and 15)
        return stateId == Blocks.AIR.stateId || stateId in leafStateIds
    }

    private companion object {
        val leafStateIds: Set<Int> by lazy {
            listOf(
                "oak_leaves", "birch_leaves", "spruce_leaves", "jungle_leaves", "acacia_leaves",
                "dark_oak_leaves", "pale_oak_leaves", "mangrove_leaves", "cherry_leaves", "azalea_leaves",
                "flowering_azalea_leaves",
            ).mapNotNull { Blocks.REGISTRY.byIdentifier(org.basaltmc.registry.Identifier.vanilla(it))?.stateId }.toSet()
        }
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
        for (y in 0 until treeHeight) placeLog(chunk, originX, originY + y, originZ, trunkStateId)
        return listOf(FoliageAttachment(originX, originY + treeHeight, originZ, 0, false))
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
        val clustersPerY = min(1, floor(CLUSTER_DENSITY_MAGIC + (height.toDouble() / 13.0) * (height.toDouble() / 13.0)).toInt())
        val trunkTop = originY + trunkHeight
        var relativeY = height - ASSUMED_FOLIAGE_HEIGHT
        val foliageCoords = mutableListOf(FancyFoliageCoords(FoliageAttachment(originX, originY + relativeY, originZ, 0), trunkTop))

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
                            foliageCoords += FancyFoliageCoords(FoliageAttachment(checkStartX, checkStartY, checkStartZ, 0), branchTop)
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
            if (coord.attachment.x == originX && coord.attachment.y == coord.branchBase && coord.attachment.z == originZ) continue
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
                    branchSteps--
                }
                yo++
            }
            if (ey != Int.MIN_VALUE) attachments.add(FoliageAttachment(tx, ey, tz, 0, false))
        }
        return attachments
    }
}

/** // parity: DarkOakTrunkPlacer, a 2x2 trunk with random side branches. No below-trunk-block placement (documented gap, see this file's own KDoc). */
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

/** // parity: GiantTrunkPlacer (mega spruce/pine): a solid 2x2 trunk, no lean/branches. No below-trunk-block placement (documented gap). */
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
        val attachments = super.placeTrunk(chunk, random, treeHeight, originX, originY, originZ, trunkStateId).toMutableList()
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
 * branches rolled at every log. [TrunkPlacer.isFree]/`validTreePos`'s real `canGrowThrough` override
 * (logs may grow through mangrove leaves/propagules) is not ported: a documented, minor gap, not
 * expected to change the trunk's own shape in the common case (nothing else is placed yet when the
 * trunk runs).
 */
public class UpwardsBranchingTrunkPlacer(
    baseHeight: Int,
    heightRandA: Int,
    heightRandB: Int,
    private val extraBranchSteps: IntProvider,
    private val placeBranchPerLogProbability: Float,
    private val extraBranchLength: IntProvider,
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
        for (heightPos in 0 until treeHeight) {
            val currentHeight = originY + heightPos
            val placed = placeLog(chunk, originX, currentHeight, originZ, trunkStateId)
            if (placed && heightPos < treeHeight - 1 && random.nextFloat() < placeBranchPerLogProbability) {
                val branchDir = HORIZONTAL_DIRECTIONS[random.nextInt(4)]
                val branchLen = extraBranchLength.sample(random)
                val branchPos = max(0, branchLen - extraBranchLength.sample(random) - 1)
                var branchSteps = extraBranchSteps.sample(random)
                placeBranch(chunk, random, treeHeight, trunkStateId, attachments, originX, originZ, currentHeight, branchDir, branchPos, branchSteps)
            }
            if (heightPos == treeHeight - 1) attachments.add(FoliageAttachment(originX, currentHeight + 1, originZ, 0, false))
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
}

/**
 * // parity: CherryTrunkPlacer: a trunk with one or two horizontal side branches, plus a middle
 * foliage attachment when the branch-count roll lands on 3. Axis-oriented log states are not
 * modeled (a single [trunkStateId] everywhere), the same documented simplification the rest of
 * this file already uses. No below-trunk-block placement (documented gap, see this file's own KDoc).
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
        attachments.add(
            generateBranch(
                chunk, random, treeHeight, originX, originY, originZ, trunkStateId,
                treeDirection, firstBranchOffset, firstBranchOffset < trunkHeight - 1,
            ),
        )
        if (hasBothSideBranches) {
            val opposite = -treeDirection.first to -treeDirection.second
            attachments.add(
                generateBranch(
                    chunk, random, treeHeight, originX, originY, originZ, trunkStateId,
                    opposite, secondBranchOffset, secondBranchOffset < trunkHeight - 1,
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
            placeLog(chunk, logX, logY, logZ, trunkStateId)
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
            placeLog(chunk, logX, logY, logZ, trunkStateId)
        }
    }
}

/**
 * // parity: RootPlacer + MangroveRootPlacer (the only root placer real vanilla has), decompiled
 * from the real jar: an elevated trunk start plus a recursive root simulation per horizontal
 * direction. Waterlogged root states not modeled (no fluids, this file's documented gap);
 * `TreeFeature.validTreePos` keeps this file's established air-only simplification, with the real
 * `mangrove_roots_can_grow_through` tag on top as a literal set extracted from the jar's own data.
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
            if (!simulateRoots(chunk, random, posX, trunkOriginY, posZ, dir, originX, trunkOriginY, originZ, positionsInDirection, 0)) return false
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
                if (!simulateRoots(chunk, random, pos.first, pos.second, pos.third, dir, originX, originY, originZ, rootPositions, layer + 1)) return false
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
            if (random.nextFloat() < RANDOM_SKEW_CHANCE) listOf(below, Triple(nextTo.first, nextTo.second - 1, nextTo.third)) else listOf(below)
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
        val id = chunk.getBlockStateId(x and 15, y, z and 15)
        return id == Blocks.AIR.stateId || id in canGrowThroughIds
    }

    private fun placeRoot(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return
        val current = chunk.getBlockStateId(x and 15, y, z and 15)
        // parity: MangroveRootPlacer.placeRoot: mud/muddy roots become muddy_mangrove_roots
        // regardless of canPlaceRoot; everything else goes through the real RootPlacer.placeRoot.
        if (current == mudStateId || current == muddyMangroveRootsStateId) {
            chunk.setBlockStateId(x and 15, y, z and 15, muddyMangroveRootsStateId)
        } else if (canPlaceRoot(chunk, x, y, z)) {
            chunk.setBlockStateId(x and 15, y, z and 15, mangroveRootsStateId)
            if (random.nextFloat() < ABOVE_ROOT_CHANCE && y + 1 <= WorldHeight.MAX_Y &&
                chunk.getBlockStateId(x and 15, y + 1, z and 15) == Blocks.AIR.stateId
            ) {
                chunk.setBlockStateId(x and 15, y + 1, z and 15, mossCarpetStateId)
            }
        }
    }

    private companion object {
        // parity: MangroveRootPlacement(8, 15, 0.2) and AboveRootPlacement(moss_carpet, 0.5), the
        // exact real values both mangrove tree configs use.
        const val MAX_ROOT_WIDTH = 8
        const val MAX_ROOT_LENGTH = 15
        const val RANDOM_SKEW_CHANCE = 0.2f
        const val ABOVE_ROOT_CHANCE = 0.5f

        fun state(name: String): Int =
            Blocks.REGISTRY.byIdentifier(org.basaltmc.registry.Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

        val mudStateId = state("mud")
        val muddyMangroveRootsStateId = state("muddy_mangrove_roots")
        val mangroveRootsStateId = state("mangrove_roots")
        val mossCarpetStateId = state("moss_carpet")

        // parity: block tag mangrove_roots_can_grow_through.
        val canGrowThroughIds: Set<Int> =
            listOf("mud", "muddy_mangrove_roots", "mangrove_roots", "moss_carpet", "vine", "mangrove_propagule", "snow")
                .mapNotNull { Blocks.REGISTRY.byIdentifier(org.basaltmc.registry.Identifier.vanilla(it))?.stateId }
                .toSet()
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
    ) {
        val extra = if (doubleTrunk) 1 else 0
        for (dx in -currentRadius..currentRadius + extra) {
            for (dz in -currentRadius..currentRadius + extra) {
                if (!shouldSkipLocationSigned(random, dx, y, dz, currentRadius, doubleTrunk)) {
                    tryPlaceLeaf(chunk, foliageStateId, originX + dx, originY + y, originZ + dz)
                }
            }
        }
    }

    /**
     * // parity: FoliagePlacer.placeLeavesRowWithHangingLeavesBelow. The real `foliageSetter.isSet`
     * ("did this foliage placement set a leaf here") is approximated by "the chunk holds
     * [foliageStateId] here": identical within one tree, marginally wider where two trees' canopies
     * touch (a previous tree's leaves also count).
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
    ) {
        placeLeavesRow(chunk, random, foliageStateId, originX, originY, originZ, currentRadius, y, doubleTrunk)
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
                        chunk.getBlockStateId(posX and 15, posY + 1, posZ and 15) == foliageStateId
                if (leavesAbove &&
                    tryPlaceExtension(chunk, random, foliageStateId, hangingLeavesChance, originX, logY, originZ, posX, posY, posZ)
                ) {
                    tryPlaceExtension(chunk, random, foliageStateId, hangingLeavesExtensionChance, originX, logY, originZ, posX, posY - 1, posZ)
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
    ): Boolean {
        if (abs(x - logX) + abs(y - logY) + abs(z - logZ) >= 7) return false
        if (random.nextFloat() > chance) return false
        return tryPlaceLeaf(chunk, foliageStateId, x, y, z)
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
    ) {
        var yo = offset
        while (yo >= offset - foliageHeight) {
            val currentRadius = max(leafRadius + attachment.radiusOffset - 1 - yo / 2, 0)
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, yo, attachment.doubleTrunk)
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
    ) {
        for (y in offset downTo offset - foliageHeight) {
            val currentRadius = leafRadius + attachment.radiusOffset - 1 - y
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, y, attachment.doubleTrunk)
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
    ) {
        val leafHeight = if (attachment.doubleTrunk) foliageHeight else 1 + random.nextInt(2)
        for (y in offset downTo offset - leafHeight) {
            val currentRadius = leafRadius + attachment.radiusOffset + 1 - y
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, y, attachment.doubleTrunk)
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
    ) {
        for (y in offset downTo offset - foliageHeight) {
            val currentRadius = leafRadius + if (y != offset && y != offset - foliageHeight) 1 else 0
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, y, attachment.doubleTrunk)
        }
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean =
        (dx + 0.5f) * (dx + 0.5f) + (dz + 0.5f) * (dz + 0.5f) > currentRadius * currentRadius
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
    ) {
        val fx = attachment.x
        val fy = attachment.y + offset
        val fz = attachment.z
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + attachment.radiusOffset, -1 - foliageHeight, attachment.doubleTrunk)
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius - 1, -foliageHeight, attachment.doubleTrunk)
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + attachment.radiusOffset - 1, 0, attachment.doubleTrunk)
    }

    override fun shouldSkipLocation(
        random: WorldgenRandom,
        dx: Int,
        y: Int,
        dz: Int,
        currentRadius: Int,
        doubleTrunk: Boolean,
    ): Boolean = if (y == 0) (dx > 1 || dz > 1) && dx != 0 && dz != 0 else dx == currentRadius && dz == currentRadius && currentRadius > 0
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
    ) {
        var currentRadius = random.nextInt(2)
        var maxRadius = 1
        var minRadius = 0
        var yo = offset
        while (yo >= -foliageHeight) {
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, yo, attachment.doubleTrunk)
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
    ) {
        var currentRadius = 0
        var yo = offset
        while (yo >= offset - foliageHeight) {
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, attachment.y, attachment.z, currentRadius, yo, attachment.doubleTrunk)
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
    ) {
        val fx = attachment.x
        val fy = attachment.y + offset
        val fz = attachment.z
        val doubleTrunk = attachment.doubleTrunk
        if (doubleTrunk) {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, -1, doubleTrunk)
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 3, 0, doubleTrunk)
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius + 2, 1, doubleTrunk)
            if (random.nextBoolean()) placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, leafRadius, 2, doubleTrunk)
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
        if (y == 0 && doubleTrunk && (dx == -currentRadius || dx >= currentRadius) && (dz == -currentRadius || dz >= currentRadius)) {
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
    ) {
        var prevRadius = 0
        val topY = attachment.y + offset
        val bottomY = attachment.y - foliageHeight + offset
        for (yy in bottomY..topY) {
            val yo = attachment.y - yy
            val smoothRadius = leafRadius + attachment.radiusOffset + kotlin.math.floor(yo.toFloat() / foliageHeight * 3.5f).toInt()
            val jaggedRadius = if (yo > 0 && smoothRadius == prevRadius && yy and 1 == 0) smoothRadius + 1 else smoothRadius
            placeLeavesRow(chunk, random, foliageStateId, attachment.x, yy, attachment.z, jaggedRadius, 0, attachment.doubleTrunk)
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
    ) {
        val doubleTrunk = attachment.doubleTrunk
        val fx = attachment.x
        val fy = attachment.y + offset
        val fz = attachment.z
        val currentRadius = leafRadius + attachment.radiusOffset - 1
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, currentRadius - 2, foliageHeight - 3, doubleTrunk)
        placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, currentRadius - 1, foliageHeight - 4, doubleTrunk)
        for (y in foliageHeight - 5 downTo 0) {
            placeLeavesRow(chunk, random, foliageStateId, fx, fy, fz, currentRadius, y, doubleTrunk)
        }
        placeLeavesRowWithHangingLeavesBelow(
            chunk, random, foliageStateId, fx, fy, fz, currentRadius, -1, doubleTrunk, hangingLeavesChance, hangingLeavesExtensionChance,
        )
        placeLeavesRowWithHangingLeavesBelow(
            chunk, random, foliageStateId, fx, fy, fz, currentRadius - 1, -2, doubleTrunk, hangingLeavesChance, hangingLeavesExtensionChance,
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
        if (y == -1 && (dx == currentRadius || dz == currentRadius) && random.nextFloat() < wideBottomLayerHoleChance) return true
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
)

/** // parity: TreeFeature.place/doPlace/getMaxFreeTreeHeight, narrowed per this file's own KDoc. */
public object TreeFeature {
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: TreeConfiguration,
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
        if (clippedTreeHeight < treeHeight) return false
        if (config.rootPlacer != null && !config.rootPlacer.placeRoots(chunk, random, originX, originY, originZ, trunkOriginY)) return false

        val attachments = config.trunkPlacer.placeTrunk(chunk, random, clippedTreeHeight, originX, trunkOriginY, originZ, config.trunkStateId)
        val offset = config.foliagePlacer.offset(random)
        for (attachment in attachments) {
            config.foliagePlacer.createFoliage(chunk, random, config.foliageStateId, attachment, foliageHeight, leafRadius, offset)
        }
        return true
    }

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
                    if (!config.trunkPlacer.isFree(chunk, originX + x, originY + y, originZ + z)) return y - 2
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
    return chunk.getBlockStateId(x and 15, y, z and 15) == Blocks.AIR.stateId
}

private fun setBlock(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
    stateId: Int,
) {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return
    chunk.setBlockStateId(x and 15, y, z and 15, stateId)
}

private fun tryPlaceLeaf(
    chunk: Chunk,
    foliageStateId: Int,
    x: Int,
    y: Int,
    z: Int,
): Boolean {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return false
    if (chunk.getBlockStateId(x and 15, y, z and 15) == Blocks.AIR.stateId) {
        chunk.setBlockStateId(x and 15, y, z and 15, foliageStateId)
        return true
    }
    return false
}

// parity: real Direction.Plane.HORIZONTAL faces order (NORTH, EAST, SOUTH, WEST); index 0-3 of a
// single nextInt(4) draw, matching Util.getRandom(faces, random).
private val HORIZONTAL_DIRECTIONS = arrayOf(0 to -1, 1 to 0, 0 to 1, -1 to 0)
