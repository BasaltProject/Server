package org.basaltmc.server.world.climate

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.terrain.NoiseRouterData
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the spawn search. The router runs on the same synthetic noise
 * the other terrain oracles use, so what this pins is the spiral and the fitness, not Mojang's
 * numbers.
 */
class SpawnFinderOracle {
    private fun syntheticNoise(id: String): NormalNoise {
        var hash = -3750763034362895579L
        for (character in id) {
            hash = (hash xor character.code.toLong()) * 1099511628211L
        }
        val source = XoroshiroRandom(hash)
        return NormalNoise.create(source, -(id.length % 5 + 1), doubleArrayOf(1.0, 0.6, 0.3))
    }

    private fun base3d(): DensityFunction = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

    @Test
    fun dump() {
        val router = NoiseRouterData.overworld(::syntheticNoise, base3d())
        val sampler =
            Sampler(
                temperature = router.temperature,
                humidity = router.vegetation,
                continentalness = router.continents,
                erosion = router.erosion,
                depth = router.depth,
                weirdness = router.ridges,
            )
        val (x, z) = SpawnFinder.findSpawnPosition(sampler)
        println("ORACLE overworld x=$x z=$z")
        // One target only: the second weirdness band is what the two-entry list adds.
        val (singleX, singleZ) = SpawnFinder.findSpawnPosition(sampler, listOf(SpawnFinder.OVERWORLD_TARGET[0]))
        println("ORACLE single x=$singleX z=$singleZ")
        val (emptyX, emptyZ) = SpawnFinder.findSpawnPosition(sampler, emptyList())
        println("ORACLE empty x=$emptyX z=$emptyZ")
    }
}
