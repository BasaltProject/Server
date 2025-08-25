package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeLookup

/** The vegetation patch itself, without the layer scan that chooses where the step puts one. */
internal fun NetherVegetalDecoration.placeVegetationAt(
    chunk: Chunk,
    random: BasaltRandom,
    origin: Pos,
    config: ForestVegetation,
) {
    repeat(64) {
        val x = origin.x + random.nextInt(8) - random.nextInt(8)
        val y = origin.y + random.nextInt(4) - random.nextInt(4)
        val z = origin.z + random.nextInt(8) - random.nextInt(8)
        val state = config.sample(random)
        val support = blockPathAt(chunk, x, y - 1, z)
        if (isAir(chunk, x, y, z) && y > chunk.verticalBounds.minY && support in VEGETATION_SUPPORT) {
            setBlock(chunk, x, y, z, stateId(state))
        }
    }
}

internal fun NetherVegetalDecoration.placeForestVegetation(
    chunk: Chunk,
    random: BasaltRandom,
    biomeSource: NetherBiomeLookup,
    config: ForestVegetation,
) {
    val origins = positionsOnEveryLayer(chunk, random, config.layerCount)
    for (origin in origins) {
        if (biomeSource.blockBiomeAt(origin.x, origin.y, origin.z).id.path != config.biome) continue
        if (blockPathAt(chunk, origin.x, origin.y - 1, origin.z) !in NYLIUM) continue
        if (origin.y < chunk.verticalBounds.minY + 1 || origin.y + 1 > chunk.verticalBounds.maxY) continue
        repeat(64) {
            val x = origin.x + random.nextInt(8) - random.nextInt(8)
            val y = origin.y + random.nextInt(4) - random.nextInt(4)
            val z = origin.z + random.nextInt(8) - random.nextInt(8)
            val state = config.sample(random)
            val support = blockPathAt(chunk, x, y - 1, z)
            if (isAir(chunk, x, y, z) && y > chunk.verticalBounds.minY && support in VEGETATION_SUPPORT) {
                setBlock(chunk, x, y, z, stateId(state))
            }
        }
    }
}

internal enum class ForestVegetation(
    val layerCount: Int,
    val biome: String,
) {
    WARPED(5, "warped_forest") {
        override fun sample(random: BasaltRandom): String =
            when (random.nextInt(100)) {
                in 0..84 -> "warped_roots"
                85 -> "crimson_roots"
                in 86..98 -> "warped_fungus"
                else -> "crimson_fungus"
            }
    },
    SPROUTS(4, "warped_forest") {
        override fun sample(random: BasaltRandom): String = "nether_sprouts"
    },
    CRIMSON(6, "crimson_forest") {
        override fun sample(random: BasaltRandom): String =
            when (random.nextInt(99)) {
                in 0..86 -> "crimson_roots"
                in 87..97 -> "crimson_fungus"
                else -> "warped_fungus"
            }
    },
    ;

    abstract fun sample(random: BasaltRandom): String
}

private val NYLIUM = setOf("crimson_nylium", "warped_nylium")
private val VEGETATION_SUPPORT =
    setOf(
        "dirt",
        "coarse_dirt",
        "rooted_dirt",
        "mud",
        "muddy_mangrove_roots",
        "moss_block",
        "pale_moss_block",
        "grass_block",
        "mycelium",
        "podzol",
        "farmland",
        "crimson_nylium",
        "warped_nylium",
        "soul_soil",
    )
