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

/** The frozen Kotlin as the oracle for the ten Nether ore features. */
class NetherOreOracle {
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

    /**
     * Solid netherrack with a cavern, so the scattered ores' air-exposure rule can bite. No basalt or
     * blackstone bands: blackstone is also what one of the veins places, and terrain of the same
     * block would hide it from the count.
     */
    private fun netherChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0..125) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
                for (y in 70..78) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var ores = 0
        for (y in 0..125) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path != null && path != "netherrack" && path != "air") {
                        ores++
                    }
                }
            }
        }
        return checksum to ores
    }

    private fun report(
        name: String,
        biome: Biome,
        index: Int? = null,
        count: Int = 16,
    ) {
        var checksum = -3750763034362895579L
        var ores = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12)
            if (index == null) {
                NetherOreDecoration.decorate(chunk, 3030L + chunkIndex, FixedBiome(biome))
            } else {
                val random = WorldgenRandom(LegacyRandom(77L + chunkIndex * 8191L))
                NetherOreDecoration.placeFeature(index, chunk, random, FixedBiome(biome))
            }
            val (chunkChecksum, chunkOres) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            ores += chunkOres
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name ores=$ores deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("wastesStep", Biomes.NETHER_WASTES)
        report("deltasStep", Biomes.BASALT_DELTAS)
        report("valleyStep", Biomes.SOUL_SAND_VALLEY)
        for ((name, index) in NetherOreDecoration.PLACED_IDS) {
            val biome =
                if (name.endsWith("_deltas")) {
                    Biomes.BASALT_DELTAS
                } else if (name == "ore_soul_sand") {
                    Biomes.SOUL_SAND_VALLEY
                } else {
                    Biomes.NETHER_WASTES
                }
            report("feature_$name", biome, index)
        }

        var checksum = -3750763034362895579L
        var ores = 0
        val known = StringBuilder()
        var trailing = 0
        for ((name, _) in NetherOreDecoration.PLACED_IDS) {
            for (chunkIndex in 0 until 4) {
                val chunk = netherChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(151L + chunkIndex * 613L))
                known.append(
                    if (NetherOreDecoration.placeAt(
                            chunk,
                            random,
                            chunk.chunkX * 16 + 8,
                            60,
                            chunk.chunkZ * 16 + 8,
                            name,
                        )
                    ) {
                        '1'
                    } else {
                        '0'
                    },
                )
                val (chunkChecksum, chunkOres) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                ores += chunkOres
                trailing = random.nextInt()
            }
        }
        val chunk = netherChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(1L))
        known.append(if (NetherOreDecoration.placeAt(chunk, random, 8, 60, 8, "not_an_ore")) '1' else '0')
        println("ORACLE placeAt known=$known ores=$ores checksum=$checksum after=$trailing")
    }
}
