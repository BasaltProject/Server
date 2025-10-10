package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the waterlily and leaf litter step-9 patches. */
class SurfacePatchOracle {
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

    /** Grass on land, water in a basin, so both patches have ground they accept. */
    private fun surfaceChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                if (x in 4..11 && z in 4..11) {
                    chunk.setBlockStateId(x, 70, z, Blocks.WATER.stateId)
                } else {
                    chunk.setBlockStateId(x, 70, z, Blocks.GRASS_BLOCK.stateId)
                }
            }
        }
        return chunk
    }

    private fun report(
        name: String,
        biome: String,
        body: (Chunk, FakeEnvironment) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (index in 0 until 24) {
            val chunk = surfaceChunk(index * 3 - 20, 7 - index * 2)
            body(chunk, FakeEnvironment(biome))
            for (y in 69..74) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        val id = chunk.getBlockStateId(x, y, z)
                        checksum = (checksum xor id.toLong()) * 1099511628211L
                        if (!Blocks.isAir(id) &&
                            id != Blocks.STONE.stateId &&
                            id != Blocks.GRASS_BLOCK.stateId &&
                            id != Blocks.WATER.stateId
                        ) {
                            placed++
                        }
                    }
                }
            }
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("waterlily", "minecraft:swamp") { c, e -> WaterlilyPatchDecoration.decorate(c, 918273L, e) }
        report("waterlilyWrongBiome", "minecraft:plains") { c, e ->
            WaterlilyPatchDecoration.decorate(c, 918273L, e)
        }
        report("leafLitter", "minecraft:dark_forest") { c, e ->
            LeafLitterPatchDecoration.decorate(c, 918273L, e)
        }
        // With a reader installed the patches keep the part that falls outside the chunk.
        report("waterlilyRegional", "minecraft:swamp") { c, e ->
            WaterlilyPatchDecoration.decorate(c, 918273L, e, externalBlockState = { _, y, _ ->
                if (y < 70) {
                    Blocks.STONE.stateId
                } else if (y == 70) {
                    Blocks.WATER.stateId
                } else {
                    Blocks.AIR.stateId
                }
            })
        }
        report("leafLitterRegional", "minecraft:dark_forest") { c, e ->
            LeafLitterPatchDecoration.decorate(c, 918273L, e, externalBlockState = { _, y, _ ->
                if (y < 70) {
                    Blocks.STONE.stateId
                } else if (y == 70) {
                    Blocks.GRASS_BLOCK.stateId
                } else {
                    Blocks.AIR.stateId
                }
            })
        }

        val chunk = surfaceChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(4242L))
        WaterlilyPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8))
        var checksum = -3750763034362895579L
        for (y in 69..74) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        println("ORACLE waterlilyAt checksum=$checksum after=" + random.nextInt())
    }
}
