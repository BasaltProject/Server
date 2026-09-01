package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the two top-level generators: the Overworld pipeline and the
 * Nether and End ones. What this pins is the order of the phases and what each hands the next.
 */
class TerrainPipelineOracle {
    /** FNV-1a over a name, folded small: a stable per-noise offset both implementations reproduce. */
    private fun nameOffset(name: String): Int {
        var hash = -3750763034362895579L
        for (character in name) hash = (hash xor character.code.toLong()) * 1099511628211L
        return ((hash ushr 32).toInt() and 0x7FFFFFFF) % 97
    }

    private val biomeNames =
        listOf("minecraft:desert", "minecraft:jungle", "minecraft:snowy_plains", "minecraft:swamp")

    /**
     * An environment whose every answer moves with position. A flat one leaves the surface rules,
     * the steepness test and most of decoration with nothing to branch on, and then the phases can
     * be reordered without the result changing.
     */
    private class VaryingEnvironment(
        private val offsets: (String) -> Int,
        private val biomes: List<String>,
    ) : SurfaceRuleEnvironment {
        override fun surfaceDepthNoise(
            x: Int,
            z: Int,
        ): Int = (x * 7 + z * 3).mod(5) - 2

        override fun surfaceSecondaryNoise(
            x: Int,
            z: Int,
        ): Double = (x * 3 - z).mod(11) / 10.0 - 0.5

        override fun sample2d(
            noiseId: String,
            x: Int,
            z: Int,
        ): Double = (offsets(noiseId) + x * 5 + z * 7).mod(21) / 10.0 - 1.0

        override fun sample3d(
            noiseId: String,
            x: Int,
            y: Int,
            z: Int,
        ): Double = (offsets(noiseId) + x * 5 + y * 11 + z * 7).mod(21) / 10.0 - 1.0

        override fun verticalGradientRandom(
            name: String,
            x: Int,
            y: Int,
            z: Int,
        ): Float = (offsets(name) + x + y * 3 + z * 5).mod(100) / 100.0f

        override fun positionalRandom(
            x: Int,
            y: Int,
            z: Int,
        ): BasaltRandom = XoroshiroRandom(x * 31L + y * 17L + z)

        override fun estimatedSurfaceHeight(
            x: Int,
            z: Int,
        ): Int = 62 + (x * 2 + z).mod(9)

        override fun biomeAt(
            x: Int,
            y: Int,
            z: Int,
        ): String = biomes[((x shr 2) + (y shr 2) + (z shr 2)).mod(biomes.size)]

        override fun noiseBiomeAt(
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            cache: DensityCache?,
        ): String = biomes[(quartX + quartY + quartZ).mod(biomes.size)]

        override fun isSteep(
            x: Int,
            z: Int,
        ): Boolean = false

        override fun clayBand(
            x: Int,
            y: Int,
            z: Int,
        ): Int = (x + y + z).mod(4)
    }

    /**
     * A rolling floor around y 60. Interpolated on purpose: without the marker the sampler
     * evaluates every block and the cell sizes a dimension's settings choose stop mattering.
     */
    private fun terrainDensity(
        seed: Long,
        low: Int,
        high: Int,
    ): DensityFunction {
        val noise = NormalNoise.create(XoroshiroRandom(seed), -6, doubleArrayOf(1.0, 1.0))
        return DensityFunctions.interpolated(
            DensityFunctions.add(
                DensityFunctions.yClampedGradient(low, high, 1.0, -1.0),
                DensityFunctions.mul(DensityFunctions.constant(2.0), DensityFunctions.noise(noise, 0.6, 0.3)),
            ),
        )
    }

    private fun overworldRouter(seed: Long): OverworldNoiseRouter {
        val zero = DensityFunctions.zero()
        return OverworldNoiseRouter(
            barrierNoise = zero,
            fluidLevelFloodednessNoise = DensityFunctions.constant(-0.2),
            fluidLevelSpreadNoise = zero,
            lavaNoise = zero,
            shiftX = zero,
            shiftZ = zero,
            temperature = zero,
            vegetation = zero,
            continents = zero,
            erosion = zero,
            depth = zero,
            ridges = zero,
            preliminarySurfaceLevel = DensityFunctions.constant(64.0),
            finalDensity = terrainDensity(seed, 40, 90),
            veinToggle = DensityFunctions.yClampedGradient(-60, 60, 1.0, -1.0),
            veinRidged = DensityFunctions.constant(1.0),
            veinGap = DensityFunctions.constant(-0.5),
        )
    }

    private fun fold(
        chunk: Chunk,
        lowY: Int,
        highY: Int,
    ): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var solid = 0
        for (y in lowY..highY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != org.basaltmc.server.registry.Blocks.AIR.stateId) solid++
                }
            }
        }
        var biomes = 0
        for (quartY in 0 until chunk.verticalBounds.height / 4) {
            for (quartX in 0 until 4) {
                for (quartZ in 0 until 4) {
                    val id = chunk.getBiomeId(quartX, quartY, quartZ)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != 0) biomes++
                }
            }
        }
        return Triple(checksum, solid, biomes)
    }

    private fun reportOverworld(
        name: String,
        seed: Long,
        rotation: Int,
    ) {
        val router = overworldRouter(seed)
        val environment =
            VaryingEnvironment(::nameOffset, biomeNames.drop(rotation) + biomeNames.take(rotation))
        var checksum = -3750763034362895579L
        var solid = 0
        var biomes = 0
        for (index in 0 until 4) {
            val chunk = RealTerrainGenerator.generate(router, environment, index % 2, index / 2, seed)
            val (chunkChecksum, chunkSolid, chunkBiomes) = fold(chunk, -64, 200)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            solid += chunkSolid
            biomes += chunkBiomes
        }
        println("ORACLE $name solid=$solid biomes=$biomes checksum=$checksum")
    }

    private fun reportNether(
        name: String,
        seed: Long,
        temperature: Double,
        vegetation: Double,
    ) {
        val router =
            NetherNoiseRouter(
                DensityFunctions.constant(temperature),
                DensityFunctions.constant(vegetation),
                terrainDensity(seed, 0, 100),
            )
        val biomeSource = NetherBiomeSource(router.temperature, router.vegetation)
        var checksum = -3750763034362895579L
        var solid = 0
        var biomes = 0
        for (index in 0 until 4) {
            val chunk =
                DimensionTerrainGenerator.nether(
                    router,
                    biomeSource,
                    index % 2,
                    index / 2,
                    VaryingEnvironment(::nameOffset, listOf("minecraft:nether_wastes")),
                    seed,
                )
            val (chunkChecksum, chunkSolid, chunkBiomes) = fold(chunk, 0, 128)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            solid += chunkSolid
            biomes += chunkBiomes
        }
        println("ORACLE $name solid=$solid biomes=$biomes checksum=$checksum")
    }

    private fun reportEnd(
        name: String,
        seed: Long,
    ) {
        val router = EndNoiseRouter(DensityFunctions.constant(0.0), terrainDensity(seed, 0, 100))
        val biomeSource = TheEndBiomeSource(router.erosion)
        var checksum = -3750763034362895579L
        var solid = 0
        var biomes = 0
        for (index in 0 until 4) {
            val chunk = DimensionTerrainGenerator.end(router, biomeSource, index % 2, index / 2, seed)
            val (chunkChecksum, chunkSolid, chunkBiomes) = fold(chunk, 0, 128)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            solid += chunkSolid
            biomes += chunkBiomes
        }
        println("ORACLE $name solid=$solid biomes=$biomes checksum=$checksum")
    }

    @Test
    fun dump() {
        println(
            "ORACLE plainsId id=${Biomes.REGISTRY.idOf(
                org.basaltmc.wire.common.Identifier
                    .vanilla("plains"),
            ) ?: -1}",
        )
        reportOverworld("plains", 1234L, 0)
        reportOverworld("desert", 1234L, 1)
        reportOverworld("otherSeed", 77L, 0)
        reportNether("nether", 1234L, 0.0, 0.0)
        reportNether("netherOther", 77L, 0.4, 0.0)
        reportEnd("end", 1234L)
        reportEnd("endOther", 77L)
    }
}
