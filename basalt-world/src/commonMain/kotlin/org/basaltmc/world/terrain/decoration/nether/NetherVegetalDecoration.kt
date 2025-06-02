package org.basaltmc.world.terrain.decoration.nether

import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.NetherBiomeSource

/** // parity: Nether placed features in the VEGETAL_DECORATION step. */
public object NetherVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeSource,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in 0..9) {
            random.setFeatureSeed(decorationSeed, globalIndex, VEGETAL_DECORATION_STEP)
            placeFeature(chunk, random, biomeSource, globalIndex)
        }
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeSource,
        globalIndex: Int,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, globalIndex, VEGETAL_DECORATION_STEP)
        placeFeature(chunk, random, biomeSource, globalIndex)
    }

    private fun placeFeature(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        globalIndex: Int,
    ) {
        when (globalIndex) {
            0 -> placeLavaSprings(chunk, random, biomeSource)
            1 -> placeNormalMushroom(chunk, random, biomeSource, 256, "brown_mushroom")
            2 -> placeNormalMushroom(chunk, random, biomeSource, 512, "red_mushroom")
            3 -> placeHugeFungi(chunk, random, biomeSource, HugeFungus.CRIMSON)
            4 -> placeForestVegetation(chunk, random, biomeSource, ForestVegetation.WARPED)
            5 -> placeForestVegetation(chunk, random, biomeSource, ForestVegetation.SPROUTS)
            6 -> placeTwistingVines(chunk, random, biomeSource)
            7 -> placeWeepingVines(chunk, random, biomeSource)
            8 -> placeHugeFungi(chunk, random, biomeSource, HugeFungus.WARPED)
            9 -> placeForestVegetation(chunk, random, biomeSource, ForestVegetation.CRIMSON)
        }
    }

    private fun placeLavaSprings(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
    ) {
        repeat(20) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = veryBiasedHeight(random)
            if (biomeSource.biomeAt(x, y, z).id.path in ALL_BIOMES) {
                placeLavaSpring(chunk, x, y, z)
            }
        }
    }

    private fun veryBiasedHeight(random: BasaltRandom): Int {
        val upperInclusive = 8 + random.nextInt(112)
        val biasedUpperInclusive = random.nextInt(upperInclusive)
        return random.nextInt(biasedUpperInclusive + 8)
    }

    private fun placeLavaSpring(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (!isSpringRock(chunk, x, y + 1, z) || !isSpringRock(chunk, x, y - 1, z)) return
        val current = stateAt(chunk, x, y, z) ?: return
        if (!Blocks.isAir(current) && !isSpringRock(chunk, x, y, z)) return
        var rocks = 0
        var holes = 0
        for ((offsetX, offsetY, offsetZ) in SPRING_NEIGHBOURS) {
            val state = stateAt(chunk, x + offsetX, y + offsetY, z + offsetZ)
            if (state != null && blockPath(state) in OVERWORLD_SPRING_ROCKS) rocks++
            if (state != null && Blocks.isAir(state)) holes++
        }
        if (rocks == 4 && holes == 1) setBlock(chunk, x, y, z, Blocks.LAVA.stateId)
    }

    private fun placeNormalMushroom(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        rarity: Int,
        mushroom: String,
    ) {
        if (random.nextFloat() >= 1.0f / rarity) return
        val baseX = chunk.chunkX * 16 + random.nextInt(16)
        val baseZ = chunk.chunkZ * 16 + random.nextInt(16)
        val baseY = chunk.heightAt(baseX and 15, baseZ and 15, Chunk.HeightmapType.MOTION_BLOCKING)
        if (biomeSource.biomeAt(baseX, baseY, baseZ).id.path !in NORMAL_MUSHROOM_BIOMES) return
        val mushroomState = stateId(mushroom)
        repeat(PATCH_ATTEMPTS) {
            val x = baseX + triangle(random, 7)
            val y = baseY + triangle(random, 3)
            val z = baseZ + triangle(random, 7)
            val support = blockPathAt(chunk, x, y - 1, z)
            if (isAir(chunk, x, y, z) && support != null && supportsMushroom(support)) {
                setBlock(chunk, x, y, z, mushroomState)
            }
        }
    }

    private fun supportsMushroom(path: String): Boolean =
        path in MUSHROOM_LIGHT_OVERRIDE || path in MUSHROOM_SOLID_SUPPORT

    private fun placeHugeFungi(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        config: HugeFungus,
    ) {
        val origins = positionsOnEveryLayer(chunk, random, 8)
        for (origin in origins) {
            if (biomeSource.biomeAt(origin.x, origin.y, origin.z).id.path == config.biome) {
                placeHugeFungus(chunk, random, origin, config)
            }
        }
    }

    private fun placeHugeFungus(
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

    private fun placeHugeFungusStem(
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

    private fun placeHugeFungusHat(
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

    private fun placeHugeFungusHatBlock(
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

    private fun placeHugeFungusDropBlock(
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

    private fun tryPlaceHugeFungusVines(
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

    private fun isHugeFungusReplaceable(
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

    private fun placeForestVegetation(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        config: ForestVegetation,
    ) {
        val origins = positionsOnEveryLayer(chunk, random, config.layerCount)
        for (origin in origins) {
            if (biomeSource.biomeAt(origin.x, origin.y, origin.z).id.path != config.biome) continue
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

    private fun positionsOnEveryLayer(
        chunk: Chunk,
        random: BasaltRandom,
        count: Int,
    ): List<Pos> {
        val positions = mutableListOf<Pos>()
        var layer = 0
        do {
            var foundAny = false
            repeat(count) {
                val x = chunk.chunkX * 16 + random.nextInt(16)
                val z = chunk.chunkZ * 16 + random.nextInt(16)
                val startY = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
                val y = findGroundLayer(chunk, x, startY, z, layer)
                if (y != null) {
                    positions += Pos(x, y, z)
                    foundAny = true
                }
            }
            layer++
        } while (foundAny)
        return positions
    }

    private fun findGroundLayer(
        chunk: Chunk,
        x: Int,
        startY: Int,
        z: Int,
        targetLayer: Int,
    ): Int? {
        var currentLayer = 0
        var currentState = stateAt(chunk, x, startY, z)
        for (y in startY downTo chunk.verticalBounds.minY + 1) {
            val below = stateAt(chunk, x, y - 1, z)
            val surface = below != null && !isLayerEmpty(below) && currentState != null && isLayerEmpty(currentState)
            if (surface && blockPath(below) != "bedrock") {
                if (currentLayer == targetLayer) return y
                currentLayer++
            }
            currentState = below
        }
        return null
    }

    private fun isLayerEmpty(stateId: Int): Boolean {
        val path = blockPath(stateId)
        return Blocks.isAir(stateId) || path == "water" || path == "lava"
    }

    private fun placeTwistingVines(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
    ) {
        repeat(10) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = random.nextInt(128)
            if (biomeSource.biomeAt(x, y, z).id.path == "warped_forest") {
                placeTwistingVinesFeature(chunk, random, Pos(x, y, z))
            }
        }
    }

    private fun placeTwistingVinesFeature(
        chunk: Chunk,
        random: BasaltRandom,
        origin: Pos,
    ) {
        if (isInvalidTwistingPosition(chunk, origin.x, origin.y, origin.z)) return
        repeat(64) {
            val x = origin.x - 8 + random.nextInt(17)
            var y = origin.y - 4 + random.nextInt(9)
            val z = origin.z - 8 + random.nextInt(17)
            while (true) {
                y--
                if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return@repeat
                if (!isAir(chunk, x, y, z)) break
            }
            y++
            if (isInvalidTwistingPosition(chunk, x, y, z)) return@repeat
            var vineHeight = 1 + random.nextInt(8)
            if (random.nextInt(6) == 0) vineHeight *= 2
            if (random.nextInt(5) == 0) vineHeight = 1
            placeTwistingColumn(chunk, random, x, y, z, vineHeight)
        }
    }

    private fun isInvalidTwistingPosition(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = !isAir(chunk, x, y, z) || blockPathAt(chunk, x, y - 1, z) !in TWISTING_SUPPORT

    private fun placeTwistingColumn(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        startY: Int,
        z: Int,
        totalHeight: Int,
    ) {
        var y = startY
        for (height in 1..totalHeight) {
            if (isAir(chunk, x, y, z)) {
                if (height == totalHeight || !isAir(chunk, x, y + 1, z)) {
                    setBlock(chunk, x, y, z, vineHeadState("twisting_vines", random))
                    break
                }
                setBlock(chunk, x, y, z, stateId("twisting_vines_plant"))
            }
            y++
        }
    }

    private fun placeWeepingVines(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
    ) {
        repeat(10) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = random.nextInt(128)
            if (biomeSource.biomeAt(x, y, z).id.path == "crimson_forest") {
                placeWeepingVinesFeature(chunk, random, Pos(x, y, z))
            }
        }
    }

    private fun placeWeepingVinesFeature(
        chunk: Chunk,
        random: BasaltRandom,
        origin: Pos,
    ) {
        if (!isAir(chunk, origin.x, origin.y, origin.z)) return
        if (blockPathAt(chunk, origin.x, origin.y + 1, origin.z) !in WEEPING_ROOF_SUPPORT) return
        setBlock(chunk, origin.x, origin.y, origin.z, stateId("nether_wart_block"))
        repeat(200) {
            val x = origin.x + random.nextInt(6) - random.nextInt(6)
            val y = origin.y + random.nextInt(2) - random.nextInt(5)
            val z = origin.z + random.nextInt(6) - random.nextInt(6)
            if (!isAir(chunk, x, y, z)) return@repeat
            val neighbours =
                ALL_NEIGHBOURS.count { (offsetX, offsetY, offsetZ) ->
                    blockPathAt(chunk, x + offsetX, y + offsetY, z + offsetZ) in WEEPING_ROOF_SUPPORT
                }
            if (neighbours == 1) setBlock(chunk, x, y, z, stateId("nether_wart_block"))
        }
        repeat(100) {
            val x = origin.x + random.nextInt(8) - random.nextInt(8)
            val y = origin.y + random.nextInt(2) - random.nextInt(7)
            val z = origin.z + random.nextInt(8) - random.nextInt(8)
            if (!isAir(chunk, x, y, z) || blockPathAt(chunk, x, y + 1, z) !in WEEPING_ROOF_SUPPORT) return@repeat
            var vineHeight = 1 + random.nextInt(8)
            if (random.nextInt(6) == 0) vineHeight *= 2
            if (random.nextInt(5) == 0) vineHeight = 1
            placeWeepingColumn(chunk, random, x, y, z, vineHeight, 17)
        }
    }

    private fun placeWeepingColumn(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        startY: Int,
        z: Int,
        totalHeight: Int,
        minAge: Int,
    ) {
        var y = startY
        for (height in 0..totalHeight) {
            if (isAir(chunk, x, y, z)) {
                if (height == totalHeight || !isAir(chunk, x, y - 1, z)) {
                    val age = minAge + random.nextInt(26 - minAge)
                    val default = stateId("weeping_vines")
                    val head = Blocks.growingVineHeadStateId(default, age) ?: error("missing weeping vines age $age")
                    setBlock(chunk, x, y, z, head)
                    break
                }
                setBlock(chunk, x, y, z, stateId("weeping_vines_plant"))
            }
            y--
        }
    }

    private fun vineHeadState(
        path: String,
        random: BasaltRandom,
    ): Int {
        val age = 17 + random.nextInt(9)
        return Blocks.growingVineHeadStateId(stateId(path), age) ?: error("missing $path age $age")
    }

    private fun isSpringRock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = blockPathAt(chunk, x, y, z) in OVERWORLD_SPRING_ROCKS

    private fun stateAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int? =
        if (isInChunk(chunk, x, z) && y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.getBlockStateId(x and 15, y, z and 15)
        } else {
            null
        }

    private fun blockPathAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): String? = stateAt(chunk, x, y, z)?.let(::blockPath)

    private fun blockPath(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = stateAt(chunk, x, y, z)?.let(Blocks::isAir) == true

    private fun setBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (isInChunk(chunk, x, z) && y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.setBlockStateId(x and 15, y, z and 15, stateId)
        }
    }

    private fun isInChunk(
        chunk: Chunk,
        x: Int,
        z: Int,
    ): Boolean = x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ

    private fun triangle(
        random: BasaltRandom,
        range: Int,
    ): Int = random.nextInt(range + 1) - random.nextInt(range + 1)

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private data class Pos(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private enum class ForestVegetation(
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

    private enum class HugeFungus(
        val biome: String,
        val validBase: String,
        val stem: String,
        val hat: String,
        val decor: String,
    ) {
        CRIMSON("crimson_forest", "crimson_nylium", "crimson_stem", "nether_wart_block", "shroomlight"),
        WARPED("warped_forest", "warped_nylium", "warped_stem", "warped_wart_block", "shroomlight"),
    }

    private const val VEGETAL_DECORATION_STEP = 9
    private const val PATCH_ATTEMPTS = 96
    private const val NETHER_GENERATION_DEPTH = 128
    private const val HUGE_FUNGUS_PROBABILITY = 0.06f
    private const val HUGE_STEM_CORNER_PROBABILITY = 0.1f
    private const val HUGE_HAT_DROP_PROBABILITY = 0.15f
    private val ALL_BIOMES =
        setOf("nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest", "basalt_deltas")
    private val NORMAL_MUSHROOM_BIOMES = setOf("nether_wastes", "crimson_forest", "warped_forest")
    private val OVERWORLD_SPRING_ROCKS =
        setOf("stone", "granite", "diorite", "andesite", "deepslate", "tuff", "calcite", "dirt")
    private val MUSHROOM_LIGHT_OVERRIDE = setOf("mycelium", "podzol", "crimson_nylium", "warped_nylium")
    private val MUSHROOM_SOLID_SUPPORT =
        setOf(
            "netherrack",
            "basalt",
            "blackstone",
            "bedrock",
            "soul_sand",
            "soul_soil",
            "gravel",
            "magma_block",
            "nether_gold_ore",
            "nether_quartz_ore",
            "ancient_debris",
        )
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
    private val TWISTING_SUPPORT = setOf("netherrack", "warped_nylium", "warped_wart_block")
    private val WEEPING_ROOF_SUPPORT = setOf("netherrack", "nether_wart_block")
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
    private val SPRING_NEIGHBOURS =
        listOf(
            Triple(-1, 0, 0),
            Triple(1, 0, 0),
            Triple(0, 0, -1),
            Triple(0, 0, 1),
            Triple(0, -1, 0),
        )
    private val ALL_NEIGHBOURS =
        listOf(
            Triple(0, -1, 0),
            Triple(0, 1, 0),
            Triple(0, 0, -1),
            Triple(0, 0, 1),
            Triple(-1, 0, 0),
            Triple(1, 0, 0),
        )
}
