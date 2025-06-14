package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/**
 * // parity: the sulfur and dripstone cluster/point features, global indexes 2..5 of
 * `UNDERGROUND_DECORATION`.
 *
 * The pointed features share one configured-feature graph, and the clusters share another; only
 * biome, base/point blocks, replaceable tag, and a few providers differ. [externalBlockState]
 * models the surrounding WorldGenRegion; virtual writes are retained between attempts because
 * both feature families can extend beyond the decorating chunk.
 */
public object CaveSpeleothemDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        decorateFeatures(
            chunk,
            seed,
            environment,
            SULFUR_CLUSTER_INDEX..POINTED_DRIPSTONE_INDEX,
            externalBlockState,
        )
    }

    internal fun decorateFeatures(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        globalIndices: Iterable<Int>,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        val region = SpeleothemRegion(chunk, externalBlockState)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in globalIndices) {
            random.setFeatureSeed(decorationSeed, globalIndex, UNDERGROUND_DECORATION_STEP)
            when (globalIndex) {
                SULFUR_CLUSTER_INDEX, DRIPSTONE_CLUSTER_INDEX ->
                    placeClusters(region, random, environment, CLUSTERS_BY_INDEX.getValue(globalIndex))
                SULFUR_SPIKE_INDEX, POINTED_DRIPSTONE_INDEX ->
                    placePointedFeature(region, random, environment, POINTED_BY_INDEX.getValue(globalIndex))
            }
        }
    }

    /** The vanilla placed feature names this step draws, by the config that draws them. */
    public val PLACED_IDS: Set<String> =
        setOf("pointed_dripstone", "dripstone_cluster", "sulfur_spike", "sulfur_spike_cluster")

    /** One of them at one position, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: BasaltRandom,
        origin: DecorationPos,
        placedFeature: String,
    ): Boolean {
        val region = SpeleothemRegion(chunk, null)
        return when (placedFeature) {
            "pointed_dripstone" ->
                placeSpeleothem(region, random, origin, POINTED_BY_INDEX.getValue(POINTED_DRIPSTONE_INDEX))
            "sulfur_spike" -> placeSpeleothem(region, random, origin, POINTED_BY_INDEX.getValue(SULFUR_SPIKE_INDEX))
            "dripstone_cluster" ->
                placeCluster(region, random, origin, CLUSTERS.first { it.globalIndex == DRIPSTONE_CLUSTER_INDEX })
            "sulfur_spike_cluster" ->
                placeCluster(region, random, origin, CLUSTERS.first { it.globalIndex == SULFUR_CLUSTER_INDEX })
            else -> false
        }
    }

    private fun placePointedFeature(
        region: SpeleothemRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
        config: SpeleothemConfig,
    ) {
        val count = 192 + random.nextInt(65)
        repeat(count) {
            val x = region.chunk.chunkX * 16 + random.nextInt(16)
            val z = region.chunk.chunkZ * 16 + random.nextInt(16)
            val origin =
                DecorationPos(
                    x,
                    WorldHeight.MIN_Y + random.nextInt(321),
                    z,
                )
            val nestedCount = 1 + random.nextInt(5)
            repeat(nestedCount) nested@{
                val offset =
                    origin.offset(
                        clampedNormal(random, 3.0f, -10, 10),
                        clampedNormal(random, 0.6f, -2, 2),
                        clampedNormal(random, 3.0f, -10, 10),
                    )
                if (environment.biomeAt(offset.x, offset.y, offset.z).substringAfter(':') != config.biome) {
                    return@nested
                }
                val scanDirection = if (random.nextInt(2) == 0) DOWN else UP
                val solid = scanForSolid(region, offset, scanDirection) ?: return@nested
                placeSpeleothem(region, random, solid.offset(0, -scanDirection, 0), config)
            }
        }
    }

    private fun placeClusters(
        region: SpeleothemRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
        config: ClusterConfig,
    ) {
        val count = 48 + random.nextInt(49)
        repeat(count) {
            val x = region.chunk.chunkX * 16 + random.nextInt(16)
            val z = region.chunk.chunkZ * 16 + random.nextInt(16)
            val origin = DecorationPos(x, WorldHeight.MIN_Y + random.nextInt(321), z)
            if (environment.biomeAt(x, origin.y, z).substringAfter(':') != config.speleothem.biome) return@repeat
            placeCluster(region, random, origin, config)
        }
    }

    private fun placeCluster(
        region: SpeleothemRegion,
        random: BasaltRandom,
        origin: DecorationPos,
        config: ClusterConfig,
    ): Boolean {
        if (!region.isEmptyOrWater(origin)) return false
        val height = config.minHeight + random.nextInt(config.maxHeight - config.minHeight + 1)
        val wetness =
            if (config.wetnessDeviation == 0.0f) {
                config.wetnessMean
            } else {
                clampedNormalFloat(
                    random,
                    config.wetnessMean,
                    config.wetnessDeviation,
                    config.minWetness,
                    config.maxWetness,
                )
            }
        val density = 0.3f + random.nextFloat() * 0.4f
        val xRadius = 2 + random.nextInt(7)
        val zRadius = 2 + random.nextInt(7)
        for (dx in -xRadius..xRadius) {
            for (dz in -zRadius..zRadius) {
                val chance = chanceOfSpeleothem(xRadius, zRadius, dx, dz)
                placeColumn(region, random, origin.offset(dx, 0, dz), dx, dz, wetness, chance, height, density, config)
            }
        }
        return true
    }

    private fun placeColumn(
        region: SpeleothemRegion,
        random: BasaltRandom,
        origin: DecorationPos,
        dx: Int,
        dz: Int,
        wetness: Float,
        chance: Double,
        clusterHeight: Int,
        density: Float,
        config: ClusterConfig,
    ) {
        val baseColumn = scanColumn(region, origin, 12) ?: return
        if (baseColumn.ceiling == null && baseColumn.floor == null) return
        val ceiling = baseColumn.ceiling
        val wantPool = random.nextFloat() < wetness
        val column =
            if (wantPool &&
                baseColumn.floor != null &&
                canPlacePool(region, origin.copy(y = baseColumn.floor), config)
            ) {
                region.set(origin.copy(y = baseColumn.floor), Blocks.WATER.stateId)
                baseColumn.copy(floor = baseColumn.floor - 1)
            } else {
                baseColumn
            }
        val floor = column.floor

        val wantStalactite = random.nextDouble() < chance
        val stalactiteHeight =
            if (ceiling != null && wantStalactite && !region.isLava(origin.copy(y = ceiling))) {
                val thickness = 2 + random.nextInt(3)
                replaceWithBase(region, origin.copy(y = ceiling), thickness, UP, config.speleothem)
                val maxHeight = if (floor != null) minOf(clusterHeight, ceiling - floor) else clusterHeight
                speleothemHeight(random, dx, dz, density, maxHeight)
            } else {
                0
            }

        val wantStalagmite = random.nextDouble() < chance
        val stalagmiteHeight =
            if (floor != null && wantStalagmite && !region.isLava(origin.copy(y = floor))) {
                val thickness = 2 + random.nextInt(3)
                replaceWithBase(region, origin.copy(y = floor), thickness, DOWN, config.speleothem)
                if (ceiling != null) {
                    maxOf(0, stalactiteHeight + random.nextInt(3) - 1)
                } else {
                    speleothemHeight(random, dx, dz, density, clusterHeight)
                }
            } else {
                0
            }

        var actualStalactiteHeight = stalactiteHeight
        var actualStalagmiteHeight = stalagmiteHeight
        if (ceiling != null && floor != null && ceiling - stalactiteHeight <= floor + stalagmiteHeight) {
            val lowestStalactiteBottom = maxOf(ceiling - stalactiteHeight, floor + 1)
            val highestStalagmiteTop = minOf(floor + stalagmiteHeight, ceiling - 1)
            val actualStalactiteBottom =
                lowestStalactiteBottom + random.nextInt(highestStalagmiteTop + 1 - lowestStalactiteBottom + 1)
            actualStalagmiteHeight = actualStalactiteBottom - 1 - floor
            actualStalactiteHeight = ceiling - actualStalactiteBottom
        }

        val mergeTips =
            random.nextBoolean() &&
                actualStalactiteHeight > 0 &&
                actualStalagmiteHeight > 0 &&
                column.height != null &&
                actualStalactiteHeight + actualStalagmiteHeight == column.height
        if (ceiling != null) {
            growSpeleothem(
                region,
                origin.copy(y = ceiling - 1),
                DOWN,
                actualStalactiteHeight,
                config.speleothem,
                mergeTips,
            )
        }
        if (floor != null) {
            growSpeleothem(
                region,
                origin.copy(y = floor + 1),
                UP,
                actualStalagmiteHeight,
                config.speleothem,
                mergeTips,
            )
        }
    }

    private fun scanColumn(
        region: SpeleothemRegion,
        origin: DecorationPos,
        searchRange: Int,
    ): CaveColumn? {
        if (!region.isEmptyOrWater(origin)) return null

        fun scan(direction: Int): Int? {
            var position = origin
            for (ignored in 1 until searchRange) {
                if (!region.isEmptyOrWater(position)) break
                position = position.offset(0, direction, 0)
            }
            return position.y.takeIf { region.isNeitherEmptyNorWater(position) }
        }

        return CaveColumn(scan(DOWN), scan(UP))
    }

    private fun canPlacePool(
        region: SpeleothemRegion,
        floor: DecorationPos,
        config: ClusterConfig,
    ): Boolean {
        val stateId = region.get(floor) ?: return false
        val blockPath = Blocks.byStateId(stateId)?.block?.path
        if (stateId == Blocks.WATER.stateId ||
            stateId == config.speleothem.baseStateId ||
            blockPath == config.speleothem.pointPath
        ) {
            return false
        }
        if (region.hasWater(floor.offset(0, 1, 0))) return false
        for (direction in HORIZONTAL_DIRECTIONS) {
            if (!region.canBeAdjacentToWater(floor.offset(direction.x, direction.y, direction.z))) return false
        }
        return region.canBeAdjacentToWater(floor.offset(0, -1, 0))
    }

    private fun replaceWithBase(
        region: SpeleothemRegion,
        first: DecorationPos,
        maxCount: Int,
        direction: Int,
        config: SpeleothemConfig,
    ) {
        var position = first
        repeat(maxCount) {
            if (region.blockPath(position) !in config.replaceable) return
            region.set(position, config.baseStateId)
            position = position.offset(0, direction, 0)
        }
    }

    private fun speleothemHeight(
        random: BasaltRandom,
        dx: Int,
        dz: Int,
        density: Float,
        maxHeight: Int,
    ): Int {
        if (random.nextFloat() > density) return 0
        val distanceFromCenter = kotlin.math.abs(dx) + kotlin.math.abs(dz)
        val heightMean =
            clampedMap(
                distanceFromCenter.toDouble(),
                0.0,
                8.0,
                maxHeight / 2.0,
                0.0,
            ).toFloat()
        return clampedNormalFloat(random, heightMean, 3.0f, 0.0f, maxHeight.toFloat()).toInt()
    }

    private fun chanceOfSpeleothem(
        xRadius: Int,
        zRadius: Int,
        dx: Int,
        dz: Int,
    ): Double {
        val distanceFromEdge = minOf(xRadius - kotlin.math.abs(dx), zRadius - kotlin.math.abs(dz))
        return clampedMap(distanceFromEdge.toFloat(), 0.0f, 3.0f, 0.1f, 1.0f).toDouble()
    }

    private fun scanForSolid(
        region: SpeleothemRegion,
        origin: DecorationPos,
        direction: Int,
    ): DecorationPos? {
        var position = origin
        if (!region.isEmptyOrWater(position)) return null
        for (ignored in 0 until 12) {
            if (region.isSolid(position)) return position
            position = position.offset(0, direction, 0)
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return null
            if (!region.isEmptyOrWater(position)) break
        }
        return position.takeIf(region::isSolid)
    }

    private fun placeSpeleothem(
        region: SpeleothemRegion,
        random: BasaltRandom,
        origin: DecorationPos,
        config: SpeleothemConfig,
    ): Boolean {
        val canPlaceAbove = region.isBase(origin.offset(0, 1, 0), config)
        val canPlaceBelow = region.isBase(origin.offset(0, -1, 0), config)
        val tipDirection =
            when {
                canPlaceAbove && canPlaceBelow -> if (random.nextBoolean()) DOWN else UP
                canPlaceAbove -> DOWN
                canPlaceBelow -> UP
                else -> return false
            }

        val root = origin.offset(0, -tipDirection, 0)
        placeBaseIfPossible(region, root, config)
        for (direction in HORIZONTAL_DIRECTIONS) {
            if (random.nextFloat() <= 0.7f) {
                val radius1 = root.offset(direction.x, direction.y, direction.z)
                placeBaseIfPossible(region, radius1, config)
                if (random.nextFloat() <= 0.5f) {
                    val radius2Direction = DIRECTIONS[random.nextInt(DIRECTIONS.size)]
                    val radius2 = radius1.offset(radius2Direction.x, radius2Direction.y, radius2Direction.z)
                    placeBaseIfPossible(region, radius2, config)
                    if (random.nextFloat() <= 0.5f) {
                        val radius3Direction = DIRECTIONS[random.nextInt(DIRECTIONS.size)]
                        placeBaseIfPossible(
                            region,
                            radius2.offset(radius3Direction.x, radius3Direction.y, radius3Direction.z),
                            config,
                        )
                    }
                }
            }
        }

        val height =
            if (random.nextFloat() < 0.2f && region.isEmptyOrWater(origin.offset(0, tipDirection, 0))) 2 else 1
        growSpeleothem(region, origin, tipDirection, height, config)
        return true
    }

    private fun placeBaseIfPossible(
        region: SpeleothemRegion,
        position: DecorationPos,
        config: SpeleothemConfig,
    ) {
        if (region.blockPath(position) in config.replaceable) region.set(position, config.baseStateId)
    }

    private fun growSpeleothem(
        region: SpeleothemRegion,
        origin: DecorationPos,
        tipDirection: Int,
        height: Int,
        config: SpeleothemConfig,
        mergedTip: Boolean = false,
    ) {
        if (!region.isBase(origin.offset(0, -tipDirection, 0), config)) return
        var position = origin

        fun place(thickness: String) {
            region.set(position, pointedStateId(config.pointPath, thickness, tipDirection, region.hasWater(position)))
            position = position.offset(0, tipDirection, 0)
        }
        if (height >= 3) {
            place("base")
            repeat(height - 3) { place("middle") }
        }
        if (height >= 2) {
            place("frustum")
        }
        if (height >= 1) {
            place(if (mergedTip) "tip_merge" else "tip")
        }
    }

    private fun clampedNormal(
        random: BasaltRandom,
        deviation: Float,
        minInclusive: Int,
        maxInclusive: Int,
    ): Int =
        (random.nextGaussian().toFloat() * deviation).coerceIn(minInclusive.toFloat(), maxInclusive.toFloat()).toInt()

    private fun clampedNormalFloat(
        random: BasaltRandom,
        mean: Float,
        deviation: Float,
        minInclusive: Float,
        maxInclusive: Float,
    ): Float = (mean + random.nextGaussian().toFloat() * deviation).coerceIn(minInclusive, maxInclusive)

    private fun clampedMap(
        value: Double,
        fromMin: Double,
        fromMax: Double,
        toMin: Double,
        toMax: Double,
    ): Double {
        val factor = (value - fromMin) / (fromMax - fromMin)
        return when {
            factor < 0.0 -> toMin
            factor > 1.0 -> toMax
            else -> toMin + factor * (toMax - toMin)
        }
    }

    private fun clampedMap(
        value: Float,
        fromMin: Float,
        fromMax: Float,
        toMin: Float,
        toMax: Float,
    ): Float {
        val factor = (value - fromMin) / (fromMax - fromMin)
        return when {
            factor < 0.0f -> toMin
            factor > 1.0f -> toMax
            else -> toMin + factor * (toMax - toMin)
        }
    }

    private fun pointedStateId(
        path: String,
        thickness: String,
        tipDirection: Int,
        waterlogged: Boolean,
    ): Int =
        Blocks.stateIdOf(
            Identifier.vanilla(path),
            linkedMapOf(
                "thickness" to thickness,
                "vertical_direction" to if (tipDirection == UP) "up" else "down",
                "waterlogged" to waterlogged.toString(),
            ),
        ) ?: error("missing $path state")

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private data class SpeleothemConfig(
        val globalIndex: Int,
        val biome: String,
        val baseStateId: Int,
        val pointPath: String,
        val replaceable: Set<String>,
    )

    private data class ClusterConfig(
        val globalIndex: Int,
        val speleothem: SpeleothemConfig,
        val minHeight: Int,
        val maxHeight: Int,
        val wetnessMean: Float,
        val wetnessDeviation: Float,
        val minWetness: Float,
        val maxWetness: Float,
    )

    private data class CaveColumn(
        val floor: Int?,
        val ceiling: Int?,
    ) {
        val height: Int? get() = if (floor != null && ceiling != null) ceiling - floor - 1 else null
    }

    private data class Direction(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private class SpeleothemRegion(
        val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()

        fun get(position: DecorationPos): Int? =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else -> externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
            }

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
            } else {
                externalWrites[position] = stateId
            }
        }

        fun blockPath(position: DecorationPos): String? = get(position)?.let(Blocks::byStateId)?.block?.path

        fun isEmptyOrWater(position: DecorationPos): Boolean {
            val stateId = get(position) ?: return false
            return Blocks.isAir(stateId) || stateId == Blocks.WATER.stateId
        }

        fun isSolid(position: DecorationPos): Boolean {
            val stateId = get(position) ?: return false
            return Chunk.HeightmapType.OCEAN_FLOOR.matches(stateId)
        }

        fun isNeitherEmptyNorWater(position: DecorationPos): Boolean {
            val stateId = get(position) ?: return false
            return !Blocks.isAir(stateId) && stateId != Blocks.WATER.stateId
        }

        fun isLava(position: DecorationPos): Boolean = get(position)?.let(Blocks::byStateId)?.block?.path == "lava"

        fun hasWater(position: DecorationPos): Boolean = get(position)?.let(Blocks::hasWaterFluid) == true

        fun canBeAdjacentToWater(position: DecorationPos): Boolean =
            blockPath(position) in BASE_STONE_OVERWORLD || hasWater(position)

        fun isBase(
            position: DecorationPos,
            config: SpeleothemConfig,
        ): Boolean {
            val stateId = get(position) ?: return false
            return stateId == config.baseStateId || Blocks.byStateId(stateId)?.block?.path in config.replaceable
        }
    }

    private const val UNDERGROUND_DECORATION_STEP: Int = 7
    private const val SULFUR_CLUSTER_INDEX: Int = 2
    private const val SULFUR_SPIKE_INDEX: Int = 3
    private const val DRIPSTONE_CLUSTER_INDEX: Int = 4
    private const val POINTED_DRIPSTONE_INDEX: Int = 5
    private const val DOWN: Int = -1
    private const val UP: Int = 1
    private val DIRECTIONS =
        listOf(
            Direction(0, -1, 0),
            Direction(0, 1, 0),
            Direction(0, 0, -1),
            Direction(0, 0, 1),
            Direction(-1, 0, 0),
            Direction(1, 0, 0),
        )
    private val HORIZONTAL_DIRECTIONS = listOf(DIRECTIONS[2], DIRECTIONS[5], DIRECTIONS[3], DIRECTIONS[4])
    private val BASE_STONE_OVERWORLD = setOf("stone", "granite", "diorite", "andesite", "tuff", "deepslate")
    private val POINTED_FEATURES =
        listOf(
            SpeleothemConfig(
                SULFUR_SPIKE_INDEX,
                "sulfur_caves",
                stateId("sulfur"),
                "sulfur_spike",
                setOf("sulfur", "cinnabar"),
            ),
            SpeleothemConfig(
                POINTED_DRIPSTONE_INDEX,
                "dripstone_caves",
                stateId("dripstone_block"),
                "pointed_dripstone",
                BASE_STONE_OVERWORLD,
            ),
        )
    private val POINTED_BY_INDEX = POINTED_FEATURES.associateBy(SpeleothemConfig::globalIndex)
    private val CLUSTERS =
        listOf(
            ClusterConfig(
                SULFUR_CLUSTER_INDEX,
                POINTED_BY_INDEX.getValue(SULFUR_SPIKE_INDEX),
                minHeight = 1,
                maxHeight = 4,
                wetnessMean = 0.0f,
                wetnessDeviation = 0.0f,
                minWetness = 0.0f,
                maxWetness = 0.0f,
            ),
            ClusterConfig(
                DRIPSTONE_CLUSTER_INDEX,
                POINTED_BY_INDEX.getValue(POINTED_DRIPSTONE_INDEX),
                minHeight = 3,
                maxHeight = 6,
                wetnessMean = 0.1f,
                wetnessDeviation = 0.3f,
                minWetness = 0.1f,
                maxWetness = 0.9f,
            ),
        )
    private val CLUSTERS_BY_INDEX = CLUSTERS.associateBy(ClusterConfig::globalIndex)
}
