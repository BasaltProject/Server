package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class MushroomDecorationTest {
    @Test
    fun normalPairPlacesBothMushroomStates() {
        val brownSeed = findSeed { random -> random.nextFloat() < 1.0f / 256.0f }
        val redSeed =
            findSeed { random ->
                random.nextFloat() >= 1.0f / 256.0f && random.nextFloat() < 1.0f / 512.0f
            }
        assertPlaces(MushroomDecoration.NORMAL, brownSeed, brownMushroomStateId)
        assertPlaces(MushroomDecoration.NORMAL, redSeed, redMushroomStateId)
    }

    @Test
    fun taigaPairPlacesBothMushroomStates() {
        val brownSeed = findSeed { random -> random.nextFloat() < 1.0f / 4.0f }
        val redSeed =
            findSeed { random ->
                random.nextFloat() >= 1.0f / 4.0f && random.nextFloat() < 1.0f / 256.0f
            }
        assertPlaces(MushroomDecoration.TAIGA, brownSeed, brownMushroomStateId)
        assertPlaces(MushroomDecoration.TAIGA, redSeed, redMushroomStateId)
    }

    @Test
    fun oldGrowthRunsThreeBrownFiltersBeforeItsRedFilter() {
        val brownSeed = findSeed { random -> random.nextFloat() < 1.0f / 4.0f }
        val redSeed =
            findSeed { random ->
                repeat(3) {
                    if (random.nextFloat() < 1.0f / 4.0f) return@findSeed false
                }
                random.nextFloat() < 1.0f / 171.0f
            }
        assertPlaces(MushroomDecoration.OLD_GROWTH, brownSeed, brownMushroomStateId)
        assertPlaces(MushroomDecoration.OLD_GROWTH, redSeed, redMushroomStateId)
    }

    @Test
    fun swampRunsTwoBrownPatchesBeforeItsRedFilter() {
        val redSeed =
            findSeed { random ->
                repeat(2) { consumeSuccessfulPatch(random) }
                random.nextFloat() < 1.0f / 64.0f
            }
        assertPlaces(MushroomDecoration.SWAMP, 0L, brownMushroomStateId)
        assertPlaces(MushroomDecoration.SWAMP, redSeed, redMushroomStateId)
    }

    private fun findSeed(predicate: (WorldgenRandom) -> Boolean): Long =
        (0L..100_000L).first { seed -> predicate(WorldgenRandom(XoroshiroRandom(seed))) }

    private fun consumeSuccessfulPatch(random: WorldgenRandom) {
        random.nextInt(16)
        random.nextInt(16)
        repeat(96) {
            random.nextInt(8)
            random.nextInt(8)
            random.nextInt(4)
            random.nextInt(4)
            random.nextInt(8)
            random.nextInt(8)
        }
    }

    private fun assertPlaces(
        decoration: BiomeVegetalDecoration,
        seed: Long,
        expectedStateId: Int,
    ) {
        val chunk = flatGrassChunk()
        decoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
        var found = false
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, 63, z) == expectedStateId) found = true
            }
        }
        assertTrue(found, "expected state $expectedStateId with seed $seed")
    }

    private fun flatGrassChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val brownMushroomStateId = stateId("brown_mushroom")
    private val redMushroomStateId = stateId("red_mushroom")
}
