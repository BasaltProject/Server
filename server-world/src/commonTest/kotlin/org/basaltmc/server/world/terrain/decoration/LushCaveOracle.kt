package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the lush cave vegetation step. */
class LushCaveOracle {
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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val cavernBottoms = listOf(-40, -12, 16)
    private val cavernHeight = 10

    /** Stone caverns with a water pool and a dirt band, which is what the clay and moss want. */
    private fun caveChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                for (y in -64 until 40) {
                    val block = if ((worldX * 3 + y * 5 + worldZ).mod(17) == 0) "dirt" else "stone"
                    chunk.setBlockStateId(x, y, z, stateId(block))
                }
                for (bottom in cavernBottoms) {
                    for (y in bottom until bottom + cavernHeight) {
                        // A shallow pool on one cavern floor: the clay patch only forms under water.
                        val flooded = bottom == -12 && y < bottom + 2
                        chunk.setBlockStateId(x, y, z, if (flooded) Blocks.WATER.stateId else Blocks.AIR.stateId)
                    }
                }
            }
        }
        return chunk
    }

    private val ignored = setOf("stone", "dirt", "air", "water")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in -64 until 40) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path != null && path !in ignored) placed++
                }
            }
        }
        return checksum to placed
    }

    private fun report(
        name: String,
        biome: String,
        feature: String?,
        count: Int = 3,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = caveChunk(chunkIndex * 4 - 5, chunkIndex * -2 + 3)
            if (feature == null) {
                LushCaveVegetationDecoration.decorate(chunk, 2131L + chunkIndex, FakeEnvironment(biome))
            } else {
                LushCaveVegetationDecoration.decorateFeature(chunk, 2131L + chunkIndex, FakeEnvironment(biome), feature)
            }
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("wholeStep", "minecraft:lush_caves", null)
        // Any other biome leaves every feature drawing positions and placing nothing.
        report("plainsStep", "minecraft:plains", null)
        for (spec in LushCaveVegetationDecoration.featureSpecs()) {
            // The azalea tree gets one or two attempts a chunk and needs a ceiling to root in, so it
            // is the one feature that places nothing at all in a three-chunk sample.
            val chunks = if (spec.name == "rooted_azalea_tree") 48 else 3
            report(spec.name, "minecraft:lush_caves", spec.name, chunks)
        }
    }
}
