package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** parity: the passive CREATURE packs produced by the chunk-generation SPAWN status. */
class OriginalMobSpawnerTest {
    @Test
    fun aBiomeCreaturePackBecomesGeneratedEntityDescriptorsInsideItsChunk() {
        val chunk = Chunk(2, -3)
        val grass = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("grass_block"), emptyMap()))
        for (z in 0..15) {
            for (x in 0..15) chunk.setBlockStateId(x, 70, z, grass)
        }

        val added =
            OriginalMobSpawner.spawnInto(
                chunk,
                listOf(SpawnerEntry(Identifier.vanilla("cow"), weight = 1, minCount = 4, maxCount = 4)),
                creatureProbability = 0.1f,
                random = LegacyRandom(4096),
            )

        assertTrue(added > 0)
        assertEquals(added, chunk.generatedEntities().size)
        assertTrue(chunk.generatedEntities().all { it.type == Identifier.vanilla("cow") })
        assertTrue(chunk.generatedEntities().all { it.x >= 32.0 && it.x < 48.0 })
        assertTrue(chunk.generatedEntities().all { it.z >= -48.0 && it.z < -32.0 })
        assertTrue(chunk.generatedEntities().all { it.y == 71.0 })
    }

    @Test
    fun aCeilingDimensionFindsTheInteriorLavaSurfaceForStriders() {
        val chunk = Chunk(0, 0, ChunkHeight.NETHER)
        val lava = state("lava")
        val netherrack = state("netherrack")
        for (z in 0..15) {
            for (x in 0..15) {
                chunk.setBlockStateId(x, 32, z, lava)
                chunk.setBlockStateId(x, 127, z, netherrack)
            }
        }

        val added =
            OriginalMobSpawner.spawnInto(
                chunk,
                listOf(SpawnerEntry(Identifier.vanilla("strider"), weight = 1, minCount = 1, maxCount = 1)),
                creatureProbability = 0.1f,
                random = LegacyRandom(4096),
                hasCeiling = true,
            )

        assertTrue(added > 0)
        assertTrue(chunk.generatedEntities().all { it.type == Identifier.vanilla("strider") })
        assertTrue(chunk.generatedEntities().all { it.y == 32.0 })
    }

    private fun state(path: String): Int =
        checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(path), emptyMap()))
}
