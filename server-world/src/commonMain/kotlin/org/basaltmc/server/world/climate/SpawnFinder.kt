package org.basaltmc.server.world.climate

import org.basaltmc.wire.common.Mth
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/**
 * // parity: Climate.SpawnFinder: the column whose climate is closest to the dimension's
 * `spawn_target`, biased towards the origin. Two spirals, not a scan: the first walks out to 2048 in
 * steps of 512, the second re-walks 512 in steps of 32 from wherever the first landed.
 */
public object SpawnFinder {
    private const val MAX_RADIUS = 2048L

    /** // parity: `data/minecraft/worldgen/noise_settings/overworld.json`'s `spawn_target`. */
    public val OVERWORLD_TARGET: List<ParameterPoint> =
        listOf(
            ParameterPoint(
                temperature = Parameter.span(-1.0f, 1.0f),
                humidity = Parameter.span(-1.0f, 1.0f),
                continentalness = Parameter.span(-0.11f, 1.0f),
                erosion = Parameter.span(-1.0f, 1.0f),
                depth = Parameter.point(0.0f),
                weirdness = Parameter.span(-1.0f, -0.16f),
                offset = Parameter.quantizeCoord(0.0f),
            ),
            ParameterPoint(
                temperature = Parameter.span(-1.0f, 1.0f),
                humidity = Parameter.span(-1.0f, 1.0f),
                continentalness = Parameter.span(-0.11f, 1.0f),
                erosion = Parameter.span(-1.0f, 1.0f),
                depth = Parameter.point(0.0f),
                weirdness = Parameter.span(0.16f, 1.0f),
                offset = Parameter.quantizeCoord(0.0f),
            ),
        )

    private data class Candidate(
        val x: Int,
        val z: Int,
        val fitness: Long,
    )

    /** The block column the world spawn is measured from, x and z only. */
    public fun findSpawnPosition(
        sampler: Sampler,
        targets: List<ParameterPoint> = OVERWORLD_TARGET,
    ): Pair<Int, Int> {
        if (targets.isEmpty()) return 0 to 0
        var result = fitnessAt(sampler, targets, 0, 0)
        result = radialSearch(sampler, targets, result, maxRadius = 2048.0f, increment = 512.0f)
        result = radialSearch(sampler, targets, result, maxRadius = 512.0f, increment = 32.0f)
        return result.x to result.z
    }

    private fun radialSearch(
        sampler: Sampler,
        targets: List<ParameterPoint>,
        from: Candidate,
        maxRadius: Float,
        increment: Float,
    ): Candidate {
        var best = from
        var angle = 0.0f
        var radius = increment
        // parity: the spiral is anchored where this search *started*, not where it is winning, so
        // improving mid-spiral does not drag the remaining samples along with it.
        val originX = from.x
        val originZ = from.z
        while (radius <= maxRadius) {
            val x = originX + (sin(angle.toDouble()) * radius).toInt()
            val z = originZ + (cos(angle.toDouble()) * radius).toInt()
            val candidate = fitnessAt(sampler, targets, x, z)
            if (candidate.fitness < best.fitness) best = candidate
            angle += increment / radius
            if (angle > PI * 2) {
                angle = 0.0f
                radius += increment
            }
        }
        return best
    }

    /**
     * // parity: getSpawnPositionAndFitness. The sample is taken at y 0 and its depth is then
     * **overwritten with zero**, so the target's own depth span is what decides, not the terrain's.
     * The distance to the origin is added afterwards, scaled so that climate always outranks it.
     */
    private fun fitnessAt(
        sampler: Sampler,
        targets: List<ParameterPoint>,
        blockX: Int,
        blockZ: Int,
    ): Candidate {
        val sampled = sampler.sample(blockX shr 2, 0, blockZ shr 2)
        val flat =
            TargetPoint(
                sampled.temperature,
                sampled.humidity,
                sampled.continentalness,
                sampled.erosion,
                0L,
                sampled.weirdness,
            )
        var minFitness = Long.MAX_VALUE
        for (target in targets) minFitness = minOf(minFitness, target.fitness(flat))
        val distanceBias = Mth.square(blockX.toLong()) + Mth.square(blockZ.toLong())
        return Candidate(blockX, blockZ, minFitness * Mth.square(MAX_RADIUS) + distanceBias)
    }
}
