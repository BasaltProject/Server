package org.basaltmc.world.terrain.decoration.end

import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Biomes
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.TheEndBiomeSource
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
            if (biomeSource.biomeAtQuart(x shr 2, y shr 2, z shr 2) == Biomes.SMALL_END_ISLANDS) {
                placeIsland(chunk, random, x, y, z)
            }
        }
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
        if (x.floorDiv(16) != chunk.chunkX || z.floorDiv(16) != chunk.chunkZ) return
        if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
        chunk.setBlockStateId(x and 15, y, z and 15, Blocks.END_STONE.stateId)
    }

    private const val RAW_GENERATION_STEP = 0
    private const val END_ISLAND_INDEX = 0
    private const val END_ISLAND_RARITY = 14
    private const val END_ISLAND_MIN_Y = 55
    private const val END_ISLAND_MAX_Y = 70
}
