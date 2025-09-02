package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.Mth
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.sin

/**
 * // parity: the 28 ore, underwater magma, and four disk placed features in the Overworld
 * `UNDERGROUND_ORES` step.
 *
 * [externalBlockState] and [externalSurfaceHeight] read the surrounding region when there is one:
 * ores write across chunk borders and consume RNG testing those positions, so the writes are kept
 * for the rest of the step and later features see what vanilla saw. Without a region provider,
 * placement stays exact inside the chunk rather than aliasing an outside coordinate onto it.
 */
public object OverworldOreDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalSurfaceHeight: ((x: Int, z: Int) -> Int?)? = null,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        decorateFeatures(chunk, seed, environment, 0..LAST_GLOBAL_INDEX, externalSurfaceHeight, externalBlockState)
    }

    internal fun decorateFeatures(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        globalIndices: Iterable<Int>,
        externalSurfaceHeight: ((x: Int, z: Int) -> Int?)? = null,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        val region = PlacementRegion(chunk, externalSurfaceHeight, externalBlockState)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in globalIndices) {
            random.setFeatureSeed(decorationSeed, globalIndex, UNDERGROUND_ORES_STEP)
            when (globalIndex) {
                UNDERWATER_MAGMA_INDEX -> placeUnderwaterMagma(region, random, environment)
                DISK_GRASS_INDEX -> placeDiskGrass(region, random, environment)
                DISK_SAND_INDEX -> placeWaterDisk(region, random, environment, SAND_DISK, 3, DEFAULT_DISK_BIOMES)
                DISK_CLAY_INDEX -> placeWaterDisk(region, random, environment, CLAY_DISK, 1, ALL_BIOMES)
                DISK_GRAVEL_INDEX -> placeWaterDisk(region, random, environment, GRAVEL_DISK, 1, DEFAULT_DISK_BIOMES)
                else -> FEATURES_BY_INDEX[globalIndex]?.place(region, random, environment)
            }
        }
    }

    /** parity: one complete configured `underwater_magma` body at the supplied placement origin. */
    public fun placeUnderwaterMagmaAt(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val region = PlacementRegion(chunk, null, null)
        return placeUnderwaterMagmaBody(region, random, x, y, z)
    }

    /** The vanilla placed feature names of the disks this step runs. */
    public val PLACED_DISK_IDS: Set<String> = setOf("disk_sand", "disk_clay", "disk_gravel", "disk_grass")

    /** One disk at one position, which is the feature without its scatter. */
    public fun placeDiskAt(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        placedFeature: String,
    ): Boolean {
        val disk =
            when (placedFeature) {
                "disk_sand" -> SAND_DISK
                "disk_clay" -> CLAY_DISK
                "disk_gravel" -> GRAVEL_DISK
                "disk_grass" -> GRASS_DISK
                else -> return false
            }
        placeDisk(PlacementRegion(chunk, null, null), random, x, y, z, disk)
        return true
    }

    /**
     * The vanilla placed feature names of the ore features this step runs, by their global index.
     *
     * The index is how the decoration step addresses them (it is what seeds each feature's own
     * random); the name is what `/place feature` says. Both exist because they answer different
     * questions, and pairing them here keeps the pairing next to the table it refers to.
     */
    public val PLACED_ORE_IDS: Map<String, Int> =
        mapOf(
            "ore_dirt" to 0,
            "ore_gravel" to 1,
            "ore_granite_upper" to 2,
            "ore_granite_lower" to 3,
            "ore_diorite_upper" to 4,
            "ore_diorite_lower" to 5,
            "ore_andesite_upper" to 6,
            "ore_andesite_lower" to 7,
            "ore_tuff" to 8,
            "ore_coal_upper" to 9,
            "ore_coal_lower" to 10,
            "ore_iron_upper" to 11,
            "ore_iron_middle" to 12,
            "ore_iron_small" to 13,
            "ore_gold" to 14,
            "ore_gold_lower" to 15,
            "ore_redstone" to 16,
            "ore_redstone_lower" to 17,
            "ore_diamond" to 18,
            "ore_diamond_medium" to 19,
            "ore_diamond_large" to 20,
            "ore_diamond_buried" to 21,
            "ore_lapis" to 22,
            "ore_lapis_buried" to 23,
            "ore_copper_large" to 24,
            "ore_copper" to 25,
            "ore_clay" to 27,
            "ore_gold_extra" to 28,
            "ore_emerald" to 33,
            "ore_infested" to INFESTED_GLOBAL_INDEX,
        )

    /**
     * One ore vein at one position, which is the feature itself without the placement modifiers
     * that decide where generation puts it: what `/place feature` runs.
     */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        placedFeature: String,
    ): Boolean {
        val index = PLACED_ORE_IDS[placedFeature] ?: return false
        val ore = if (index == INFESTED_GLOBAL_INDEX) INFESTED_ORE else FEATURES_BY_INDEX[index] ?: return false
        return placeVein(PlacementRegion(chunk, null, null), random, x, y, z, ore.feature)
    }

    /** // parity: `ore_infested`, global feature index 6 in `UNDERGROUND_DECORATION`. */
    public fun decorateInfested(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalSurfaceHeight: ((x: Int, z: Int) -> Int?)? = null,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        val region = PlacementRegion(chunk, externalSurfaceHeight, externalBlockState)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, INFESTED_GLOBAL_INDEX, UNDERGROUND_DECORATION_STEP)
        INFESTED_ORE.place(region, random, environment)
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
            // parity: each distance is divided by the radius and compared against 1, which is not
            // the same as comparing squared distances against a squared radius once floating point
            // is involved. A blob this size is mostly edge, and which edge cells are visited also
            // decides how many draws the air-exposure check makes.
            for (x in minX..maxX) {
                val xd = (x + 0.5 - centerX) / radius
                if (xd * xd >= 1.0) continue
                for (y in minY..maxY) {
                    val yd = (y + 0.5 - centerY) / radius
                    if (xd * xd + yd * yd >= 1.0) continue
                    for (z in minZ..maxZ) {
                        val zd = (z + 0.5 - centerZ) / radius
                        if (xd * xd + yd * yd + zd * zd >= 1.0 ||
                            y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y
                        ) {
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

    private const val UNDERGROUND_ORES_STEP = 6
    private const val UNDERGROUND_DECORATION_STEP = 7
    private const val UNDERWATER_MAGMA_INDEX = 26
    private const val DISK_GRASS_INDEX = 29
    private const val DISK_SAND_INDEX = 30
    private const val DISK_CLAY_INDEX = 31
    private const val DISK_GRAVEL_INDEX = 32
    private const val LAST_GLOBAL_INDEX = 33
    private const val PI_FLOAT: Float = 3.1415927f
    private val DIRECTIONS =
        listOf(
            Triple(1, 0, 0),
            Triple(-1, 0, 0),
            Triple(0, 1, 0),
            Triple(0, -1, 0),
            Triple(0, 0, 1),
            Triple(0, 0, -1),
        )
}

// parity: global feature index of `ore_infested` / `INFESTED_GLOBAL_INDEX` in `UNDERGROUND_DECORATION`.
internal const val INFESTED_GLOBAL_INDEX: Int = 6

internal class PlacementRegion(
    val chunk: Chunk,
    private val externalSurfaceHeight: ((x: Int, z: Int) -> Int?)?,
    private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
) {
    private val externalWrites = HashMap<BlockPosition, Int>()

    // parity: the preflight asks the level, so it sees the blobs this very step has already put
    // down. A copy taken when the region was built freezes the surface as it was before any ore.
    fun oceanFloorHeight(
        x: Int,
        z: Int,
    ): Int? =
        if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
            chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR)
        } else {
            externalSurfaceHeight?.invoke(x, z)
        }

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
                if (yStart <= chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR)) return true
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
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            Blocks.AIR.stateId
        } else if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
            chunk.getBlockStateId(x and 15, y, z and 15)
        } else {
            externalWrites[BlockPosition(x, y, z)] ?: externalBlockState?.invoke(x, y, z)
        }

    /**
     * An ore or stone blob is a sphere, so most cross a chunk edge. Keeping the outside only in
     * [externalWrites] threw those blocks away, costing about 30% of every large blob against vanilla
     * 26.2, so it now goes to the deferred pipeline guarded by the very block the blob replaced.
     */
    fun setBlockStateId(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
            chunk.setBlockStateId(x and 15, y, z and 15, stateId)
            return
        }
        val position = BlockPosition(x, y, z)
        val observed = externalWrites[position] ?: externalBlockState?.invoke(x, y, z) ?: Blocks.AIR.stateId
        externalWrites[position] = stateId
        chunk.setDecorationBlockStateId(x, y, z, stateId, expectedExternalStateId = observed)
    }
}

internal data class BlockPosition(
    val x: Int,
    val y: Int,
    val z: Int,
)

internal fun stateId(path: String): Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
        ?: error("missing vanilla block minecraft:$path")
