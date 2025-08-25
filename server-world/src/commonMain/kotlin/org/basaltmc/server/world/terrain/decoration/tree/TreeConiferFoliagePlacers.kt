package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.IntProvider
import kotlin.math.max

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
