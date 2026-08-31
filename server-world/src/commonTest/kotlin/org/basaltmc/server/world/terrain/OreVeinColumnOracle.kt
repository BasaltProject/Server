package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.noise.NormalNoise
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the ore veins and the column heights. The noise parameters
 * are the same synthetic set the router oracle uses, so what this pins is the two algorithms and
 * not Mojang's numbers.
 */
class OreVeinColumnOracle {
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
        val veinifier = OverworldOreVeinifier(router, 5417L)
        var checksum = -3750763034362895579L
        var replaced = 0
        val counts = HashMap<String, Int>()
        // A dense block rather than a strided walk: the positions that pass the veininess threshold
        // are clustered, and only inside a cluster does the ore branch ever beat the filler.
        for (x in -112..112) {
            for (z in -112..112) {
                for (y in listOf(-40, -30, -20, -12, 6, 14, 22, 30, 44)) {
                    val state = veinifier.compute(x, y, z)
                    val id = state?.stateId ?: -1
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (state != null) {
                        replaced++
                        val path = state.block.path
                        counts[path] = (counts[path] ?: 0) + 1
                    }
                }
            }
        }
        val summary = counts.keys.sorted().joinToString(",") { "$it=${counts[it]}" }
        println("ORACLE veins replaced=$replaced checksum=$checksum counts=$summary")

        val heights = NoiseColumnHeights(router)
        var heightChecksum = -3750763034362895579L
        var surfaceSum = 0L
        var floorSum = 0L
        var solid = 0
        for (index in 0 until 128) {
            val x = (index * 53).mod(384) - 192
            val z = (index * 29).mod(384) - 192
            val surface = heights.surfaceHeight(x, z)
            val floor = heights.oceanFloorHeight(x, z)
            surfaceSum += surface
            floorSum += floor
            if (heights.isSolid(x, surface - 1, z)) solid++
            heightChecksum = (heightChecksum xor surface.toLong()) * 1099511628211L
            heightChecksum = (heightChecksum xor floor.toLong()) * 1099511628211L
        }
        println("ORACLE heights surface=$surfaceSum floor=$floorSum solid=$solid checksum=$heightChecksum")
    }
}
