package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.world.noise.PerlinSimplexNoise
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the fixed-seed simplex octaves and the biome zoom. */
class BiomeOracle {
    @Test
    fun dump() {
        val single = PerlinSimplexNoise(LegacyRandom(1234L), listOf(0))
        val threeOctave = PerlinSimplexNoise(LegacyRandom(3456L), listOf(-2, -1, 0))
        for (i in 0 until 8) {
            val x = i * 13.5 - 40.0
            val y = i * -7.25 + 11.0
            println("ORACLE simplex1[$i]=" + single.getValue(x, y).toRawBits())
            println("ORACLE simplex3[$i]=" + threeOctave.getValue(x, y).toRawBits())
            println("ORACLE simplex3s[$i]=" + threeOctave.getValue(x, y, true).toRawBits())
        }

        val zoomer = BiomeZoomer(0x1234_5678_9ABCL)
        val picked = StringBuilder()
        for (i in 0 until 24) {
            val x = i * 37 - 400
            val y = i * 11 - 60
            val z = i * -53 + 300
            zoomer.biomeAt(x, y, z) { qx, qy, qz -> picked.append("$qx/$qy/$qz,") }
        }
        println("ORACLE zoom=$picked")

        val temps = StringBuilder()
        for (biome in listOf("minecraft:plains", "minecraft:frozen_ocean", "minecraft:desert", "minecraft:unknown")) {
            for (i in 0 until 6) {
                val x = i * 91 - 200
                val y = 40 + i * 30
                val z = i * -67 + 150
                temps.append(if (BiomeTemperature.coldEnoughToSnow(biome, x, y, z)) '1' else '0')
                temps.append(if (BiomeTemperature.shouldMeltFrozenOceanIcebergSlightly(biome, x, y, z)) '1' else '0')
            }
        }
        println("ORACLE temps=$temps")
    }
}
