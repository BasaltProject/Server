package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the whole Overworld vegetal step. */
class OverworldVegetalOracle {
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

    /** A land surface with a lake and a cavern, so land, water and cave features all have a home. */
    private fun worldChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                val lake = (worldX * 3 + worldZ * 5).mod(11) < 3
                val surface = if (lake) 60 else 70
                for (y in 20..surface - 1) chunk.setBlockStateId(x, y, z, stateId("stone"))
                chunk.setBlockStateId(
                    x,
                    surface,
                    z,
                    if (lake) stateId("sand") else Blocks.GRASS_BLOCK.stateId,
                )
                if (lake) for (y in surface + 1..63) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                // A cavern for the lush cave and glow lichen entries.
                for (y in 30..37) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                if ((worldX + worldZ).mod(5) == 0) chunk.setBlockStateId(x, 30, z, stateId("dirt"))
            }
        }
        return chunk
    }

    private val ignored = setOf("stone", "grass_block", "sand", "dirt", "air", "water")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 20..110) {
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

    @Test
    fun dump() {
        println("ORACLE order names=${OverworldVegetalDecoration.featureOrder().joinToString(",") { it.second }}")
        for (biome in listOf(
            "minecraft:plains",
            "minecraft:jungle",
            "minecraft:lush_caves",
            "minecraft:swamp",
            "minecraft:warm_ocean",
        )) {
            var checksum = -3750763034362895579L
            var placed = 0
            var deferred = 0
            for (chunkIndex in 0 until 3) {
                val chunk = worldChunk(chunkIndex * 2 - 3, chunkIndex + 4)
                OverworldVegetalDecoration.decorate(chunk, 4157L + chunkIndex, FakeEnvironment(biome))
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                deferred += chunk.takeGeneratedBlockWrites().size
            }
            println("ORACLE ${biome.substringAfter(':')} placed=$placed deferred=$deferred checksum=$checksum")
        }
    }
}
