package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isKelpState
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the eleven aquatic patches. */
class AquaticOracle {
    private inner class FakeEnvironment(
        private val biome: String,
    ) : SurfaceRuleEnvironment {
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
        ): Float = 0.5f

        override fun positionalRandom(
            x: Int,
            y: Int,
            z: Int,
        ): BasaltRandom = XoroshiroRandom(x * 31L + z)

        override fun estimatedSurfaceHeight(
            x: Int,
            z: Int,
        ): Int = 70

        override fun biomeAt(
            x: Int,
            y: Int,
            z: Int,
        ): String = biome

        override fun noiseBiomeAt(
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            cache: DensityCache?,
        ): String = biome

        override fun isSteep(
            x: Int,
            z: Int,
        ): Boolean = false

        override fun clayBand(
            x: Int,
            y: Int,
            z: Int,
        ): Int = 0
    }

    /**
     * A sea floor under a water column: every aquatic spec plants on the ocean floor and needs water
     * above it, so a dry chunk would exercise nothing but the water test.
     */
    private fun seaChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 40 until 50) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                // A magma stripe: seagrass and kelp both refuse it specifically.
                if (x % 7 == 0) chunk.setBlockStateId(x, 49, z, stateId("magma_block"))
                for (y in 50..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var plants = 0
        var kelp = 0
        for (y in 48..66) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.isKelpState(id) || id == Blocks.KELP_PLANT.stateId) {
                        kelp++
                    } else if (id != Blocks.WATER.stateId &&
                        id != Blocks.STONE.stateId &&
                        id != stateId("magma_block") &&
                        !Blocks.isAir(id)
                    ) {
                        plants++
                    }
                }
            }
        }
        return Triple(checksum, plants, kelp)
    }

    private fun report(
        name: String,
        biome: String,
        body: (Chunk, FakeEnvironment) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var plants = 0
        var kelp = 0
        var deferred = 0
        for (index in 0 until 80) {
            val chunk = seaChunk(index * 3 - 40, index * -5 + 15)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkPlants, chunkKelp) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            plants += chunkPlants
            kelp += chunkKelp
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name plants=$plants kelp=$kelp deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("warmOcean", "minecraft:warm_ocean") { c, e -> AquaticPatchDecoration.decorate(c, 5432L, e) }
        report("deepLukewarm", "minecraft:deep_lukewarm_ocean") { c, e ->
            AquaticPatchDecoration.decorate(c, 5432L, e)
        }
        report("ocean", "minecraft:ocean") { c, e -> AquaticPatchDecoration.decorate(c, 5432L, e) }
        report("river", "minecraft:river") { c, e -> AquaticPatchDecoration.decorate(c, 5432L, e) }
        report("swamp", "minecraft:swamp") { c, e -> AquaticPatchDecoration.decorate(c, 5432L, e) }
        // desert is in none of the eleven specs, so this is the real rejection case.
        report("wrong", "minecraft:desert") { c, e -> AquaticPatchDecoration.decorate(c, 5432L, e) }
        report("kelpColdOnly", "minecraft:ocean") { c, e ->
            AquaticPatchDecoration.decorateFeature(c, 5432L, e, "kelp_cold")
        }
        report("seaPickleOnly", "minecraft:warm_ocean") { c, e ->
            AquaticPatchDecoration.decorateFeature(c, 5432L, e, "sea_pickle")
        }
        report("regional", "minecraft:ocean") { c, e ->
            AquaticPatchDecoration.decorate(c, 5432L, e, externalBlockState = { _, y, _ ->
                if (y in 40..49) {
                    Blocks.STONE.stateId
                } else if (y in 50..62) {
                    Blocks.WATER.stateId
                } else {
                    Blocks.AIR.stateId
                }
            })
        }
    }
}
