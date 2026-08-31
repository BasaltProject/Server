package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the three firefly-bush patches. */
class FireflyBushOracle {
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
     * A grass bank beside a water channel. Two of the three specs only place next to water, so a
     * chunk without a shoreline would exercise nothing but their rejection.
     */
    private fun shoreChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, if (z in 7..8) Blocks.WATER.stateId else Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var bushes = 0
        for (y in 69..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == Blocks.FIREFLY_BUSH.stateId) bushes++
                }
            }
        }
        return checksum to bushes
    }

    private fun report(
        name: String,
        biome: String,
        body: (Chunk, FakeEnvironment) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var bushes = 0
        var deferred = 0
        for (index in 0 until 120) {
            val chunk = shoreChunk(index * 3 - 60, index * -5 + 20)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkBushes) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            bushes += chunkBushes
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name bushes=$bushes deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("swamp", "minecraft:swamp") { c, e -> FireflyBushPatchDecoration.decorate(c, 737373L, e) }
        report("plains", "minecraft:plains") { c, e -> FireflyBushPatchDecoration.decorate(c, 737373L, e) }
        // desert is in neither the swamp specs nor the 42-biome default list.
        report("wrong", "minecraft:desert") { c, e -> FireflyBushPatchDecoration.decorate(c, 737373L, e) }
        report("swampRarityOnly", "minecraft:swamp") { c, e ->
            FireflyBushPatchDecoration.decorateFeature(c, 737373L, e, "patch_firefly_bush_swamp")
        }
        report("nearWaterOnly", "minecraft:swamp") { c, e ->
            FireflyBushPatchDecoration.decorateFeature(c, 737373L, e, "patch_firefly_bush_near_water_swamp")
        }
        report("regional", "minecraft:plains") { c, e ->
            FireflyBushPatchDecoration.decorate(c, 737373L, e, externalBlockState = { _, y, z ->
                if (y in 66..69) {
                    Blocks.STONE.stateId
                } else if (y == 70) {
                    (if ((z and 15) in 7..8) Blocks.WATER.stateId else Blocks.GRASS_BLOCK.stateId)
                } else {
                    Blocks.AIR.stateId
                }
            })
        }
        // Without a shoreline the two near-water specs must place nothing at all.
        val dry = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) dry.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                dry.setBlockStateId(x, 70, z, Blocks.GRASS_BLOCK.stateId)
            }
        }
        FireflyBushPatchDecoration.decorateFeature(
            dry,
            737373L,
            FakeEnvironment("minecraft:swamp"),
            "patch_firefly_bush_near_water_swamp",
        )
        println("ORACLE dry=" + fold(dry).second)
    }
}
