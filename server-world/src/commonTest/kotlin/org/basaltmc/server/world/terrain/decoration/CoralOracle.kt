package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the warm ocean coral. */
class CoralOracle {
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

    /** A sandy floor under deep water, with a ridge so the ocean floor is not one flat plane. */
    private fun oceanChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val floor = 30 + ((chunkX * 16 + x) * 3 + (chunkZ * 16 + z)).mod(4)
                for (y in -64..floor) chunk.setBlockStateId(x, y, z, stateId("sand"))
                for (y in floor + 1..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private val counted = setOf("sea_pickle")

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var coral = 0
        var pickles = 0
        for (y in -64..70) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path ?: continue
                    if (path.contains("coral")) coral++
                    if (path in counted) pickles++
                }
            }
        }
        return Triple(checksum, coral, pickles)
    }

    private fun report(
        name: String,
        biome: String,
        count: Int = 6,
    ) {
        var checksum = -3750763034362895579L
        var coral = 0
        var pickles = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = oceanChunk(chunkIndex * 2 - 5, chunkIndex * -4 + 7)
            CoralDecoration.decorate(chunk, 3389L + chunkIndex, FakeEnvironment(biome))
            val (chunkChecksum, chunkCoral, chunkPickles) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            coral += chunkCoral
            pickles += chunkPickles
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name coral=$coral pickles=$pickles deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("warmOcean", "minecraft:warm_ocean")
        // Any other biome leaves the step drawing positions and placing nothing.
        report("coldOcean", "minecraft:cold_ocean")

        var checksum = -3750763034362895579L
        var coral = 0
        var pickles = 0
        var trailing = 0
        for (chunkIndex in 0 until 12) {
            val chunk = oceanChunk(chunkIndex, -chunkIndex)
            val random = WorldgenRandom(LegacyRandom(881L + chunkIndex * 5077L))
            CoralDecoration.placeAt(chunk, random, DecorationPos(chunk.chunkX * 16 + 8, 34, chunk.chunkZ * 16 + 8))
            val (chunkChecksum, chunkCoral, chunkPickles) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            coral += chunkCoral
            pickles += chunkPickles
            trailing = random.nextInt()
        }
        println("ORACLE placeAt coral=$coral pickles=$pickles checksum=$checksum after=$trailing")

        var vegetalChecksum = -3750763034362895579L
        var vegetalCoral = 0
        var vegetalPickles = 0
        var vegetalDeferred = 0
        for (chunkIndex in 0 until 6) {
            val chunk = oceanChunk(chunkIndex * 3 - 2, chunkIndex + 1)
            val random = WorldgenRandom(XoroshiroRandom(7717L + chunkIndex))
            CoralDecoration.decorate(chunk, random)
            val (chunkChecksum, chunkCoral, chunkPickles) = fold(chunk)
            vegetalChecksum = (vegetalChecksum xor chunkChecksum) * 1099511628211L
            vegetalCoral += chunkCoral
            vegetalPickles += chunkPickles
            vegetalDeferred += chunk.takeGeneratedBlockWrites().size
        }
        println(
            "ORACLE vegetal coral=$vegetalCoral pickles=$vegetalPickles " +
                "deferred=$vegetalDeferred checksum=$vegetalChecksum",
        )
    }
}
