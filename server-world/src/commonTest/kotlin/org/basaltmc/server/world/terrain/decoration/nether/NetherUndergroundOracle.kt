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

/** The frozen Kotlin as the oracle for the Nether underground decoration step. */
class NetherUndergroundOracle {
    private class FixedBiome(
        private val biome: Biome,
    ) : NetherBiomeLookup {
        override fun blockBiomeAt(
            x: Int,
            y: Int,
            z: Int,
            cache: DensityCache?,
        ): Biome = biome
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Netherrack with soul soil, gravel and caverns, so every support and spring rule can fire. */
    private fun netherChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0..125) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
                for (y in 20..23) chunk.setBlockStateId(x, y, z, stateId("soul_soil"))
                // Open air over the soul soil, or the soul fire and crimson roots have nowhere to go.
                for (y in 24..26) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                for (y in 40..42) chunk.setBlockStateId(x, y, z, stateId("gravel"))
                for (y in 60..70) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                if ((x * 5 + z) % 7 == 0) {
                    for (y in 80..84) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                }
                // Single-block gaps: only the top block of a cavern has rock above it, and that is
                // the one thing a glowstone blob hangs from.
                for (y in listOf(30, 35, 45, 50, 90, 95, 100)) {
                    chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 0..125) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path != null && path !in setOf("netherrack", "air", "soul_soil", "gravel")) placed++
                }
            }
        }
        return checksum to placed
    }

    private fun report(
        name: String,
        biome: Biome,
        index: Int? = null,
        count: Int = 4,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12)
            if (index == null) {
                NetherUndergroundDecoration.decorate(chunk, 4141L + chunkIndex, FixedBiome(biome))
            } else {
                NetherUndergroundDecoration.decorateFeature(chunk, 4141L + chunkIndex, FixedBiome(biome), index)
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
        report("deltasStep", Biomes.BASALT_DELTAS)
        report("wastesStep", Biomes.NETHER_WASTES)
        report("valleyStep", Biomes.SOUL_SAND_VALLEY)
        for (index in 0..15) {
            val biome =
                when (index) {
                    0, 1, 2, 12, 13, 14 -> Biomes.BASALT_DELTAS
                    8 -> Biomes.SOUL_SAND_VALLEY
                    else -> Biomes.NETHER_WASTES
                }
            // Each rare one is paid for only where it is rare: the crimson roots need a wider
            // sample than the rest before they fire at all.
            val chunks =
                when (index) {
                    8 -> 44
                    5, 6, 9, 10 -> 24
                    else -> 4
                }
            report("index_$index", biome, index, chunks)
        }

        var checksum = -3750763034362895579L
        var placed = 0
        val known = StringBuilder()
        var trailing = 0
        val features = NetherUndergroundDecoration.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            for (chunkIndex in 0 until 3) {
                val chunk = netherChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(313L + chunkIndex * 2029L))
                known.append(
                    if (NetherUndergroundDecoration.placeAt(
                            chunk,
                            random,
                            chunk.chunkX * 16 + 8,
                            59,
                            chunk.chunkZ * 16 + 8,
                            feature,
                        )
                    ) {
                        '1'
                    } else {
                        '0'
                    },
                )
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known placed=$placed checksum=$checksum after=$trailing")
    }
}
