package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeLookup

internal fun NetherVegetalDecoration.placeHugeFungi(
    chunk: Chunk,
    random: BasaltRandom,
    biomeSource: NetherBiomeLookup,
    config: HugeFungus,
) {
    val origins = positionsOnEveryLayer(chunk, random, 8)
    for (origin in origins) {
        if (biomeSource.blockBiomeAt(origin.x, origin.y, origin.z).id.path == config.biome) {
            placeHugeFungus(chunk, random, origin, config)
        }
    }
}

internal fun NetherVegetalDecoration.placeHugeFungus(
    chunk: Chunk,
    random: BasaltRandom,
    origin: Pos,
    config: HugeFungus,
) {
    if (blockPathAt(chunk, origin.x, origin.y - 1, origin.z) != config.validBase) return
    var totalHeight = 4 + random.nextInt(10)
    if (random.nextInt(12) == 0) totalHeight *= 2
    if (origin.y + totalHeight + 1 >= NETHER_GENERATION_DEPTH) return
    val isHuge = random.nextFloat() < HUGE_FUNGUS_PROBABILITY
    setBlock(chunk, origin.x, origin.y, origin.z, Blocks.AIR.stateId)
    placeHugeFungusStem(chunk, random, origin, totalHeight, isHuge, config)
    placeHugeFungusHat(chunk, random, origin, totalHeight, isHuge, config)
}

internal fun NetherVegetalDecoration.placeHugeFungusStem(
    chunk: Chunk,
    random: BasaltRandom,
    origin: Pos,
    totalHeight: Int,
    isHuge: Boolean,
    config: HugeFungus,
) {
    val radius = if (isHuge) 1 else 0
    val stem = stateId(config.stem)
    for (dx in -radius..radius) {
        for (dz in -radius..radius) {
            val corner = isHuge && kotlin.math.abs(dx) == radius && kotlin.math.abs(dz) == radius
            for (dy in 0 until totalHeight) {
                val x = origin.x + dx
                val y = origin.y + dy
                val z = origin.z + dz
                if (!isHugeFungusReplaceable(chunk, x, y, z, true)) continue
                if (!corner || random.nextFloat() < HUGE_STEM_CORNER_PROBABILITY) {
                    setBlock(chunk, x, y, z, stem)
                }
            }
        }
    }
}

internal fun NetherVegetalDecoration.placeHugeFungusHat(
    chunk: Chunk,
    random: BasaltRandom,
    origin: Pos,
    totalHeight: Int,
    isHuge: Boolean,
    config: HugeFungus,
) {
    val placesVines = config.hat == "nether_wart_block"
    val hatHeight = minOf(random.nextInt(1 + totalHeight / 3) + 5, totalHeight)
    val hatStartY = totalHeight - hatHeight
    for (dy in hatStartY..totalHeight) {
        var radius = if (dy < totalHeight - random.nextInt(3)) 2 else 1
        if (hatHeight > 8 && dy < hatStartY + 4) radius = 3
        if (isHuge) radius++
        for (dx in -radius..radius) {
            for (dz in -radius..radius) {
                val edgeX = dx == -radius || dx == radius
                val edgeZ = dz == -radius || dz == radius
                val inside = !edgeX && !edgeZ && dy != totalHeight
                val corner = edgeX && edgeZ
                val hatBottom = dy < hatStartY + 3
                val x = origin.x + dx
                val y = origin.y + dy
                val z = origin.z + dz
                if (!isHugeFungusReplaceable(chunk, x, y, z, false)) continue
                if (hatBottom) {
                    if (!inside) placeHugeFungusDropBlock(chunk, random, x, y, z, config, placesVines)
                } else if (inside) {
                    placeHugeFungusHatBlock(
                        chunk,
                        random,
                        x,
                        y,
                        z,
                        config,
                        0.1f,
                        0.2f,
                        if (placesVines) 0.1f else 0.0f,
                    )
                } else if (corner) {
                    placeHugeFungusHatBlock(
                        chunk,
                        random,
                        x,
                        y,
                        z,
                        config,
                        0.01f,
                        0.7f,
                        if (placesVines) 0.083f else 0.0f,
                    )
                } else {
                    placeHugeFungusHatBlock(
                        chunk,
                        random,
                        x,
                        y,
                        z,
                        config,
                        0.0005f,
                        0.98f,
                        if (placesVines) 0.07f else 0.0f,
                    )
                }
            }
        }
    }
}

internal fun NetherVegetalDecoration.placeHugeFungusHatBlock(
    chunk: Chunk,
    random: BasaltRandom,
    x: Int,
    y: Int,
    z: Int,
    config: HugeFungus,
    decorProbability: Float,
    hatProbability: Float,
    vinesProbability: Float,
) {
    if (random.nextFloat() < decorProbability) {
        setBlock(chunk, x, y, z, stateId(config.decor))
    } else if (random.nextFloat() < hatProbability) {
        setBlock(chunk, x, y, z, stateId(config.hat))
        if (random.nextFloat() < vinesProbability) tryPlaceHugeFungusVines(chunk, random, x, y, z)
    }
}

internal fun NetherVegetalDecoration.placeHugeFungusDropBlock(
    chunk: Chunk,
    random: BasaltRandom,
    x: Int,
    y: Int,
    z: Int,
    config: HugeFungus,
    placesVines: Boolean,
) {
    if (blockPathAt(chunk, x, y - 1, z) == config.hat) {
        setBlock(chunk, x, y, z, stateId(config.hat))
    } else if (random.nextFloat() < HUGE_HAT_DROP_PROBABILITY) {
        setBlock(chunk, x, y, z, stateId(config.hat))
        if (placesVines && random.nextInt(11) == 0) tryPlaceHugeFungusVines(chunk, random, x, y, z)
    }
}

internal fun NetherVegetalDecoration.tryPlaceHugeFungusVines(
    chunk: Chunk,
    random: BasaltRandom,
    x: Int,
    y: Int,
    z: Int,
) {
    if (!isAir(chunk, x, y - 1, z)) return
    var height = 1 + random.nextInt(5)
    if (random.nextInt(7) == 0) height *= 2
    placeWeepingColumn(chunk, random, x, y - 1, z, height, 23)
}

internal fun NetherVegetalDecoration.isHugeFungusReplaceable(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
    includeStemPlants: Boolean,
): Boolean {
    val state = stateAt(chunk, x, y, z) ?: return false
    val path = blockPath(state) ?: return false
    return Blocks.isAir(state) ||
        path in GENERALLY_REPLACEABLE ||
        includeStemPlants &&
        path in HUGE_FUNGUS_STEM_REPLACEABLE
}

internal enum class HugeFungus(
    val biome: String,
    val validBase: String,
    val stem: String,
    val hat: String,
    val decor: String,
) {
    CRIMSON("crimson_forest", "crimson_nylium", "crimson_stem", "nether_wart_block", "shroomlight"),
    WARPED("warped_forest", "warped_nylium", "warped_stem", "warped_wart_block", "shroomlight"),
}

private const val NETHER_GENERATION_DEPTH = 128
private const val HUGE_FUNGUS_PROBABILITY = 0.06f
private const val HUGE_STEM_CORNER_PROBABILITY = 0.1f
private const val HUGE_HAT_DROP_PROBABILITY = 0.15f
private val GENERALLY_REPLACEABLE =
    setOf(
        "water",
        "lava",
        "short_grass",
        "fern",
        "dead_bush",
        "bush",
        "short_dry_grass",
        "tall_dry_grass",
        "seagrass",
        "tall_seagrass",
        "fire",
        "soul_fire",
        "snow",
        "vine",
        "glow_lichen",
        "resin_clump",
        "light",
        "tall_grass",
        "large_fern",
        "structure_void",
        "bubble_column",
        "warped_roots",
        "nether_sprouts",
        "crimson_roots",
        "leaf_litter",
        "hanging_roots",
    )
private val HUGE_FUNGUS_STEM_REPLACEABLE =
    setOf(
        "oak_sapling",
        "spruce_sapling",
        "birch_sapling",
        "jungle_sapling",
        "acacia_sapling",
        "cherry_sapling",
        "dark_oak_sapling",
        "pale_oak_sapling",
        "mangrove_propagule",
        "dandelion",
        "torchflower",
        "poppy",
        "blue_orchid",
        "allium",
        "azure_bluet",
        "red_tulip",
        "orange_tulip",
        "white_tulip",
        "pink_tulip",
        "oxeye_daisy",
        "cornflower",
        "wither_rose",
        "lily_of_the_valley",
        "brown_mushroom",
        "red_mushroom",
        "wheat",
        "sugar_cane",
        "attached_pumpkin_stem",
        "attached_melon_stem",
        "pumpkin_stem",
        "melon_stem",
        "lily_pad",
        "nether_wart",
        "cocoa",
        "carrots",
        "potatoes",
        "chorus_plant",
        "chorus_flower",
        "torchflower_crop",
        "pitcher_crop",
        "beetroots",
        "sweet_berry_bush",
        "warped_fungus",
        "crimson_fungus",
        "weeping_vines",
        "weeping_vines_plant",
        "twisting_vines",
        "twisting_vines_plant",
        "cave_vines",
        "cave_vines_plant",
        "spore_blossom",
        "azalea",
        "flowering_azalea",
        "moss_carpet",
        "pink_petals",
        "wildflowers",
        "big_dripleaf",
        "big_dripleaf_stem",
        "small_dripleaf",
    )
