package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Mth
import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.sin

/**
 * // parity: the 28 ore placed features in the Overworld `UNDERGROUND_ORES` step.
 *
 * [externalBlockState] supplies the surrounding generation region when one is available. Ore
 * features write a few blocks across chunk borders, and discard-sensitive configurations consume
 * RNG while testing those positions. Virtual external writes are retained for the rest of this
 * decoration step so later features observe the same state transitions as vanilla.
 * [externalSurfaceHeight] has the equivalent role for the `OCEAN_FLOOR_WG` precheck. Without a
 * region provider, placement remains exact inside the chunk and deliberately does not alias an
 * outside coordinate onto the local heightmap.
 */
public object OverworldOreDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalSurfaceHeight: ((x: Int, z: Int) -> Int?)? = null,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        val region = PlacementRegion(chunk, externalSurfaceHeight, externalBlockState)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (placed in FEATURES) {
            random.setFeatureSeed(decorationSeed, placed.globalIndex, UNDERGROUND_ORES_STEP)
            placed.place(region, random, environment)
        }
    }

    private fun PlacedOre.place(
        region: PlacementRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
    ) {
        val chunk = region.chunk
        val attempts = count.sample(random)
        repeat(attempts) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = height.sample(random)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in biomes) return@repeat
            placeVein(region, random, x, y, z, feature)
        }
    }

    private fun placeVein(
        region: PlacementRegion,
        random: BasaltRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: ConfiguredOre,
    ): Boolean {
        val direction = random.nextFloat() * PI_FLOAT
        val spreadXZ = config.size / 8.0f
        val maxRadius = ceil(((config.size / 16.0f * 2.0f + 1.0f) / 2.0f).toDouble()).toInt()
        val x0 = originX + sin(direction.toDouble()) * spreadXZ
        val x1 = originX - sin(direction.toDouble()) * spreadXZ
        val z0 = originZ + cos(direction.toDouble()) * spreadXZ
        val z1 = originZ - cos(direction.toDouble()) * spreadXZ
        val y0 = originY + random.nextInt(3) - 2.0
        val y1 = originY + random.nextInt(3) - 2.0
        val spreadCeil = ceil(spreadXZ.toDouble()).toInt()
        val xStart = originX - spreadCeil - maxRadius
        val yStart = originY - 2 - maxRadius
        val zStart = originZ - spreadCeil - maxRadius
        val sizeXZ = 2 * (spreadCeil + maxRadius)
        val sizeY = 2 * (2 + maxRadius)

        if (!region.hasOceanFloorAtOrAbove(xStart, zStart, sizeXZ, yStart)) return false

        val spheres = DoubleArray(config.size * 4)
        for (index in 0 until config.size) {
            val step = index.toFloat() / config.size.toFloat()
            val sphereScale = random.nextDouble() * config.size / 16.0
            spheres[index * 4] = Mth.lerp(step.toDouble(), x0, x1)
            spheres[index * 4 + 1] = Mth.lerp(step.toDouble(), y0, y1)
            spheres[index * 4 + 2] = Mth.lerp(step.toDouble(), z0, z1)
            spheres[index * 4 + 3] = ((Mth.sin(PI_FLOAT * step) + 1.0f) * sphereScale + 1.0) / 2.0
        }
        removeContainedSpheres(spheres, config.size)

        var placed = 0
        val tested = mutableSetOf<Int>()
        for (index in 0 until config.size) {
            val radius = spheres[index * 4 + 3]
            if (radius < 0.0) continue
            val centerX = spheres[index * 4]
            val centerY = spheres[index * 4 + 1]
            val centerZ = spheres[index * 4 + 2]
            val minX = maxOf(Mth.floor(centerX - radius), xStart)
            val minY = maxOf(Mth.floor(centerY - radius), yStart)
            val minZ = maxOf(Mth.floor(centerZ - radius), zStart)
            val maxX = maxOf(Mth.floor(centerX + radius), minX)
            val maxY = maxOf(Mth.floor(centerY + radius), minY)
            val maxZ = maxOf(Mth.floor(centerZ + radius), minZ)
            for (x in minX..maxX) {
                val xd = (x + 0.5 - centerX) / radius
                if (xd * xd >= 1.0) continue
                for (y in minY..maxY) {
                    val yd = (y + 0.5 - centerY) / radius
                    if (xd * xd + yd * yd >= 1.0) continue
                    for (z in minZ..maxZ) {
                        val zd = (z + 0.5 - centerZ) / radius
                        if (xd * xd + yd * yd + zd * zd >= 1.0 || y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                            continue
                        }
                        val testedIndex =
                            x - xStart +
                                (y - yStart) * sizeXZ +
                                (z - zStart) * sizeXZ * sizeY
                        if (!tested.add(testedIndex)) continue
                        if (placeTarget(region, random, x, y, z, config)) placed++
                    }
                }
            }
        }
        return placed > 0
    }

    private fun placeTarget(
        region: PlacementRegion,
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
        config: ConfiguredOre,
    ): Boolean {
        val stateId = region.getBlockStateId(x, y, z) ?: return false
        val blockPath = Blocks.byStateId(stateId)?.block?.path ?: return false
        val target = config.targets.firstOrNull { blockPath in it.replaceable } ?: return false
        val skipAirCheck =
            when {
                config.discardChanceOnAirExposure <= 0.0f -> true
                config.discardChanceOnAirExposure >= 1.0f -> false
                else -> random.nextFloat() >= config.discardChanceOnAirExposure
            }
        if (!skipAirCheck && isAdjacentToAir(region, x, y, z)) return false
        region.setBlockStateId(x, y, z, target.stateId)
        return true
    }

    private fun removeContainedSpheres(
        spheres: DoubleArray,
        size: Int,
    ) {
        for (first in 0 until size - 1) {
            if (spheres[first * 4 + 3] <= 0.0) continue
            for (second in first + 1 until size) {
                if (spheres[second * 4 + 3] <= 0.0) continue
                val dx = spheres[first * 4] - spheres[second * 4]
                val dy = spheres[first * 4 + 1] - spheres[second * 4 + 1]
                val dz = spheres[first * 4 + 2] - spheres[second * 4 + 2]
                val radiusDelta = spheres[first * 4 + 3] - spheres[second * 4 + 3]
                if (radiusDelta * radiusDelta > dx * dx + dy * dy + dz * dz) {
                    if (radiusDelta > 0.0) spheres[second * 4 + 3] = -1.0 else spheres[first * 4 + 3] = -1.0
                }
            }
        }
    }

    private fun isAdjacentToAir(
        region: PlacementRegion,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        DIRECTIONS.any { direction ->
            val adjacentX = x + direction.first
            val adjacentY = y + direction.second
            val adjacentZ = z + direction.third
            if (adjacentY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return@any true
            region.getBlockStateId(adjacentX, adjacentY, adjacentZ)?.let(Blocks::isAir) == true
        }

    private class PlacementRegion(
        val chunk: Chunk,
        private val externalSurfaceHeight: ((x: Int, z: Int) -> Int?)?,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
    ) {
        private val localSurfaceHeight = chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR)
        private val externalWrites = HashMap<BlockPosition, Int>()

        fun hasOceanFloorAtOrAbove(
            xStart: Int,
            zStart: Int,
            sizeXZ: Int,
            yStart: Int,
        ): Boolean {
            val chunkMinX = chunk.chunkX * 16
            val chunkMinZ = chunk.chunkZ * 16
            val localMinX = maxOf(xStart, chunkMinX)
            val localMaxX = minOf(xStart + sizeXZ, chunkMinX + 15)
            val localMinZ = maxOf(zStart, chunkMinZ)
            val localMaxZ = minOf(zStart + sizeXZ, chunkMinZ + 15)
            for (x in localMinX..localMaxX) {
                for (z in localMinZ..localMaxZ) {
                    if (yStart <= localSurfaceHeight[(x and 15) + (z and 15) * 16]) return true
                }
            }
            val outsideHeight = externalSurfaceHeight ?: return false
            for (x in xStart..xStart + sizeXZ) {
                for (z in zStart..zStart + sizeXZ) {
                    if (x in chunkMinX..chunkMinX + 15 && z in chunkMinZ..chunkMinZ + 15) continue
                    if (outsideHeight(x, z)?.let { yStart <= it } == true) return true
                }
            }
            return false
        }

        fun getBlockStateId(
            x: Int,
            y: Int,
            z: Int,
        ): Int? =
            if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
                chunk.getBlockStateId(x and 15, y, z and 15)
            } else {
                externalWrites[BlockPosition(x, y, z)] ?: externalBlockState?.invoke(x, y, z)
            }

        fun setBlockStateId(
            x: Int,
            y: Int,
            z: Int,
            stateId: Int,
        ) {
            if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(x and 15, y, z and 15, stateId)
            } else {
                externalWrites[BlockPosition(x, y, z)] = stateId
            }
        }
    }

    private data class BlockPosition(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private data class Target(
        val replaceable: Set<String>,
        val stateId: Int,
    )

    private data class ConfiguredOre(
        val size: Int,
        val discardChanceOnAirExposure: Float,
        val targets: List<Target>,
    )

    private data class PlacedOre(
        val globalIndex: Int,
        val count: Count,
        val height: Height,
        val biomes: Set<String>,
        val feature: ConfiguredOre,
    )

    private fun interface Count {
        fun sample(random: BasaltRandom): Int
    }

    private fun interface Height {
        fun sample(random: BasaltRandom): Int
    }

    private fun fixedCount(value: Int): Count = Count { value }

    private fun uniformCount(
        min: Int,
        max: Int,
    ): Count = Count { random -> min + random.nextInt(max - min + 1) }

    private fun uniformHeight(
        min: Int,
        max: Int,
    ): Height = Height { random -> min + random.nextInt(max - min + 1) }

    private fun triangleHeight(
        min: Int,
        max: Int,
    ): Height =
        Height { random ->
            val range = max - min
            val firstHalf = range / 2
            val secondHalf = range - firstHalf
            min + random.nextInt(secondHalf + 1) + random.nextInt(firstHalf + 1)
        }

    private fun rarityCount(chance: Int): Count = Count { random -> if (random.nextFloat() < 1.0f / chance) 1 else 0 }

    private fun singleTarget(
        size: Int,
        discard: Float,
        block: String,
        replaceable: Set<String> = BASE_STONE_OVERWORLD,
    ): ConfiguredOre = ConfiguredOre(size, discard, listOf(Target(replaceable, stateId(block))))

    private fun pairedTarget(
        size: Int,
        discard: Float,
        stoneBlock: String,
        deepslateBlock: String,
    ): ConfiguredOre =
        ConfiguredOre(
            size,
            discard,
            listOf(
                Target(STONE_ORE_REPLACEABLES, stateId(stoneBlock)),
                Target(DEEPSLATE_ORE_REPLACEABLES, stateId(deepslateBlock)),
            ),
        )

    private fun placed(
        index: Int,
        count: Count,
        height: Height,
        feature: ConfiguredOre,
        biomes: Set<String> = ALL_BIOMES,
    ): PlacedOre = PlacedOre(index, count, height, biomes, feature)

    private const val UNDERGROUND_ORES_STEP = 6
    private const val PI_FLOAT: Float = 3.1415927f
    private val ALL_BIOMES =
        setOf(
            "plains",
            "sunflower_plains",
            "desert",
            "windswept_hills",
            "windswept_gravelly_hills",
            "windswept_forest",
            "windswept_savanna",
            "forest",
            "flower_forest",
            "birch_forest",
            "old_growth_birch_forest",
            "dark_forest",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "taiga",
            "snowy_taiga",
            "savanna",
            "savanna_plateau",
            "badlands",
            "wooded_badlands",
            "eroded_badlands",
            "jungle",
            "sparse_jungle",
            "bamboo_jungle",
            "swamp",
            "mangrove_swamp",
            "river",
            "frozen_river",
            "beach",
            "snowy_beach",
            "stony_shore",
            "snowy_plains",
            "ice_spikes",
            "ocean",
            "deep_ocean",
            "warm_ocean",
            "lukewarm_ocean",
            "deep_lukewarm_ocean",
            "cold_ocean",
            "deep_cold_ocean",
            "frozen_ocean",
            "deep_frozen_ocean",
            "mushroom_fields",
            "cherry_grove",
            "pale_garden",
            "meadow",
            "grove",
            "snowy_slopes",
            "frozen_peaks",
            "jagged_peaks",
            "stony_peaks",
            "dripstone_caves",
            "lush_caves",
            "deep_dark",
            "sulfur_caves",
        )
    private val MOUNTAIN_BIOMES =
        setOf(
            "windswept_hills",
            "windswept_gravelly_hills",
            "windswept_forest",
            "cherry_grove",
            "meadow",
            "grove",
            "snowy_slopes",
            "frozen_peaks",
            "jagged_peaks",
            "stony_peaks",
        )
    private val BADLANDS_BIOMES = setOf("badlands", "wooded_badlands", "eroded_badlands")
    private val BASE_STONE_OVERWORLD = setOf("stone", "granite", "diorite", "andesite", "tuff", "deepslate")
    private val STONE_ORE_REPLACEABLES = setOf("stone", "granite", "diorite", "andesite")
    private val DEEPSLATE_ORE_REPLACEABLES = setOf("deepslate", "tuff")
    private val DIRECTIONS =
        listOf(
            Triple(1, 0, 0),
            Triple(-1, 0, 0),
            Triple(0, 1, 0),
            Triple(0, -1, 0),
            Triple(0, 0, 1),
            Triple(0, 0, -1),
        )

    // parity: FeatureSorter global indexes for the 55 Overworld biome feature lists in 26.2.
    private val FEATURES =
        listOf(
            placed(0, fixedCount(7), uniformHeight(0, 160), singleTarget(33, 0.0f, "dirt")),
            placed(1, fixedCount(14), uniformHeight(-64, 319), singleTarget(33, 0.0f, "gravel")),
            placed(2, rarityCount(6), uniformHeight(64, 128), singleTarget(64, 0.0f, "granite")),
            placed(3, fixedCount(2), uniformHeight(0, 60), singleTarget(64, 0.0f, "granite")),
            placed(4, rarityCount(6), uniformHeight(64, 128), singleTarget(64, 0.0f, "diorite")),
            placed(5, fixedCount(2), uniformHeight(0, 60), singleTarget(64, 0.0f, "diorite")),
            placed(6, rarityCount(6), uniformHeight(64, 128), singleTarget(64, 0.0f, "andesite")),
            placed(7, fixedCount(2), uniformHeight(0, 60), singleTarget(64, 0.0f, "andesite")),
            placed(8, fixedCount(2), uniformHeight(-64, 0), singleTarget(64, 0.0f, "tuff")),
            placed(
                9,
                fixedCount(30),
                uniformHeight(136, 319),
                pairedTarget(17, 0.0f, "coal_ore", "deepslate_coal_ore"),
            ),
            placed(
                10,
                fixedCount(20),
                triangleHeight(0, 192),
                pairedTarget(17, 0.5f, "coal_ore", "deepslate_coal_ore"),
            ),
            placed(
                11,
                fixedCount(90),
                triangleHeight(80, 384),
                pairedTarget(9, 0.0f, "iron_ore", "deepslate_iron_ore"),
            ),
            placed(
                12,
                fixedCount(10),
                triangleHeight(-24, 56),
                pairedTarget(9, 0.0f, "iron_ore", "deepslate_iron_ore"),
            ),
            placed(13, fixedCount(10), uniformHeight(-64, 72), pairedTarget(4, 0.0f, "iron_ore", "deepslate_iron_ore")),
            placed(14, fixedCount(4), triangleHeight(-64, 32), pairedTarget(9, 0.5f, "gold_ore", "deepslate_gold_ore")),
            placed(
                15,
                uniformCount(0, 1),
                uniformHeight(-64, -48),
                pairedTarget(9, 0.5f, "gold_ore", "deepslate_gold_ore"),
            ),
            placed(
                16,
                fixedCount(4),
                uniformHeight(-64, 15),
                pairedTarget(8, 0.0f, "redstone_ore", "deepslate_redstone_ore"),
            ),
            placed(
                17,
                fixedCount(8),
                triangleHeight(-96, -32),
                pairedTarget(8, 0.0f, "redstone_ore", "deepslate_redstone_ore"),
            ),
            placed(
                18,
                fixedCount(7),
                triangleHeight(-144, 16),
                pairedTarget(4, 0.5f, "diamond_ore", "deepslate_diamond_ore"),
            ),
            placed(
                19,
                fixedCount(2),
                uniformHeight(-64, -4),
                pairedTarget(8, 0.5f, "diamond_ore", "deepslate_diamond_ore"),
            ),
            placed(
                20,
                rarityCount(9),
                triangleHeight(-144, 16),
                pairedTarget(12, 0.7f, "diamond_ore", "deepslate_diamond_ore"),
            ),
            placed(
                21,
                fixedCount(4),
                triangleHeight(-144, 16),
                pairedTarget(8, 1.0f, "diamond_ore", "deepslate_diamond_ore"),
            ),
            placed(
                22,
                fixedCount(2),
                triangleHeight(-32, 32),
                pairedTarget(7, 0.0f, "lapis_ore", "deepslate_lapis_ore"),
            ),
            placed(
                23,
                fixedCount(4),
                uniformHeight(-64, 64),
                pairedTarget(7, 1.0f, "lapis_ore", "deepslate_lapis_ore"),
            ),
            placed(
                24,
                fixedCount(16),
                triangleHeight(-16, 112),
                pairedTarget(20, 0.0f, "copper_ore", "deepslate_copper_ore"),
                setOf("dripstone_caves"),
            ),
            placed(
                25,
                fixedCount(16),
                triangleHeight(-16, 112),
                pairedTarget(10, 0.0f, "copper_ore", "deepslate_copper_ore"),
                ALL_BIOMES - "dripstone_caves",
            ),
            placed(27, fixedCount(46), uniformHeight(-64, 256), singleTarget(33, 0.0f, "clay"), setOf("lush_caves")),
            placed(
                28,
                fixedCount(50),
                uniformHeight(32, 256),
                pairedTarget(9, 0.0f, "gold_ore", "deepslate_gold_ore"),
                BADLANDS_BIOMES,
            ),
            placed(
                33,
                fixedCount(100),
                triangleHeight(-16, 480),
                pairedTarget(3, 0.0f, "emerald_ore", "deepslate_emerald_ore"),
                MOUNTAIN_BIOMES,
            ),
        )
}
