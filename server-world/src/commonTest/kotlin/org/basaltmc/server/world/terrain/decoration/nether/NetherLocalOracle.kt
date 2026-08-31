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

/** The frozen Kotlin as the oracle for the Nether basalt pillar. */
class NetherLocalOracle {
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

    /** A soul sand valley roof over an open cavern, which is the only shape a pillar hangs from. */
    private fun valleyChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 10..20) chunk.setBlockStateId(x, y, z, stateId("soul_soil"))
                for (y in 60..80) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var basalt = 0
        for (y in 0..85) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.byStateId(id)?.block?.path == "basalt") basalt++
                }
            }
        }
        return checksum to basalt
    }

    @Test
    fun dump() {
        for ((name, biome) in listOf("valley" to Biomes.SOUL_SAND_VALLEY, "wastes" to Biomes.NETHER_WASTES)) {
            var checksum = -3750763034362895579L
            var basalt = 0
            var deferred = 0
            for (index in 0 until 24) {
                val chunk = valleyChunk(index * 5 - 30, index * -3 + 12)
                NetherLocalDecoration.decorate(chunk, 606L + index, FixedBiome(biome))
                val (chunkChecksum, chunkBasalt) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                basalt += chunkBasalt
                deferred += chunk.takeGeneratedBlockWrites().size
            }
            println("ORACLE step_$name basalt=$basalt deferred=$deferred checksum=$checksum")
        }

        var checksum = -3750763034362895579L
        var basalt = 0
        var trailing = 0
        for (index in 0 until 24) {
            val chunk = valleyChunk(index * 5 - 30, index * -3 + 12)
            val random = WorldgenRandom(LegacyRandom(909L + index * 3121L))
            NetherLocalDecoration.placeAt(chunk, random, chunk.chunkX * 16 + 8, 59, chunk.chunkZ * 16 + 8)
            val (chunkChecksum, chunkBasalt) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            basalt += chunkBasalt
            trailing = random.nextInt()
        }
        println("ORACLE pillarAt basalt=$basalt checksum=$checksum after=$trailing")
    }
}
