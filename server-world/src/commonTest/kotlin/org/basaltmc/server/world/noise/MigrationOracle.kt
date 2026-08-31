package org.basaltmc.server.world.noise

import org.basaltmc.server.random.XoroshiroRandom
import kotlin.test.Test
import kotlin.time.TimeSource

/**
 * The frozen Kotlin server acting as the second parity oracle, next to the official jar. Prints the
 * raw bits of a fixed set of noise samples, plus the density-sampling timing the C++ side is
 * measured against. Values are printed as bit patterns because a decimal hides the last few bits,
 * and worldgen notices them.
 *
 * Run the release binary for the timing to mean anything:
 * `./gradlew :server-world:linkReleaseTestMacosArm64` then run `test.kexe --ktest_filter=*MigrationOracle*`.
 */
class MigrationOracle {
    @Test
    fun dump() {
        val amplitudes = doubleArrayOf(1.0, 1.0, 1.0, 1.0)
        val improved = ImprovedNoise(XoroshiroRandom(42L))
        println("ORACLE improved.xo=" + improved.xo.toRawBits())
        println("ORACLE improved.yo=" + improved.yo.toRawBits())
        println("ORACLE improved.zo=" + improved.zo.toRawBits())
        for (i in 0 until 6) {
            val v = improved.noise(i * 1.7, i * 0.3 + 2.0, i * -2.9)
            println("ORACLE improved[$i]=" + v.toRawBits() + " " + v)
        }

        val perlin = PerlinNoise.create(XoroshiroRandom(7L), -4, amplitudes)
        for (i in 0 until 6) {
            val v = perlin.getValue(i * 13.5, i * 4.25, i * -7.75)
            println("ORACLE perlin[$i]=" + v.toRawBits() + " " + v)
        }
        println("ORACLE perlin.max=" + perlin.maxValue().toRawBits())

        val normal = NormalNoise.create(XoroshiroRandom(123456789L), -3, doubleArrayOf(1.0, 1.0, 1.0))
        for (i in 0 until 6) {
            val v = normal.getValue(i * 11.0, i * 3.0, i * -5.0)
            println("ORACLE normal[$i]=" + v.toRawBits() + " " + v)
        }
        println("ORACLE normal.max=" + normal.maxValue.toRawBits())

        val simplex = SimplexNoise(XoroshiroRandom(99L))
        for (i in 0 until 4) {
            val v2 = simplex.getValue(i * 1.5, i * -2.5)
            val v3 = simplex.getValue(i * 1.5, i * -2.5, i * 0.75)
            println("ORACLE simplex2[$i]=" + v2.toRawBits())
            println("ORACLE simplex3[$i]=" + v3.toRawBits())
        }

        // The M0 measurement: a dense grid of NormalNoise samples, the shape a density function has.
        val bench = NormalNoise.create(XoroshiroRandom(1L), -7, DoubleArray(8) { 1.0 })
        var sink = 0.0
        for (warm in 0 until 2) {
            sink = 0.0
            for (x in 0 until 64) {
                for (y in 0 until 64) {
                    for (z in 0 until 64) {
                        sink += bench.getValue(x * 0.31, y * 0.31, z * 0.31)
                    }
                }
            }
        }
        val start = TimeSource.Monotonic.markNow()
        var iterations = 0
        while (start.elapsedNow().inWholeMilliseconds < 3000) {
            for (x in 0 until 64) {
                for (y in 0 until 64) {
                    for (z in 0 until 64) {
                        sink += bench.getValue(x * 0.31, y * 0.31, z * 0.31)
                    }
                }
            }
            iterations++
        }
        val elapsed = start.elapsedNow()
        val samples = iterations.toLong() * 64L * 64L * 64L
        println("ORACLE bench samples=$samples ms=${elapsed.inWholeMilliseconds} nsPerSample=${elapsed.inWholeNanoseconds.toDouble() / samples} sink=$sink")
    }
}
