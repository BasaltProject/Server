package org.basaltmc.server.world.terrain.surface

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import kotlin.math.abs
import kotlin.math.sin
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the whole overworld rule tree. The environment is a pure
 * function of the position rather than real noise data, so both implementations see the same
 * inputs and what the dump pins is the tree's own shape and ordering.
 */
class SurfaceOracle {
    private val biomes =
        listOf(
            "minecraft:plains",
            "minecraft:badlands",
            "minecraft:eroded_badlands",
            "minecraft:wooded_badlands",
            "minecraft:desert",
            "minecraft:frozen_peaks",
            "minecraft:snowy_slopes",
            "minecraft:jagged_peaks",
            "minecraft:grove",
            "minecraft:stony_peaks",
            "minecraft:stony_shore",
            "minecraft:windswept_hills",
            "minecraft:windswept_savanna",
            "minecraft:windswept_gravelly_hills",
            "minecraft:mangrove_swamp",
            "minecraft:swamp",
            "minecraft:mushroom_fields",
            "minecraft:ice_spikes",
            "minecraft:frozen_ocean",
            "minecraft:deep_frozen_ocean",
            "minecraft:warm_ocean",
            "minecraft:beach",
            "minecraft:snowy_beach",
            "minecraft:dripstone_caves",
            "minecraft:sulfur_caves",
            "minecraft:old_growth_pine_taiga",
        )

    /** A deterministic stand-in for real noise: a pure function of the name and position. */
    private fun fake(
        name: String,
        x: Int,
        y: Int,
        z: Int,
    ): Double {
        var hash = -3750763034362895579L
        for (character in name) hash = (hash xor character.code.toLong()) * 1099511628211L
        hash = (hash xor x.toLong()) * 1099511628211L
        hash = (hash xor y.toLong()) * 1099511628211L
        hash = (hash xor z.toLong()) * 1099511628211L
        return sin(hash.toDouble() * 1.0E-17)
    }

    private inner class FakeEnvironment : SurfaceRuleEnvironment {
        override val biomeZoomSeed: Long? = null

        override fun surfaceDepthNoise(
            x: Int,
            z: Int,
        ): Int = ((x * 31 + z * 17) and 7) - 2

        override fun surfaceSecondaryNoise(
            x: Int,
            z: Int,
        ): Double = fake("secondary", x, 0, z)

        override fun sample2d(
            noiseId: String,
            x: Int,
            z: Int,
        ): Double = fake(noiseId, x, 0, z)

        override fun sample3d(
            noiseId: String,
            x: Int,
            y: Int,
            z: Int,
        ): Double = fake(noiseId, x, y, z)

        override fun verticalGradientRandom(
            name: String,
            x: Int,
            y: Int,
            z: Int,
        ): Float = (abs(fake(name, x, y, z))).toFloat()

        override fun positionalRandom(
            x: Int,
            y: Int,
            z: Int,
        ): BasaltRandom = XoroshiroRandom(x * 31L + z)

        override fun estimatedSurfaceHeight(
            x: Int,
            z: Int,
        ): Int = 64 + ((x * 7 + z * 13) and 31)

        override fun biomeAt(
            x: Int,
            y: Int,
            z: Int,
        ): String = biomes[(((x * 3 + z * 5 + y) % biomes.size) + biomes.size) % biomes.size]

        override fun noiseBiomeAt(
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            cache: org.basaltmc.server.world.densityfunction.DensityCache?,
        ): String = biomeAt(quartX shl 2, quartY shl 2, quartZ shl 2)

        override fun isSteep(
            x: Int,
            z: Int,
        ): Boolean = ((x xor z) and 3) == 0

        override fun clayBand(
            x: Int,
            y: Int,
            z: Int,
        ): Int = 1000 + (((y * 7 + x) % 13) + 13) % 13
    }

    @Test
    fun dump() {
        val environment = FakeEnvironment()
        val context = SurfaceRuleContext(environment, WorldGenerationContext(-64, 384))
        val rule = SurfaceRuleData.overworld().apply(context)
        val results = StringBuilder()
        var matched = 0
        for (i in 0 until 300) {
            val x = i * 13 - 500
            val z = i * -29 + 700
            context.updateXZ(x, z)
            for (step in 0 until 5) {
                val y = -60 + step * 40 + (i % 7)
                context.updateY(1 + (i % 4), 1 + (step % 3), if (step % 2 == 0) 70 else Int.MIN_VALUE, y)
                val applied = rule.tryApply(x, y, z)
                results.append(applied ?: -1).append(',')
                if (applied != null) matched++
            }
        }
        println("ORACLE surface.matched=$matched")
        println("ORACLE surface.results=$results")

        val bands = BadlandsClayBands(XoroshiroRandom(987654321L))
        println("ORACLE bands=" + bands.copyStateIds().joinToString(","))
    }
}
