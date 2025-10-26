package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the `/place feature` registry. */
class PlacedFeaturesOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Enough of a world for most features: rock, soil, a lake, a cavern and open sky. */
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
                for (y in 0 until surface) chunk.setBlockStateId(x, y, z, stateId("stone"))
                for (y in surface - 4 until surface) chunk.setBlockStateId(x, y, z, stateId("dirt"))
                chunk.setBlockStateId(x, surface, z, if (lake) stateId("sand") else Blocks.GRASS_BLOCK.stateId)
                if (lake) for (y in surface + 1..63) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                for (y in 20..27) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                for (y in 10..17) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Long {
        var checksum = -3750763034362895579L
        for (y in 0..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        return checksum
    }

    @Test
    fun dump() {
        val ids =
            PlacedFeatures.ids
                .filter { PlacedFeatures[it] != null }
                .map { it.path }
                .sorted()
        println("ORACLE ids count=${ids.size} joined=${ids.joinToString(",")}")
        val contextual = PlacedFeatures.contextualIds.map { it.path }.sorted()
        println("ORACLE contextualIds count=${contextual.size} joined=${contextual.joinToString(",")}")
        var checksum = -3750763034362895579L
        var deferred = 0
        var trailing = 0
        for ((index, name) in ids.withIndex()) {
            val placement = PlacedFeatures[Identifier.vanilla(name)] ?: continue
            val chunk = worldChunk(index.mod(3) - 1, index.mod(5) + 2)
            val random = WorldgenRandom(LegacyRandom(3491L + index * 761L))
            // Two heights per feature: the open surface and the cavern floor underground.
            for (y in listOf(71, 21)) {
                placement.place(chunk, random, chunk.chunkX * 16 + 8, y, chunk.chunkZ * 16 + 8)
            }
            checksum = (checksum xor fold(chunk)) * 1099511628211L
            deferred += chunk.takeGeneratedBlockWrites().size
            trailing = random.nextInt()
        }
        println("ORACLE run deferred=$deferred checksum=$checksum after=$trailing")
    }
}
