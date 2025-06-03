package org.basaltmc.server.world.noise

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.common.Mth

/**
 * // parity: net.minecraft.world.level.levelgen.synth.ImprovedNoise (classic Perlin noise, the
 * per-octave building block of [PerlinNoise]). [noiseWithDerivative] (used by density-function
 * gradient sampling) is not implemented yet: nothing in this project calls it.
 */
public class ImprovedNoise(
    random: BasaltRandom,
) {
    private val p = ByteArray(256)
    public val xo: Double = random.nextDouble() * 256.0
    public val yo: Double = random.nextDouble() * 256.0
    public val zo: Double = random.nextDouble() * 256.0

    init {
        for (i in 0 until 256) p[i] = i.toByte()
        for (i in 0 until 256) {
            val offset = random.nextInt(256 - i)
            val tmp = p[i]
            p[i] = p[i + offset]
            p[i + offset] = tmp
        }
    }

    public fun noise(
        x: Double,
        y: Double,
        z: Double,
    ): Double = noise(x, y, z, 0.0, 0.0)

    public fun noise(
        x: Double,
        y: Double,
        z: Double,
        yScale: Double,
        yFudge: Double,
    ): Double {
        val xx = x + xo
        val yy = y + yo
        val zz = z + zo
        val xf = Mth.floor(xx)
        val yf = Mth.floor(yy)
        val zf = Mth.floor(zz)
        val xr = xx - xf
        val yr = yy - yf
        val zr = zz - zf
        val yrFudge: Double =
            if (yScale != 0.0) {
                val fudgeLimit = if (yFudge >= 0.0 && yFudge < yr) yFudge else yr
                Mth.floor(fudgeLimit / yScale + SHIFT_UP_EPSILON) * yScale
            } else {
                0.0
            }
        return sampleAndLerp(xf, yf, zf, xr, yr - yrFudge, zr, yr)
    }

    private fun p(x: Int): Int = p[x and 0xFF].toInt() and 0xFF

    private fun sampleAndLerp(
        x: Int,
        y: Int,
        z: Int,
        xr: Double,
        yr: Double,
        zr: Double,
        yrOriginal: Double,
    ): Double {
        val x0 = p(x)
        val x1 = p(x + 1)
        val xy00 = p(x0 + y)
        val xy01 = p(x0 + y + 1)
        val xy10 = p(x1 + y)
        val xy11 = p(x1 + y + 1)
        val d000 = gradDot(p(xy00 + z), xr, yr, zr)
        val d100 = gradDot(p(xy10 + z), xr - 1.0, yr, zr)
        val d010 = gradDot(p(xy01 + z), xr, yr - 1.0, zr)
        val d110 = gradDot(p(xy11 + z), xr - 1.0, yr - 1.0, zr)
        val d001 = gradDot(p(xy00 + z + 1), xr, yr, zr - 1.0)
        val d101 = gradDot(p(xy10 + z + 1), xr - 1.0, yr, zr - 1.0)
        val d011 = gradDot(p(xy01 + z + 1), xr, yr - 1.0, zr - 1.0)
        val d111 = gradDot(p(xy11 + z + 1), xr - 1.0, yr - 1.0, zr - 1.0)
        val xAlpha = Mth.smoothstep(xr)
        val yAlpha = Mth.smoothstep(yrOriginal)
        val zAlpha = Mth.smoothstep(zr)
        return Mth.lerp3(xAlpha, yAlpha, zAlpha, d000, d100, d010, d110, d001, d101, d011, d111)
    }

    private companion object {
        val SHIFT_UP_EPSILON = 1.0E-7F.toDouble()

        fun gradDot(
            hash: Int,
            x: Double,
            y: Double,
            z: Double,
        ): Double = SimplexNoise.dot(SimplexNoise.GRADIENT[hash and 15], x, y, z)
    }
}
