package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.registry.isVineState
import org.basaltmc.server.registry.leafDistance
import org.basaltmc.server.registry.withLeafDistance
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.GeneratedBlockWritePolicy
import org.basaltmc.server.world.WorldHeight
import kotlin.math.max
import kotlin.math.min

/** // parity: FoliagePlacer.FoliageAttachment. Coordinates are world block coordinates. */
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

/** // parity: TreeFeature.place/doPlace/getMaxFreeTreeHeight. */
public object TreeFeature {
    internal fun isLogState(stateId: Int): Boolean = TreeBlockRules.isLogState(stateId)

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
    ): Boolean =
        chunk.withGeneratedBlockWritePolicy(GeneratedBlockWritePolicy.TREE) {
            placeConfigured(chunk, random, originX, originY, originZ, config, mossyCarpetRandom)
        }

    private fun placeConfigured(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: TreeConfiguration,
        mossyCarpetRandom: BasaltRandom,
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
            if (!isInside(pos)) continue

            // parity: the jar sets the distance and *then* fills the shape, and it never skips a
            // position that is already filled. A leaf reached at two distances is therefore written
            // twice, and the second write wins even when it is the larger number. That looks like a
            // bug and is one, but it is the jar's, and skipping the second write moves leaves.
            if (smallestDistance != 0) {
                val currentStateId = chunk.getDecorationBlockStateId(pos.x, pos.y, pos.z)
                val updatedStateId = Blocks.withLeafDistance(currentStateId, smallestDistance)
                if (updatedStateId != null) chunk.setDecorationBlockStateId(pos.x, pos.y, pos.z, updatedStateId)
            }
            filled.add(pos)

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

    internal data class BlockPos(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private enum class PlacementPhase { ROOT, LOG, LEAF, DECORATION }

    internal data class TreeDecoratorContext(
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

    internal val LOG_BLOCK_PATHS: Set<String> get() = TreeBlockRules.LOG_BLOCK_PATHS
    internal val BLOCK_NEIGHBOURS =
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

internal fun isValidTreePos(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Boolean {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return false
    return TreeBlockRules.isValidTreeState(chunk.getDecorationBlockStateId(x, y, z))
}

internal fun setBlock(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
    stateId: Int,
) {
    if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return
    chunk.setDecorationBlockStateId(x, y, z, stateId)
}

internal fun tryPlaceLeaf(
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
            if (selected?.properties?.containsKey("waterlogged") == true) {
                val waterlogged = hasSourceWaterFluid(current).toString()
                Blocks.stateIdOf(selected.block, selected.properties + ("waterlogged" to waterlogged)) ?: foliageStateId
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

internal fun canPlaceLeaf(
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

internal fun decorationHeightAt(
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
internal val HORIZONTAL_DIRECTIONS = arrayOf(0 to -1, 1 to 0, 0 to 1, -1 to 0)
