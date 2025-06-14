package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.noise.SimplexNoise
import kotlin.math.ceil

/** A position a [PlacementModifier] pipeline is scattering, chunk-relative on X/Z. */
public data class DecorationPos(
    val x: Int,
    val y: Int,
    val z: Int,
)

/** // parity: net.minecraft.world.level.levelgen.placement.PlacementContext, narrowed to chunk heightmaps. */
public class PlacementContext(
    public val chunk: Chunk,
) {
    private val worldSurfaceHeightmap: IntArray = chunk.heightmap(Chunk.HeightmapType.WORLD_SURFACE)
    private val oceanFloorHeightmap: IntArray = chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR)
    private val motionBlockingHeightmap: IntArray = chunk.heightmap(Chunk.HeightmapType.MOTION_BLOCKING)
    private val motionBlockingNoLeavesHeightmap: IntArray =
        chunk.heightmap(
            Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES,
        )

    public fun height(
        localX: Int,
        localZ: Int,
        type: Chunk.HeightmapType = Chunk.HeightmapType.MOTION_BLOCKING,
    ): Int =
        when (type) {
            Chunk.HeightmapType.WORLD_SURFACE -> worldSurfaceHeightmap
            Chunk.HeightmapType.OCEAN_FLOOR -> oceanFloorHeightmap
            Chunk.HeightmapType.MOTION_BLOCKING -> motionBlockingHeightmap
            Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES -> motionBlockingNoLeavesHeightmap
        }[localX + localZ * 16]
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

/** // parity: net.minecraft.world.level.levelgen.placement.CountPlacement. */
public class CountPlacement(
    private val count: IntProvider,
) : PlacementModifier {
    public constructor(count: Int) : this(IntProvider.of(count))

    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> = (0 until count.sample(random)).asSequence().map { origin }
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

/** // parity: HeightRangePlacement with an inclusive UniformHeight provider. */
public class UniformHeightRangePlacement(
    private val minInclusive: Int,
    private val maxInclusive: Int,
) : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> =
        sequenceOf(origin.copy(y = minInclusive + random.nextInt(maxInclusive - minInclusive + 1)))
}

/** // parity: SurfaceRelativeThresholdFilter. */
public class SurfaceRelativeThresholdFilter(
    private val heightmap: Chunk.HeightmapType,
    private val minInclusive: Int = Int.MIN_VALUE,
    private val maxInclusive: Int = Int.MAX_VALUE,
) : PlacementFilter {
    override fun shouldPlace(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        val surfaceY = context.height(origin.x and 15, origin.z and 15, heightmap).toLong()
        return origin.y.toLong() in surfaceY + minInclusive..surfaceY + maxInclusive
    }
}

/** // parity: SurfaceWaterDepthFilter. */
public class SurfaceWaterDepthFilter(
    private val maxWaterDepth: Int,
) : PlacementFilter {
    override fun shouldPlace(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        val x = origin.x and 15
        val z = origin.z and 15
        val oceanFloorY = context.height(x, z, Chunk.HeightmapType.OCEAN_FLOOR)
        val worldSurfaceY = context.height(x, z, Chunk.HeightmapType.WORLD_SURFACE)
        return worldSurfaceY - oceanFloorY <= maxWaterDepth
    }
}

/** // parity: net.minecraft.world.level.levelgen.placement.HeightmapPlacement, this project's single heightmap standing in for every real heightmap type. */
public object HeightmapPlacement : PlacementModifier {
    public operator fun invoke(type: Chunk.HeightmapType): PlacementModifier =
        PlacementModifier { context, _, origin ->
            val height = context.height(origin.x and 15, origin.z and 15, type)
            sequenceOf(DecorationPos(origin.x, height, origin.z))
        }

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

/** // parity: NoiseBasedCountPlacement, using the fixed `Biome.BIOME_INFO_NOISE`. */
public class NoiseBasedCountPlacement(
    private val noiseToCountRatio: Int,
    private val noiseFactor: Double,
    private val noiseOffset: Double = 0.0,
) : PlacementModifier {
    override fun getPositions(
        context: PlacementContext,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Sequence<DecorationPos> {
        val noise = BIOME_INFO_NOISE.getValue(origin.x / noiseFactor, origin.z / noiseFactor)
        val count = ceil((noise + noiseOffset) * noiseToCountRatio).toInt()
        return (0 until count).asSequence().map { origin }
    }

    private companion object {
        val BIOME_INFO_NOISE = SimplexNoise(LegacyRandom(2345L))
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
        val bounds = context.chunk.verticalBounds
        if (origin.y !in bounds.minY..bounds.maxY) return false
        if (!Blocks.isAir(
                context.chunk.getBlockStateId(origin.x and 15, origin.y, origin.z and 15),
            )
        ) {
            return false
        }
        if (alsoCheckAbove) {
            if (origin.y + 1 > bounds.maxY) return false
            if (!Blocks.isAir(
                    context.chunk.getBlockStateId(
                        origin.x and 15,
                        origin.y + 1,
                        origin.z and 15,
                    ),
                )
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

/**
 * Runs [modifiers] from [origin] rather than from the chunk corner, and places the feature at every
 * position they hand back.
 *
 * This is what a placed feature is minus the modifiers that scatter it over a chunk: the ones that
 * shape the patch around a point stay, the ones that pick the point do not, because `/place feature`
 * was told where.
 */
public fun placeFrom(
    chunk: Chunk,
    random: WorldgenRandom,
    origin: DecorationPos,
    modifiers: List<PlacementModifier>,
    place: (DecorationPos) -> Unit,
) {
    val context = PlacementContext(chunk)
    var positions: Sequence<DecorationPos> = sequenceOf(origin)
    for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
    positions.forEach(place)
}
