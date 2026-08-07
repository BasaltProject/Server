package org.basaltmc.world.noise

import org.basaltmc.common.Mth
import org.basaltmc.random.BasaltRandom
import kotlin.math.sqrt

/**
 * // parity: net.minecraft.world.level.levelgen.synth.SimplexNoise. Only the 2D/3D value
 * functions [ImprovedNoise] itself needs ([GRADIENT], [dot]) plus the standalone 2D/3D simplex
 * value noise vanilla still exposes for a handful of callers (unused by anything in this project
 * yet, kept for parity completeness since the permutation table costs nothing extra to build).
 */
public class SimplexNoise(
    random: BasaltRandom,
) {
    private val p = IntArray(512)
    public val xo: Double = random.nextDouble() * 256.0
    public val yo: Double = random.nextDouble() * 256.0
    public val zo: Double = random.nextDouble() * 256.0

    init {
        for (i in 0 until 256) p[i] = i
        for (i in 0 until 256) {
            val offset = random.nextInt(256 - i)
            val tmp = p[i]
            p[i] = p[offset + i]
            p[offset + i] = tmp
        }
    }

    private fun p(x: Int): Int = p[x and 0xFF]

    public fun getValue(
        xin: Double,
        yin: Double,
    ): Double {
        val s = (xin + yin) * F2
        val i = Mth.floor(xin + s)
        val j = Mth.floor(yin + s)
        val t = (i + j) * G2
        val x0 = xin - (i - t)
        val y0 = yin - (j - t)
        val i1: Int
        val j1: Int
        if (x0 > y0) {
            i1 = 1
            j1 = 0
        } else {
            i1 = 0
            j1 = 1
        }
        val x1 = x0 - i1 + G2
        val y1 = y0 - j1 + G2
        val x2 = x0 - 1.0 + 2.0 * G2
        val y2 = y0 - 1.0 + 2.0 * G2
        val ii = i and 0xFF
        val jj = j and 0xFF
        val gi0 = p(ii + p(jj)) % 12
        val gi1 = p(ii + i1 + p(jj + j1)) % 12
        val gi2 = p(ii + 1 + p(jj + 1)) % 12
        val n0 = cornerNoise3d(gi0, x0, y0, 0.0, 0.5)
        val n1 = cornerNoise3d(gi1, x1, y1, 0.0, 0.5)
        val n2 = cornerNoise3d(gi2, x2, y2, 0.0, 0.5)
        return 70.0 * (n0 + n1 + n2)
    }

    public fun getValue(
        xin: Double,
        yin: Double,
        zin: Double,
    ): Double {
        val s = (xin + yin + zin) * F3
        val i = Mth.floor(xin + s)
        val j = Mth.floor(yin + s)
        val k = Mth.floor(zin + s)
        val t = (i + j + k) * G3
        val x0 = xin - (i - t)
        val y0 = yin - (j - t)
        val z0 = zin - (k - t)
        val i1: Int
        val j1: Int
        val k1: Int
        val i2: Int
        val j2: Int
        val k2: Int
        if (x0 >= y0) {
            if (y0 >= z0) {
                i1 = 1
                j1 = 0
                k1 = 0
                i2 = 1
                j2 = 1
                k2 = 0
            } else if (x0 >= z0) {
                i1 = 1
                j1 = 0
                k1 = 0
                i2 = 1
                j2 = 0
                k2 = 1
            } else {
                i1 = 0
                j1 = 0
                k1 = 1
                i2 = 1
                j2 = 0
                k2 = 1
            }
        } else if (y0 < z0) {
            i1 = 0
            j1 = 0
            k1 = 1
            i2 = 0
            j2 = 1
            k2 = 1
        } else if (x0 < z0) {
            i1 = 0
            j1 = 1
            k1 = 0
            i2 = 0
            j2 = 1
            k2 = 1
        } else {
            i1 = 0
            j1 = 1
            k1 = 0
            i2 = 1
            j2 = 1
            k2 = 0
        }
        val x1 = x0 - i1 + G3
        val y1 = y0 - j1 + G3
        val z1 = z0 - k1 + G3
        val x2 = x0 - i2 + 2.0 * G3
        val y2 = y0 - j2 + 2.0 * G3
        val z2 = z0 - k2 + 2.0 * G3
        val x3 = x0 - 1.0 + 3.0 * G3
        val y3 = y0 - 1.0 + 3.0 * G3
        val z3 = z0 - 1.0 + 3.0 * G3
        val ii = i and 0xFF
        val jj = j and 0xFF
        val kk = k and 0xFF
        val gi0 = p(ii + p(jj + p(kk))) % 12
        val gi1 = p(ii + i1 + p(jj + j1 + p(kk + k1))) % 12
        val gi2 = p(ii + i2 + p(jj + j2 + p(kk + k2))) % 12
        val gi3 = p(ii + 1 + p(jj + 1 + p(kk + 1))) % 12
        val n0 = cornerNoise3d(gi0, x0, y0, z0, 0.6)
        val n1 = cornerNoise3d(gi1, x1, y1, z1, 0.6)
        val n2 = cornerNoise3d(gi2, x2, y2, z2, 0.6)
        val n3 = cornerNoise3d(gi3, x3, y3, z3, 0.6)
        return 32.0 * (n0 + n1 + n2 + n3)
    }

    private fun cornerNoise3d(
        index: Int,
        x: Double,
        y: Double,
        z: Double,
        base: Double,
    ): Double {
        var t0 = base - x * x - y * y - z * z
        if (t0 < 0.0) return 0.0
        t0 *= t0
        return t0 * t0 * dot(GRADIENT[index], x, y, z)
    }

    public companion object {
        public val GRADIENT: Array<IntArray> =
            arrayOf(
                intArrayOf(1, 1, 0),
                intArrayOf(-1, 1, 0),
                intArrayOf(1, -1, 0),
                intArrayOf(-1, -1, 0),
                intArrayOf(1, 0, 1),
                intArrayOf(-1, 0, 1),
                intArrayOf(1, 0, -1),
                intArrayOf(-1, 0, -1),
                intArrayOf(0, 1, 1),
                intArrayOf(0, -1, 1),
                intArrayOf(0, 1, -1),
                intArrayOf(0, -1, -1),
                intArrayOf(1, 1, 0),
                intArrayOf(0, -1, 1),
                intArrayOf(-1, 1, 0),
                intArrayOf(0, -1, -1),
            )

        private val SQRT_3 = sqrt(3.0)
        private val F2 = 0.5 * (SQRT_3 - 1.0)
        private val G2 = (3.0 - SQRT_3) / 6.0
        private const val F3 = 1.0 / 3.0
        private const val G3 = 1.0 / 6.0

        public fun dot(
            g: IntArray,
            x: Double,
            y: Double,
            z: Double,
        ): Double = g[0] * x + g[1] * y + g[2] * z
    }
}
