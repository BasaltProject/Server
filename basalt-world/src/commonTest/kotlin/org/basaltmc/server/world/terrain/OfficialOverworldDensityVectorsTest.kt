package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.densityfunction.Marker
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.terrain.carver.OverworldCarvers
import org.basaltmc.server.world.terrain.surface.SurfaceRuleData
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertEquals

/** Independent vectors captured from the official 26.2 RandomState router at noise-cell corners. */
class OfficialOverworldDensityVectorsTest {
    @Test
    fun fullCaveDensityMatchesOfficialSeedZeroExactly() {
        val density = fullRouter().finalDensity

        OFFICIAL_VECTORS.forEach { (position, expected) ->
            val (x, y, z) = position
            assertEquals(expected, density.compute(FunctionContext(x, y, z)), "at ($x, $y, $z)")
        }
    }

    @Test
    fun markerInterpolationProducesOfficialWorldSurfaceHeights() {
        val density = fullRouter().finalDensity
        val samplers = mutableMapOf<Pair<Int, Int>, DensityChunkSampler>()

        OFFICIAL_HEIGHTS.forEach { (position, expected) ->
            val (x, z) = position
            val key = (x shr 4) to (z shr 4)
            val sampler =
                samplers.getOrPut(key) {
                    DensityChunkSampler(density, key.first, key.second, WorldHeight.MIN_Y, WorldHeight.HEIGHT, 4, 8)
                }
            var y = WorldHeight.MAX_Y
            while (y >= WorldHeight.MIN_Y && sampler.sample(x and 15, y, z and 15) <= 0.0) y--
            // parity: the global Overworld fluid picker fills a base column through y=62.
            assertEquals(expected, (y + 1).coerceAtLeast(63), "at ($x, $z)")
        }
    }

    @Test
    fun aquiferMaterialCategoriesMatchOfficialSeedZeroChunkExactly() {
        val router = fullRouter()
        val chunkX = 10
        val chunkZ = 10
        val cache = DensityCache(chunkX, chunkZ, Marker.slotCount)
        val sampler =
            DensityChunkSampler(
                router.finalDensity,
                chunkX,
                chunkZ,
                WorldHeight.MIN_Y,
                WorldHeight.HEIGHT,
                4,
                8,
                cache,
            )
        val aquifer = OverworldAquifer(router, 0L, chunkX, chunkZ, WorldHeight.MIN_Y, WorldHeight.HEIGHT, cache)
        var hash = 1L
        var air = 0
        var water = 0
        var lava = 0
        var solid = 0
        for (z in 0..15) {
            for (x in 0..15) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val material =
                        aquifer.computeSubstance(
                            chunkX * 16 + x,
                            y,
                            chunkZ * 16 + z,
                            sampler.sample(x, y, z),
                        )
                    val category =
                        when (material?.stateId) {
                            null -> 3
                            0 -> 0
                            86 -> 1
                            102 -> 2
                            else -> error("unexpected aquifer material $material")
                        }
                    hash = 31L * hash + category
                    when (category) {
                        0 -> air++
                        1 -> water++
                        2 -> lava++
                        else -> solid++
                    }
                }
            }
        }
        assertEquals(listOf(68050, 6435, 17, 23802), listOf(air, water, lava, solid))
        assertEquals(-317959083457566686L, hash)
    }

    @Test
    fun oreVeinMaterialsMatchOfficialSeedZeroChunksExactly() {
        val router = fullRouter()
        assertMaterialAudit(
            router,
            chunkX = 10,
            chunkZ = 10,
            expectedHash = 1142789664808446590L,
            expectedCounts = listOf(68050, 6435, 17, 23727, 0, 0, 0, 15, 0, 60),
        )
        assertMaterialAudit(
            router,
            chunkX = 9,
            chunkZ = -10,
            expectedHash = -6711663254668721703L,
            expectedCounts = listOf(72400, 0, 0, 25523, 124, 3, 254, 0, 0, 0),
        )
    }

    @Test
    fun overworldCarversMatchOfficialSeedZeroChunkExactly() {
        val router = fullRouter()
        assertCarverAudit(
            router,
            chunkX = 10,
            chunkZ = 10,
            expectedHash = 7067105384118096359L,
            expectedCounts = listOf(68321, 6435, 17, 23456, 0, 0, 0, 15, 0, 60),
        )
        // This chunk includes a canyon; the first fixture exercises both cave configurations.
        assertCarverAudit(
            router,
            chunkX = 20,
            chunkZ = 20,
            expectedHash = 8001334642285277727L,
            expectedCounts = listOf(64152, 0, 0, 34152, 0, 0, 0, 0, 0, 0),
        )
    }

    private fun assertCarverAudit(
        router: OverworldNoiseRouter,
        chunkX: Int,
        chunkZ: Int,
        expectedHash: Long,
        expectedCounts: List<Int>,
    ) {
        val cache = DensityCache(chunkX, chunkZ, Marker.slotCount)
        val sampler =
            DensityChunkSampler(
                router.finalDensity,
                chunkX,
                chunkZ,
                WorldHeight.MIN_Y,
                WorldHeight.HEIGHT,
                4,
                8,
                cache,
            )
        val aquifer = OverworldAquifer(router, 0L, chunkX, chunkZ, WorldHeight.MIN_Y, WorldHeight.HEIGHT, cache)
        val oreVeins = OverworldOreVeinifier(router, 0L, cache)
        val chunk = Chunk(chunkX, chunkZ)
        for (z in 0..15) {
            for (x in 0..15) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val density = sampler.sample(x, y, z)
                    val material =
                        aquifer.computeSubstance(chunkX * 16 + x, y, chunkZ * 16 + z, density)
                            ?: oreVeins.compute(chunkX * 16 + x, y, chunkZ * 16 + z)
                            ?: Blocks.STONE
                    if (material != Blocks.AIR) chunk.setBlockStateId(x, y, z, material.stateId)
                }
            }
        }

        OverworldCarvers.carve(chunk, 0L, aquifer, UNUSED_SURFACE_ENVIRONMENT, SurfaceRuleData.overworld())

        var hash = 1L
        val counts = IntArray(10)
        for (z in 0..15) {
            for (x in 0..15) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val category =
                        materialCategory(
                            Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path ?: "air",
                        )
                    hash = 31L * hash + category
                    counts[category]++
                }
            }
        }
        assertEquals(expectedCounts, counts.toList(), "carved material counts at chunk ($chunkX, $chunkZ)")
        assertEquals(expectedHash, hash, "carved material hash at chunk ($chunkX, $chunkZ)")
    }

    private fun assertMaterialAudit(
        router: OverworldNoiseRouter,
        chunkX: Int,
        chunkZ: Int,
        expectedHash: Long,
        expectedCounts: List<Int>,
    ) {
        val cache = DensityCache(chunkX, chunkZ, Marker.slotCount)
        val sampler =
            DensityChunkSampler(
                router.finalDensity,
                chunkX,
                chunkZ,
                WorldHeight.MIN_Y,
                WorldHeight.HEIGHT,
                4,
                8,
                cache,
            )
        val aquifer = OverworldAquifer(router, 0L, chunkX, chunkZ, WorldHeight.MIN_Y, WorldHeight.HEIGHT, cache)
        val oreVeins = OverworldOreVeinifier(router, 0L, cache)
        var hash = 1L
        val counts = IntArray(10)
        for (z in 0..15) {
            for (x in 0..15) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val aquiferMaterial =
                        aquifer.computeSubstance(
                            chunkX * 16 + x,
                            y,
                            chunkZ * 16 + z,
                            sampler.sample(x, y, z),
                        )
                    val material = aquiferMaterial ?: oreVeins.compute(chunkX * 16 + x, y, chunkZ * 16 + z)
                    val category = materialCategory(material?.block?.path ?: "stone")
                    hash = 31L * hash + category
                    counts[category]++
                }
            }
        }
        assertEquals(expectedCounts, counts.toList(), "material counts at chunk ($chunkX, $chunkZ)")
        assertEquals(expectedHash, hash, "material hash at chunk ($chunkX, $chunkZ)")
    }

    private fun materialCategory(path: String): Int =
        when (path) {
            "air" -> 0
            "water" -> 1
            "lava" -> 2
            "stone" -> 3
            "copper_ore" -> 4
            "raw_copper_block" -> 5
            "granite" -> 6
            "deepslate_iron_ore" -> 7
            "raw_iron_block" -> 8
            "tuff" -> 9
            else -> error("unexpected base material $path")
        }

    private fun fullRouter(): OverworldNoiseRouter {
        val positional = XoroshiroRandom(0L).forkPositional()
        val noises =
            PARAMETERS.mapValues { (name, parameters) ->
                NormalNoise.create(
                    positional.fromHashOf("minecraft:$name"),
                    parameters.firstOctave,
                    parameters.amplitudes,
                )
            }
        val base3d = BlendedNoise(positional.fromHashOf("minecraft:terrain"), 0.25, 0.125, 80.0, 160.0, 8.0)
        return NoiseRouterData.overworld(noises::getValue, base3d)
    }

    private companion object {
        val UNUSED_SURFACE_ENVIRONMENT =
            object : SurfaceRuleEnvironment {
                override fun surfaceDepthNoise(
                    x: Int,
                    z: Int,
                ): Int = 0

                override fun surfaceSecondaryNoise(
                    x: Int,
                    z: Int,
                ): Double = 0.0

                override fun sample2d(
                    noiseId: String,
                    x: Int,
                    z: Int,
                ): Double = 0.0

                override fun sample3d(
                    noiseId: String,
                    x: Int,
                    y: Int,
                    z: Int,
                ): Double = 0.0

                override fun verticalGradientRandom(
                    name: String,
                    x: Int,
                    y: Int,
                    z: Int,
                ): Float = 0.0f

                override fun estimatedSurfaceHeight(
                    x: Int,
                    z: Int,
                ): Int = 0

                override fun biomeAt(
                    x: Int,
                    y: Int,
                    z: Int,
                ): String = "minecraft:plains"

                override fun isSteep(
                    x: Int,
                    z: Int,
                ): Boolean = false
            }

        data class Position(
            val x: Int,
            val y: Int,
            val z: Int,
        )

        data class Column(
            val x: Int,
            val z: Int,
        )

        val OFFICIAL_VECTORS =
            mapOf(
                Position(0, -64, 0) to 0.037482421875,
                Position(0, -56, 0) to 0.04569548483037882,
                Position(0, 0, 0) to 0.15719144891255343,
                Position(4, 0, 0) to 0.16343093955441174,
                Position(100, 64, -200) to 0.10400205320310593,
                Position(116, 64, 4) to -0.09686383702812938,
                Position(120, 56, 8) to 0.06368335792591898,
                Position(128, 32, 16) to 0.06017159682669846,
                Position(172, 80, 44) to -0.3244627210053509,
                Position(-100, -32, 204) to 0.07798460225511368,
            )

        val OFFICIAL_HEIGHTS =
            mapOf(
                Column(64, -16) to 73,
                Column(65, -16) to 73,
                Column(66, -16) to 74,
                Column(68, -16) to 74,
                Column(69, -16) to 75,
                Column(72, -16) to 75,
                Column(73, -16) to 64,
                Column(76, -16) to 64,
                Column(96, -16) to 63,
                Column(117, -16) to 63,
                Column(125, -16) to 65,
                Column(128, -16) to 65,
                Column(148, -16) to 67,
                Column(172, -16) to 66,
                Column(64, -8) to 71,
                Column(68, -4) to 69,
                Column(72, -4) to 71,
                Column(116, 4) to 63,
                Column(128, 8) to 63,
                Column(160, 24) to 64,
                Column(172, 32) to 64,
                Column(180, 48) to 63,
                Column(65, -15) to 73,
                Column(117, 7) to 63,
                Column(73, 13) to 63,
            )

        fun parameters(
            firstOctave: Int,
            vararg amplitudes: Double,
        ): NormalNoise.NoiseParameters = NormalNoise.NoiseParameters(firstOctave, amplitudes)

        // Test-only copies of the official 26.2 worldgen/noise entries used by this router.
        val PARAMETERS =
            mapOf(
                "offset" to parameters(-3, 1.0, 1.0, 1.0, 0.0),
                "continentalness" to parameters(-9, 1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0),
                "erosion" to parameters(-9, 1.0, 1.0, 0.0, 1.0, 1.0),
                "ridge" to parameters(-7, 1.0, 2.0, 1.0, 0.0, 0.0, 0.0),
                "jagged" to
                    parameters(-16, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0),
                "temperature" to parameters(-10, 1.5, 0.0, 1.0, 0.0, 0.0, 0.0),
                "vegetation" to parameters(-8, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0),
                "aquifer_barrier" to parameters(-3, 1.0),
                "aquifer_fluid_level_floodedness" to parameters(-7, 1.0),
                "aquifer_fluid_level_spread" to parameters(-5, 1.0),
                "aquifer_lava" to parameters(-1, 1.0),
                "ore_veininess" to parameters(-8, 1.0),
                "ore_vein_a" to parameters(-7, 1.0),
                "ore_vein_b" to parameters(-7, 1.0),
                "ore_gap" to parameters(-5, 1.0),
                "spaghetti_roughness" to parameters(-5, 1.0),
                "spaghetti_roughness_modulator" to parameters(-8, 1.0),
                "spaghetti_2d_thickness" to parameters(-11, 1.0),
                "spaghetti_3d_rarity" to parameters(-11, 1.0),
                "spaghetti_3d_thickness" to parameters(-8, 1.0),
                "spaghetti_3d_1" to parameters(-7, 1.0),
                "spaghetti_3d_2" to parameters(-7, 1.0),
                "cave_entrance" to parameters(-7, 0.4, 0.5, 1.0),
                "spaghetti_2d_modulator" to parameters(-11, 1.0),
                "spaghetti_2d" to parameters(-7, 1.0),
                "spaghetti_2d_elevation" to parameters(-8, 1.0),
                "pillar" to parameters(-7, 1.0, 1.0),
                "pillar_rareness" to parameters(-8, 1.0),
                "pillar_thickness" to parameters(-8, 1.0),
                "noodle" to parameters(-8, 1.0),
                "noodle_thickness" to parameters(-8, 1.0),
                "noodle_ridge_a" to parameters(-7, 1.0),
                "noodle_ridge_b" to parameters(-7, 1.0),
                "cave_layer" to parameters(-8, 1.0),
                "cave_cheese" to parameters(-8, 0.5, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 2.0, 0.0),
            )
    }
}
