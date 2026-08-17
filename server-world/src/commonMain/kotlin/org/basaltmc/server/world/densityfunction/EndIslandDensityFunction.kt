package org.basaltmc.server.world.densityfunction

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.world.noise.SimplexNoise
import org.basaltmc.wire.common.Mth
import kotlin.math.abs
import kotlin.math.sqrt

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.EndIslandDensityFunction. The
 * End dimension's island shape: a big island at the origin plus smaller ones scattered by
 * [islandNoise], seeded off the world seed with a fixed [SKIPPED_RANDOM_CALLS] offset (matching
 * vanilla's own `LegacyRandomSource` skip before constructing the noise, part of the exact RNG
 * consumption order parity requires). Deliberately kept in `float`
 * throughout (not widened to `double`) to match vanilla's own arithmetic bit for bit: this
 * algorithm's rounding is part of its observable output, not an implementation detail.
 */
public class EndIslandDensityFunction(
    seed: Long,
) : DensityFunction {
    private val islandNoise: SimplexNoise

    init {
        val islandRandom = LegacyRandom(seed)
        islandRandom.consumeCount(SKIPPED_RANDOM_CALLS)
        islandNoise = SimplexNoise(islandRandom)
    }

    override fun compute(context: FunctionContext): Double =
        (heightValue(islandNoise, context.blockX / 8, context.blockZ / 8).toDouble() - 8.0) / 128.0

    override fun minValue(): Double = -0.84375

    override fun maxValue(): Double = 0.5625

    private companion object {
        const val SKIPPED_RANDOM_CALLS = 17292

        fun heightValue(
            islandNoise: SimplexNoise,
            sectionX: Int,
            sectionZ: Int,
        ): Float {
            val chunkX = sectionX / 2
            val chunkZ = sectionZ / 2
            val subSectionX = sectionX % 2
            val subSectionZ = sectionZ % 2
            var doffs = 100.0f - sqrt((sectionX * sectionX + sectionZ * sectionZ).toFloat()) * 8.0f
            doffs = Mth.clamp(doffs, -100.0f, 80.0f)

            for (xo in -12..12) {
                for (zo in -12..12) {
                    val totalChunkX = (chunkX + xo).toLong()
                    val totalChunkZ = (chunkZ + zo).toLong()
                    if (totalChunkX * totalChunkX + totalChunkZ * totalChunkZ > 4096L &&
                        islandNoise.getValue(totalChunkX.toDouble(), totalChunkZ.toDouble()) < -0.9
                    ) {
                        val islandSize =
                            (abs(totalChunkX.toFloat()) * 3439.0f + abs(totalChunkZ.toFloat()) * 147.0f) % 13.0f + 9.0f
                        val xd = (subSectionX - xo * 2).toFloat()
                        val zd = (subSectionZ - zo * 2).toFloat()
                        var newDoffs = 100.0f - sqrt(xd * xd + zd * zd) * islandSize
                        newDoffs = Mth.clamp(newDoffs, -100.0f, 80.0f)
                        doffs = maxOf(doffs, newDoffs)
                    }
                }
            }

            return doffs
        }
    }
}
