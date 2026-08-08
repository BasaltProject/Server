package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.decoration.IntProvider
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/**
 * // parity: net.minecraft.world.level.levelgen.feature.TreeFeature plus the real trunk/foliage
 * placer and `FeatureSize` types it drives (`TrunkPlacer`/`StraightTrunkPlacer`/`ForkingTrunkPlacer`,
 * `FoliagePlacer`/`BlobFoliagePlacer`/`AcaciaFoliagePlacer`/`SpruceFoliagePlacer`/`PineFoliagePlacer`,
 * `TwoLayersFeatureSize`), all decompiled from the real jar. Coordinates are world block
 * coordinates throughout, matching [org.basaltmc.world.terrain.decoration.DecorationPos]'s own
 * convention; only [Chunk] reads/writes mask X/Z down to chunk-local `and 15`, the same established
 * simplification [org.basaltmc.world.terrain.decoration.PlainsDecoration]'s own pipeline already
 * uses (this project generates one chunk at a time with no cross-chunk write support yet, so a
 * canopy that would reach a neighboring chunk wraps back into this one instead of spilling over,
 * exactly like every other decoration feature here already does).
 *
 * **Documented gaps**, narrowing real vanilla's own `TreeFeature.place`:
 * - No root placer (mangrove-only), no tree decorators (beehives, vines, cocoa, leaf litter), no
 *   below-trunk-block provider (podzol/coarse dirt swaps under certain biomes' trees).
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
        val horizontalDirections =
            arrayOf(0 to 1, 0 to -1, 1 to 0, -1 to 0) // parity: Direction.Plane.HORIZONTAL order (N,S,W,E per real jar enum ordinals is N=2,S=3,W=4,E=5, but getRandomDirection picks uniformly so order doesn't affect distribution)
        val leanDirection = horizontalDirections[random.nextInt(4)]
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
        val branchDirection = horizontalDirections[random.nextInt(4)]
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
        val horizontalDirections = arrayOf(0 to 1, 0 to -1, 1 to 0, -1 to 0)
        val leanDirection = horizontalDirections[random.nextInt(4)]
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
public class GiantTrunkPlacer(
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

public class TreeConfiguration(
    public val trunkStateId: Int,
    public val trunkPlacer: TrunkPlacer,
    public val foliageStateId: Int,
    public val foliagePlacer: FoliagePlacer,
    public val minimumSize: FeatureSize,
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
        if (originY < WorldHeight.MIN_Y + 1 || originY + treeHeight + 1 > WorldHeight.MAX_Y + 1) return false

        val clippedTreeHeight = maxFreeTreeHeight(chunk, treeHeight, originX, originY, originZ, config)
        if (clippedTreeHeight < treeHeight) return false

        val attachments = config.trunkPlacer.placeTrunk(chunk, random, clippedTreeHeight, originX, originY, originZ, config.trunkStateId)
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
) {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return
    if (chunk.getBlockStateId(x and 15, y, z and 15) == Blocks.AIR.stateId) {
        chunk.setBlockStateId(x and 15, y, z and 15, foliageStateId)
    }
}
