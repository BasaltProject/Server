package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeSource
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.Mth
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin

/** // parity: the ten Nether ore placed features in UNDERGROUND_DECORATION. */
public object NetherOreDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeSource,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (placed in FEATURES) {
            random.setFeatureSeed(decorationSeed, placed.globalIndex, UNDERGROUND_DECORATION_STEP)
            placed.place(chunk, random, biomeSource)
        }
    }

    internal fun placeFeature(
        globalIndex: Int,
        chunk: Chunk,
        random: WorldgenRandom,
        biomeSource: NetherBiomeSource,
    ): Boolean {
        val placed = FEATURES_BY_INDEX[globalIndex] ?: return false
        placed.place(chunk, random, biomeSource)
        return true
    }

    private fun PlacedOre.place(
        chunk: Chunk,
        random: WorldgenRandom,
        biomeSource: NetherBiomeSource,
    ) {
        repeat(count) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = height.sample(random)
            if (biomeSource.biomeAt(x, y, z).id.path !in biomes) return@repeat
            when (val configured = feature) {
                is ConfiguredOre.Vein -> placeVein(chunk, random, x, y, z, configured)
                is ConfiguredOre.Scattered -> placeScattered(chunk, random, x, y, z, configured)
            }
        }
    }

    private fun placeVein(
        chunk: Chunk,
        random: BasaltRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: ConfiguredOre.Vein,
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

        // The Nether's protected solid roof makes OreFeature's OCEAN_FLOOR precheck true for
        // every valid configured height. It consumes no random values, so it folds away here.
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
                        val outsideSphere = xd * xd + yd * yd + zd * zd >= 1.0
                        val outsideHeight = y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY
                        if (outsideSphere || outsideHeight) continue
                        val testedIndex =
                            x - xStart +
                                (y - yStart) * sizeXZ +
                                (z - zStart) * sizeXZ * sizeY
                        if (!tested.add(testedIndex) ||
                            !isInChunk(chunk, x, z) ||
                            y <= chunk.verticalBounds.minY
                        ) {
                            continue
                        }
                        if (canPlace(chunk, x, y, z, config.target, discardOnAirExposure = false)) {
                            chunk.setBlockStateId(x and 15, y, z and 15, config.stateId)
                            placed++
                        }
                    }
                }
            }
        }
        return placed > 0
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

    private fun placeScattered(
        chunk: Chunk,
        random: BasaltRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        config: ConfiguredOre.Scattered,
    ) {
        val tries = random.nextInt(config.size + 1)
        for (index in 0 until tries) {
            val distance = minOf(index, 7)
            val x = originX + randomOffset(random, distance)
            val y = originY + randomOffset(random, distance)
            val z = originZ + randomOffset(random, distance)
            if (!isInChunk(chunk, x, z) || y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) continue
            if (canPlace(chunk, x, y, z, config.target, discardOnAirExposure = true)) {
                chunk.setBlockStateId(x and 15, y, z and 15, config.stateId)
            }
        }
    }

    private fun randomOffset(
        random: BasaltRandom,
        distance: Int,
    ): Int {
        val value = (random.nextFloat() - random.nextFloat()) * distance.toFloat()
        return floor(value.toDouble() + 0.5).toInt()
    }

    private fun canPlace(
        chunk: Chunk,
        worldX: Int,
        y: Int,
        worldZ: Int,
        target: Target,
        discardOnAirExposure: Boolean,
    ): Boolean {
        val stateId = chunk.getBlockStateId(worldX and 15, y, worldZ and 15)
        val blockPath = Blocks.byStateId(stateId)?.block?.path ?: return false
        val targetMatches =
            when (target) {
                Target.NETHERRACK -> blockPath == "netherrack"
                Target.BASE_STONE_NETHER -> blockPath in BASE_STONE_NETHER
            }
        return targetMatches && (!discardOnAirExposure || !isAdjacentToAir(chunk, worldX, y, worldZ))
    }

    private fun isAdjacentToAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        DIRECTIONS.any { direction ->
            val adjacentX = x + direction.first
            val adjacentY = y + direction.second
            val adjacentZ = z + direction.third
            val insideChunk = isInChunk(chunk, adjacentX, adjacentZ)
            val insideHeight = adjacentY in chunk.verticalBounds.minY..chunk.verticalBounds.maxY
            if (!insideChunk || !insideHeight) {
                false
            } else {
                Blocks.isAir(chunk.getBlockStateId(adjacentX and 15, adjacentY, adjacentZ and 15))
            }
        }

    private fun isInChunk(
        chunk: Chunk,
        worldX: Int,
        worldZ: Int,
    ): Boolean = worldX shr 4 == chunk.chunkX && worldZ shr 4 == chunk.chunkZ

    private fun stateId(path: String): Int {
        val state = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))
        return state?.stateId ?: error("missing vanilla block minecraft:$path")
    }

    private sealed interface ConfiguredOre {
        val size: Int
        val stateId: Int
        val target: Target

        data class Vein(
            override val size: Int,
            override val stateId: Int,
            override val target: Target = Target.NETHERRACK,
        ) : ConfiguredOre

        data class Scattered(
            override val size: Int,
            override val stateId: Int,
            override val target: Target = Target.BASE_STONE_NETHER,
        ) : ConfiguredOre
    }

    private data class PlacedOre(
        val globalIndex: Int,
        val count: Int,
        val height: Height,
        val biomes: Set<String>,
        val feature: ConfiguredOre,
    )

    private fun interface Height {
        fun sample(random: BasaltRandom): Int
    }

    private enum class Target {
        NETHERRACK,
        BASE_STONE_NETHER,
    }

    private const val UNDERGROUND_DECORATION_STEP = 7
    private const val PI_FLOAT: Float = 3.1415927f
    private val ALL_BIOMES =
        setOf(
            "nether_wastes",
            "soul_sand_valley",
            "crimson_forest",
            "warped_forest",
            "basalt_deltas",
        )
    private val NON_DELTA_BIOMES = ALL_BIOMES - "basalt_deltas"
    private val BASE_STONE_NETHER = setOf("netherrack", "basalt", "blackstone")
    private val DIRECTIONS =
        listOf(
            Triple(1, 0, 0),
            Triple(-1, 0, 0),
            Triple(0, 1, 0),
            Triple(0, -1, 0),
            Triple(0, 0, 1),
            Triple(0, 0, -1),
        )

    private fun uniform(
        min: Int,
        max: Int,
    ): Height = Height { random -> min + random.nextInt(max - min + 1) }

    private val triangle8To24 = Height { random -> 8 + random.nextInt(9) + random.nextInt(9) }

    private fun vein(
        index: Int,
        count: Int,
        height: Height,
        biomes: Set<String>,
        size: Int,
        block: String,
    ): PlacedOre = PlacedOre(index, count, height, biomes, ConfiguredOre.Vein(size, stateId(block)))

    private fun scattered(
        index: Int,
        height: Height,
        size: Int,
    ): PlacedOre = PlacedOre(index, 1, height, ALL_BIOMES, ConfiguredOre.Scattered(size, stateId("ancient_debris")))

    // parity: FeatureSorter global indexes for the five Nether biome feature lists in 26.2.
    private val FEATURES =
        listOf(
            vein(11, 4, uniform(27, 36), ALL_BIOMES, 33, "magma_block"),
            vein(13, 20, uniform(10, 117), setOf("basalt_deltas"), 10, "nether_gold_ore"),
            vein(14, 32, uniform(10, 117), setOf("basalt_deltas"), 14, "nether_quartz_ore"),
            vein(16, 12, uniform(0, 31), setOf("soul_sand_valley"), 12, "soul_sand"),
            vein(17, 2, uniform(5, 41), NON_DELTA_BIOMES, 33, "gravel"),
            vein(18, 2, uniform(5, 31), NON_DELTA_BIOMES, 33, "blackstone"),
            vein(19, 10, uniform(10, 117), NON_DELTA_BIOMES, 10, "nether_gold_ore"),
            vein(20, 16, uniform(10, 117), NON_DELTA_BIOMES, 14, "nether_quartz_ore"),
            scattered(21, triangle8To24, 3),
            scattered(22, uniform(8, 119), 2),
        )
    private val FEATURES_BY_INDEX = FEATURES.associateBy(PlacedOre::globalIndex)
}
