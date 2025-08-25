package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.IntProvider
import kotlin.math.abs
import kotlin.math.max

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

// parity: RootPlacer + MangroveRootPlacer, the only root placer vanilla has.
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
