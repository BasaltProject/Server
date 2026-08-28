package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import kotlin.math.ceil
import kotlin.math.floor

/** // parity: `end_island_decorated`, the End's RAW_GENERATION placed feature. */
public object EndRawDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        biomeSource: TheEndBiomeSource,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, END_ISLAND_INDEX, RAW_GENERATION_STEP)
        if (random.nextFloat() >= 1.0f / END_ISLAND_RARITY) return
        val count = if (random.nextInt(4) < 3) 1 else 2
        repeat(count) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = END_ISLAND_MIN_Y + random.nextInt(END_ISLAND_MAX_Y - END_ISLAND_MIN_Y + 1)
            if (biomeSource.blockBiomeAt(x, y, z) == Biomes.SMALL_END_ISLANDS) {
                placeIsland(chunk, random, x, y, z)
            }
        }
    }

    /** parity: `end_island_decorated`, one island at one position. */
    public fun placeAt(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
    ) {
        placeIsland(chunk, random, x, y, z)
    }

    private fun placeIsland(
        chunk: Chunk,
        random: BasaltRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
    ) {
        var size = random.nextInt(3) + 4.0f
        var y = originY
        while (size > 0.5f) {
            for (x in floor(-size).toInt()..ceil(size).toInt()) {
                for (z in floor(-size).toInt()..ceil(size).toInt()) {
                    if (x * x + z * z <= (size + 1.0f) * (size + 1.0f)) {
                        setBlock(chunk, originX + x, y, originZ + z)
                    }
                }
            }
            y--
            size -= random.nextInt(2) + 0.5f
        }
    }

    private fun setBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
        chunk.setDecorationBlockStateId(x, y, z, Blocks.END_STONE.stateId)
    }

    private const val RAW_GENERATION_STEP = 0
    private const val END_ISLAND_INDEX = 0
    private const val END_ISLAND_RARITY = 14
    private const val END_ISLAND_MIN_Y = 55
    private const val END_ISLAND_MAX_Y = 70
}
