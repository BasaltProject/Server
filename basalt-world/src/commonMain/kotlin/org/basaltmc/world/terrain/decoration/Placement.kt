package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.Chunk
import org.basaltmc.world.noise.SimplexNoise

/** A position a [PlacementModifier] pipeline is scattering, chunk-relative on X/Z. */
public data class DecorationPos(
    val x: Int,
    val y: Int,
    val z: Int,
)

/** // parity: net.minecraft.world.level.levelgen.placement.PlacementContext, narrowed to what this project's single-heightmap [Chunk] model needs. */
public class PlacementContext(
    public val chunk: Chunk,
) {
    private val heightmap: IntArray = chunk.heightmap()

    /** Real vanilla distinguishes OCEAN_FLOOR/MOTION_BLOCKING/WORLD_SURFACE(_WG); this project has no fluids yet, so all four coincide. */
    public fun height(
        localX: Int,
        localZ: Int,
    ): Int = heightmap[localX + localZ * 16]
}

/** // parity: net.minecraft.world.level.levelgen.placement.PlacementModifier. Each modifier turns one position into zero or more. */
public fun interface PlacementModifier {
    public fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos>
}

/** // parity: net.minecraft.world.level.levelgen.placement.PlacementFilter. */
public fun interface PlacementFilter : PlacementModifier {
    public fun shouldPlace(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean

    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> = if (shouldPlace(context, random, origin)) sequenceOf(origin) else emptySequence()
}

/** // parity: net.minecraft.world.level.levelgen.placement.CountPlacement, constant-count case only (this project's Plains list never uses a weighted count). */
public class CountPlacement(
    private val count: Int,
) : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> = (0 until count).asSequence().map { origin }
}

/** // parity: net.minecraft.world.level.levelgen.placement.InSquarePlacement. Scatters X/Z uniformly within the 16x16 chunk. */
public object InSquarePlacement : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> {
        val x = random.nextInt(16) + origin.x
        val z = random.nextInt(16) + origin.z
        return sequenceOf(DecorationPos(x, origin.y, z))
    }
}

/**
 * // parity: net.minecraft.world.level.levelgen.placement.RandomOffsetPlacement, the symmetric
 * zero-plateau trapezoid case only (every Plains placement uses `plateau: 0` with `min == -max`,
 * the real jar's own `TrapezoidInt.sample` reduces to `nextInt(max+1) - nextInt(max+1)` exactly in
 * that case; the general trapezoid formula is not ported).
 */
public class RandomOffsetPlacement(
    private val xzSpread: Int,
    private val ySpread: Int,
) : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> {
        fun trapezoid(range: Int) = if (range == 0) 0 else random.nextInt(range + 1) - random.nextInt(range + 1)
        val x = origin.x + trapezoid(xzSpread)
        val y = origin.y + trapezoid(ySpread)
        val z = origin.z + trapezoid(xzSpread)
        return sequenceOf(DecorationPos(x, y, z))
    }
}

/** // parity: net.minecraft.world.level.levelgen.placement.HeightmapPlacement, this project's single heightmap standing in for every real heightmap type. */
public object HeightmapPlacement : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> {
        val localX = origin.x and 15
        val localZ = origin.z and 15
        val height = context.height(localX, localZ)
        return sequenceOf(DecorationPos(origin.x, height, origin.z))
    }
}

/**
 * // parity: net.minecraft.world.level.levelgen.placement.NoiseThresholdCountPlacement, backed by
 * the real `Biome.BIOME_INFO_NOISE` (a single-octave `SimplexNoise` seeded off the fixed legacy
 * seed `2345L`, confirmed from the real jar's static initializer, not the world seed) sampled at
 * `(x / 200.0, z / 200.0)`.
 */
public class NoiseThresholdCountPlacement(
    private val noiseLevel: Double,
    private val belowNoise: Int,
    private val aboveNoise: Int,
) : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> {
        val noiseValue = BIOME_INFO_NOISE.getValue(origin.x / 200.0, origin.z / 200.0)
        val count = if (noiseValue < noiseLevel) belowNoise else aboveNoise
        return (0 until count).asSequence().map { origin }
    }

    private companion object {
        val BIOME_INFO_NOISE = SimplexNoise(LegacyRandom(2345L))
    }
}

/** // parity: net.minecraft.world.level.levelgen.placement.RarityFilter. */
public class RarityFilter(
    private val chance: Int,
) : PlacementFilter {
    override fun shouldPlace(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean = random.nextFloat() < 1.0f / chance
}

/**
 * // parity: net.minecraft.world.level.levelgen.placement.BlockPredicateFilter, narrowed to the
 * one predicate every Plains vegetal-decoration feature actually needs: the target position (and,
 * for double-height plants, the block above it) must be air. Real vanilla's general
 * `BlockPredicate` DSL (tag matching, `would_survive`, tests at an offset...) is not ported.
 */
public class AirTargetFilter(
    private val alsoCheckAbove: Boolean = false,
) : PlacementFilter {
    override fun shouldPlace(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        if (origin.y !in org.basaltmc.world.WorldHeight.MIN_Y..org.basaltmc.world.WorldHeight.MAX_Y) return false
        val airStateId = org.basaltmc.registry.Blocks.AIR.stateId
        if (context.chunk.getBlockStateId(origin.x and 15, origin.y, origin.z and 15) != airStateId) return false
        if (alsoCheckAbove) {
            if (origin.y + 1 > org.basaltmc.world.WorldHeight.MAX_Y) return false
            if (context.chunk.getBlockStateId(origin.x and 15, origin.y + 1, origin.z and 15) !=
                airStateId
            ) {
                return false
            }
        }
        return true
    }
}

/**
 * // parity: net.minecraft.data.worldgen.placement.PlacementUtils.countExtra, reduced to its own
 * real two-outcome weighted list: [count] with weight `1/chance - 1`, `count + extra` with weight
 * `1`. `random.nextInt(weight)` landing on the last slot (index `weight - 1`) picks the rarer
 * `count + extra` outcome, matching real vanilla's own insertion-order cumulative-weight walk.
 */
public class CountExtraPlacement(
    private val count: Int,
    private val chance: Float,
    private val extra: Int,
) : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> {
        val weight = (1.0f / chance).toInt()
        val n = if (random.nextInt(weight) == weight - 1) count + extra else count
        return (0 until n).asSequence().map { origin }
    }
}
