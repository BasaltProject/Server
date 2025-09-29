package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.NetherBiomeLookup
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the basalt deltas surface features. */
class NetherSurfaceStructuresOracle {
    private class FixedBiome(private val biome: Biome) : NetherBiomeLookup {
        override fun blockBiomeAt(x: Int, y: Int, z: Int, cache: DensityCache?): Biome = biome
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** An uneven netherrack floor with a lava sea below the Nether sea level and open air over it. */
    private fun deltaChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val top = 28 + (x * 3 + z) % 4
                for (y in 0..top) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
                if ((x + z) % 6 == 0) {
                    for (y in 26..30) chunk.setBlockStateId(x, y, z, stateId("lava"))
                }
                for (y in 60..70) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var basalt = 0
        var magma = 0
        for (y in 0..70) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    when (Blocks.byStateId(id)?.block?.path) {
                        "basalt" -> basalt++
                        "magma_block" -> magma++
                    }
                }
            }
        }
        return Triple(checksum, basalt, magma)
    }

    private fun report(name: String, biome: Biome, index: Int? = null, count: Int = 12) {
        var checksum = -3750763034362895579L
        var basalt = 0
        var magma = 0
        for (chunkIndex in 0 until count) {
            val chunk = deltaChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12)
            if (index == null) {
                NetherSurfaceStructuresDecoration.decorate(chunk, 5150L + chunkIndex, FixedBiome(biome))
            } else {
                NetherSurfaceStructuresDecoration.decorateFeature(
                    chunk,
                    5150L + chunkIndex,
                    FixedBiome(biome),
                    index,
                )
            }
            val (chunkChecksum, chunkBasalt, chunkMagma) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            basalt += chunkBasalt
            magma += chunkMagma
        }
        println("ORACLE $name basalt=$basalt magma=$magma checksum=$checksum")
    }

    @Test
    fun dump() {
        report("deltasStep", Biomes.BASALT_DELTAS)
        report("wastesStep", Biomes.NETHER_WASTES)
        report("delta", Biomes.BASALT_DELTAS, 0)
        report("smallColumns", Biomes.BASALT_DELTAS, 1)
        report("largeColumns", Biomes.BASALT_DELTAS, 2)

        var checksum = -3750763034362895579L
        var basalt = 0
        var magma = 0
        val known = StringBuilder()
        var trailing = 0
        for (feature in listOf("delta", "small_basalt_columns", "large_basalt_columns", "not_a_feature")) {
            for (chunkIndex in 0 until 6) {
                val chunk = deltaChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(24L + chunkIndex * 4099L))
                known.append(
                    if (NetherSurfaceStructuresDecoration.placeAt(
                            chunk,
                            random,
                            chunk.chunkX * 16 + 8,
                            // The floor at (8, 8) tops out at 28, so this is the first air above it.
                            29,
                            chunk.chunkZ * 16 + 8,
                            feature,
                        )
                    ) {
                        '1'
                    } else {
                        '0'
                    },
                )
                val (chunkChecksum, chunkBasalt, chunkMagma) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                basalt += chunkBasalt
                magma += chunkMagma
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known basalt=$basalt magma=$magma checksum=$checksum after=$trailing")
    }
}
