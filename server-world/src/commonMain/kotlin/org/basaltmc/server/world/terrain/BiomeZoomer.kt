package org.basaltmc.server.world.terrain

/** // parity: BiomeManager.getBiome and its seed-fiddled three-dimensional Voronoi selection. */
public class BiomeZoomer(
    @PublishedApi internal val seed: Long,
) {
    public inline fun <T> biomeAt(
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        noiseBiomeAt: (quartX: Int, quartY: Int, quartZ: Int) -> T,
    ): T {
        val absoluteX = blockX - 2
        val absoluteY = blockY - 2
        val absoluteZ = blockZ - 2
        val parentX = absoluteX shr 2
        val parentY = absoluteY shr 2
        val parentZ = absoluteZ shr 2
        val fractionX = (absoluteX and 3) / 4.0
        val fractionY = (absoluteY and 3) / 4.0
        val fractionZ = (absoluteZ and 3) / 4.0
        var closestIndex = 0
        var closestDistance = Double.POSITIVE_INFINITY

        for (index in 0 until 8) {
            val lowerX = index and 4 == 0
            val lowerY = index and 2 == 0
            val lowerZ = index and 1 == 0
            val quartX = if (lowerX) parentX else parentX + 1
            val quartY = if (lowerY) parentY else parentY + 1
            val quartZ = if (lowerZ) parentZ else parentZ + 1
            val distance =
                fiddledDistance(
                    seed,
                    quartX,
                    quartY,
                    quartZ,
                    if (lowerX) fractionX else fractionX - 1.0,
                    if (lowerY) fractionY else fractionY - 1.0,
                    if (lowerZ) fractionZ else fractionZ - 1.0,
                )
            if (closestDistance > distance) {
                closestIndex = index
                closestDistance = distance
            }
        }

        return noiseBiomeAt(
            if (closestIndex and 4 == 0) parentX else parentX + 1,
            if (closestIndex and 2 == 0) parentY else parentY + 1,
            if (closestIndex and 1 == 0) parentZ else parentZ + 1,
        )
    }

    public companion object {
        @PublishedApi
        internal fun fiddledDistance(
            seed: Long,
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            distanceX: Double,
            distanceY: Double,
            distanceZ: Double,
        ): Double {
            var random = seed
            random = next(random, quartX.toLong())
            random = next(random, quartY.toLong())
            random = next(random, quartZ.toLong())
            random = next(random, quartX.toLong())
            random = next(random, quartY.toLong())
            random = next(random, quartZ.toLong())
            val offsetX = fiddle(random)
            random = next(random, seed)
            val offsetY = fiddle(random)
            random = next(random, seed)
            val offsetZ = fiddle(random)
            val x = distanceX + offsetX
            val y = distanceY + offsetY
            val z = distanceZ + offsetZ
            return x * x + y * y + z * z
        }

        private fun next(
            value: Long,
            addition: Long,
        ): Long = value * (value * MULTIPLIER + INCREMENT) + addition

        private fun fiddle(value: Long): Double {
            val shifted = value shr 24
            val remainder = shifted % 1024L
            val positive = if (remainder < 0L) remainder + 1024L else remainder
            return (positive / 1024.0 - 0.5) * 0.9
        }

        private const val MULTIPLIER = 6364136223846793005L
        private const val INCREMENT = 1442695040888963407L
    }
}
