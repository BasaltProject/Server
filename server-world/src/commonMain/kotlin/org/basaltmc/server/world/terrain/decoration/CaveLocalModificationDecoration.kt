package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.Mth
import kotlin.math.PI
import kotlin.math.ln
import kotlin.math.pow
import kotlin.math.sqrt

/** // parity: `amethyst_geode` and `large_dripstone`, global indexes 2 and 3 of `LOCAL_MODIFICATIONS`. */
public object CaveLocalModificationDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalSurfaceHeight: ((x: Int, z: Int) -> Int?)? = null,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        decorateFeatures(chunk, seed, environment, 2..3, externalSurfaceHeight, externalBlockState)
    }

    internal fun decorateFeatures(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        globalIndices: Iterable<Int>,
        externalSurfaceHeight: ((x: Int, z: Int) -> Int?)? = null,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        val region = LocalModificationRegion(chunk, externalSurfaceHeight, externalBlockState)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in globalIndices) {
            random.setFeatureSeed(decorationSeed, globalIndex, LOCAL_MODIFICATIONS_STEP)
            when (globalIndex) {
                AMETHYST_GEODE_INDEX -> placeAmethystGeode(region, random, seed, environment)
                LARGE_DRIPSTONE_INDEX -> placeLargeDripstones(region, random, environment)
            }
        }
    }

    private fun placeAmethystGeode(
        region: LocalModificationRegion,
        random: WorldgenRandom,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        if (random.nextFloat() >= 1.0f / 24.0f) return
        val x = region.chunk.chunkX * 16 + random.nextInt(16)
        val z = region.chunk.chunkZ * 16 + random.nextInt(16)
        val origin =
            DecorationPos(
                x,
                WorldHeight.MIN_Y + 6 + random.nextInt(30 - (WorldHeight.MIN_Y + 6) + 1),
                z,
            )
        if (environment.biomeAt(origin.x, origin.y, origin.z).substringAfter(':') !in OVERWORLD_BIOMES) return
        placeGeode(region, random, seed, origin)
    }

    /** The vanilla placed feature names this step draws. */
    public val PLACED_IDS: Set<String> = setOf("amethyst_geode", "large_dripstone")

    /** One of them at one position, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        x: Int,
        y: Int,
        z: Int,
        placedFeature: String,
    ): Boolean {
        val region = LocalModificationRegion(chunk, null, null)
        when (placedFeature) {
            "amethyst_geode" -> placeGeode(region, random, chunk.chunkX.toLong(), DecorationPos(x, y, z))
            "large_dripstone" -> placeLargeDripstone(region, random, DecorationPos(x, y, z))
            else -> return false
        }
        return true
    }

    private fun placeGeode(
        region: LocalModificationRegion,
        random: BasaltRandom,
        seed: Long,
        origin: DecorationPos,
    ): Boolean {
        val points = mutableListOf<GeodePoint>()
        val numPoints = 3 + random.nextInt(2)
        val noise = NormalNoise.create(WorldgenRandom(LegacyRandom(seed)), -4, doubleArrayOf(1.0))
        val crackPoints = mutableListOf<DecorationPos>()
        val crackSizeAdjustment = numPoints.toDouble() / 6.0
        val innerAir = 1.0 / sqrt(1.7)
        val innermostBlockLayer = 1.0 / sqrt(2.2 + crackSizeAdjustment)
        val innerCrust = 1.0 / sqrt(3.2 + crackSizeAdjustment)
        val outerCrust = 1.0 / sqrt(4.2 + crackSizeAdjustment)
        val crackSize = 1.0 / sqrt(2.0 + random.nextDouble() / 2.0 + if (numPoints > 3) crackSizeAdjustment else 0.0)
        val shouldGenerateCrack = random.nextFloat() < 0.95f
        var invalidPoints = 0

        repeat(numPoints) {
            val point =
                origin.offset(
                    4 + random.nextInt(3),
                    4 + random.nextInt(3),
                    4 + random.nextInt(3),
                )
            val path = region.blockPath(point)
            if (region.isAir(point) || path in GEODE_INVALID_BLOCKS) {
                invalidPoints++
                if (invalidPoints > 1) return false
            }
            points += GeodePoint(point, 1 + random.nextInt(2))
        }

        if (shouldGenerateCrack) {
            val crackOffset = numPoints * 2 + 1
            when (random.nextInt(4)) {
                0 -> addCrackPoints(crackPoints, origin, crackOffset, 0)
                1 -> addCrackPoints(crackPoints, origin, 0, crackOffset)
                2 -> addCrackPoints(crackPoints, origin, crackOffset, crackOffset)
                else -> addCrackPoints(crackPoints, origin, 0, 0)
            }
        }

        val potentialCrystals = mutableListOf<DecorationPos>()
        for (zOffset in -16..16) {
            for (yOffset in -16..16) {
                for (xOffset in -16..16) {
                    val position = origin.offset(xOffset, yOffset, zOffset)
                    val noiseOffset =
                        noise.getValue(position.x.toDouble(), position.y.toDouble(), position.z.toDouble()) * 0.05
                    var shellDistance = 0.0
                    var crackDistance = 0.0
                    for (point in points) {
                        shellDistance +=
                            inverseSqrt(position.distanceSquared(point.position) + point.offset) + noiseOffset
                    }
                    for (point in crackPoints) {
                        crackDistance += inverseSqrt(position.distanceSquared(point) + 2) + noiseOffset
                    }
                    if (shellDistance < outerCrust) continue

                    when {
                        shouldGenerateCrack && crackDistance >= crackSize && shellDistance < innerAir -> {
                            region.safeSet(position, Blocks.AIR.stateId)
                            scheduleAdjacentFluids(region, position)
                        }
                        shellDistance >= innerAir -> region.safeSet(position, Blocks.AIR.stateId)
                        shellDistance >= innermostBlockLayer -> {
                            val alternate = random.nextFloat() < 0.083f
                            region.safeSet(position, if (alternate) buddingAmethystStateId else amethystBlockStateId)
                            if (alternate && random.nextFloat() < 0.35f) potentialCrystals += position
                        }
                        shellDistance >= innerCrust -> region.safeSet(position, calciteStateId)
                        else -> region.safeSet(position, smoothBasaltStateId)
                    }
                }
            }
        }

        for (crystalPosition in potentialCrystals) {
            val block = AMETHYST_PLACEMENTS[random.nextInt(AMETHYST_PLACEMENTS.size)]
            for (direction in DIRECTIONS) {
                val placePosition = crystalPosition.offset(direction)
                val placeState = region.get(placePosition)
                if (placeState != null && canAmethystClusterGrowAtState(placeState)) {
                    val stateId = amethystPlacementState(block, direction.name, isSourceAmethystWater(placeState))
                    region.safeSet(placePosition, stateId)
                    break
                }
            }
        }
        return true
    }

    private fun addCrackPoints(
        points: MutableList<DecorationPos>,
        origin: DecorationPos,
        xOffset: Int,
        zOffset: Int,
    ) {
        points += origin.offset(xOffset, 7, zOffset)
        points += origin.offset(xOffset, 5, zOffset)
        points += origin.offset(xOffset, 1, zOffset)
    }

    private fun scheduleAdjacentFluids(
        region: LocalModificationRegion,
        position: DecorationPos,
    ) {
        for (direction in DIRECTIONS) {
            val adjacent = position.offset(direction)
            val stateId = region.get(adjacent) ?: continue
            val fluid =
                when {
                    Blocks.hasWaterFluid(stateId) -> Identifier.vanilla("water")
                    stateId in Blocks.LAVA.stateId..Blocks.LAVA.stateId + 15 -> Identifier.vanilla("lava")
                    else -> continue
                }
            region.scheduleFluidTick(adjacent, fluid)
        }
    }

    private fun placeLargeDripstones(
        region: LocalModificationRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
    ) {
        val count = 10 + random.nextInt(39)
        repeat(count) {
            val x = region.chunk.chunkX * 16 + random.nextInt(16)
            val z = region.chunk.chunkZ * 16 + random.nextInt(16)
            val origin =
                DecorationPos(
                    x,
                    WorldHeight.MIN_Y + random.nextInt(256 - WorldHeight.MIN_Y + 1),
                    z,
                )
            if (environment.biomeAt(origin.x, origin.y, origin.z).substringAfter(':') == "dripstone_caves") {
                placeLargeDripstone(region, random, origin)
            }
        }
    }

    private fun placeLargeDripstone(
        region: LocalModificationRegion,
        random: BasaltRandom,
        origin: DecorationPos,
    ): Boolean {
        if (!region.isEmptyOrWater(origin)) return false
        val column = scanColumn(region, origin, 30) ?: return false
        if (column.height < 4) return false

        val radiusLimit = (column.height * 0.33f).toInt().coerceIn(3, 16)
        val radius = 3 + random.nextInt(radiusLimit - 3 + 1)
        val stalactite =
            LargeDripstone(
                origin.copy(y = column.ceiling - 1),
                pointingUp = false,
                radius,
                uniformFloat(random, 0.3f, 0.9f).toDouble(),
                uniformFloat(random, 0.4f, 2.0f).toDouble(),
            )
        val stalagmite =
            LargeDripstone(
                origin.copy(y = column.floor + 1),
                pointingUp = true,
                radius,
                uniformFloat(random, 0.4f, 1.0f).toDouble(),
                uniformFloat(random, 0.4f, 2.0f).toDouble(),
            )
        val wind =
            if (stalactite.isSuitableForWind() && stalagmite.isSuitableForWind()) {
                val speed = uniformFloat(random, 0.0f, 0.3f)
                val direction = uniformFloat(random, 0.0f, PI.toFloat())
                Wind(origin.y, Mth.cos(direction) * speed, Mth.sin(direction) * speed, 16 - radius)
            } else {
                Wind.NONE
            }

        if (stalactite.embedBase(region, wind)) stalactite.place(region, random, wind)
        if (stalagmite.embedBase(region, wind)) stalagmite.place(region, random, wind)
        return true
    }

    private fun scanColumn(
        region: LocalModificationRegion,
        origin: DecorationPos,
        range: Int,
    ): CaveColumn? {
        fun scan(direction: Int): Int? {
            var position = origin
            for (ignored in 1 until range) {
                if (!region.isEmptyOrWater(position)) break
                position = position.copy(y = position.y + direction)
            }
            return if (region.isDripstoneEdge(position)) position.y else null
        }
        val ceiling = scan(1) ?: return null
        val floor = scan(-1) ?: return null
        return CaveColumn(floor, ceiling)
    }

    private data class LargeDripstone(
        var root: DecorationPos,
        val pointingUp: Boolean,
        var radius: Int,
        val bluntness: Double,
        val scale: Double,
    ) {
        private fun height(): Int = heightAtRadius(0.0f)

        private fun heightAtRadius(checkRadius: Float): Int =
            speleothemHeight(checkRadius.toDouble(), radius.toDouble(), scale, bluntness).toInt()

        fun isSuitableForWind(): Boolean = radius >= 4 && bluntness >= 0.6

        fun embedBase(
            region: LocalModificationRegion,
            wind: Wind,
        ): Boolean {
            while (radius > 1) {
                var newRoot = root
                repeat(minOf(10, height())) {
                    if (region.blockPath(newRoot) == "lava") return false
                    if (region.isCircleMostlyEmbedded(wind.offset(newRoot), radius)) {
                        root = newRoot
                        return true
                    }
                    newRoot = newRoot.copy(y = newRoot.y + if (pointingUp) -1 else 1)
                }
                radius /= 2
            }
            return false
        }

        fun place(
            region: LocalModificationRegion,
            random: BasaltRandom,
            wind: Wind,
        ) {
            for (dx in -radius..radius) {
                for (dz in -radius..radius) {
                    val currentRadius = sqrt((dx * dx + dz * dz).toFloat())
                    if (currentRadius > radius) continue
                    var height = heightAtRadius(currentRadius)
                    if (height <= 0) continue
                    if (random.nextFloat() < 0.2f) height = (height * uniformFloat(random, 0.8f, 1.0f)).toInt()

                    var position = root.offset(dx, 0, dz)
                    var hasBeenOutOfStone = false
                    val maxY = if (pointingUp) region.worldSurfaceHeight(position.x, position.z) else Int.MAX_VALUE
                    for (ignored in 0 until height) {
                        if (position.y >= maxY) break
                        val adjusted = wind.offset(position)
                        if (region.isEmptyOrWaterOrLava(adjusted)) {
                            hasBeenOutOfStone = true
                            region.set(adjusted, dripstoneBlockStateId)
                        } else if (hasBeenOutOfStone && region.blockPath(adjusted) in BASE_STONE_OVERWORLD) {
                            break
                        }
                        position = position.copy(y = position.y + if (pointingUp) 1 else -1)
                    }
                }
            }
        }
    }

    private data class Wind(
        val originY: Int,
        val speedX: Float,
        val speedZ: Float,
        val maxOffset: Int,
    ) {
        fun offset(position: DecorationPos): DecorationPos {
            if (this === NONE) return position
            val dy = originY - position.y
            val dx = Mth.floor(speedX.toDouble() * dy).coerceIn(-maxOffset, maxOffset)
            val dz = Mth.floor(speedZ.toDouble() * dy).coerceIn(-maxOffset, maxOffset)
            return position.offset(dx, 0, dz)
        }

        companion object {
            val NONE = Wind(0, 0.0f, 0.0f, 0)
        }
    }

    private data class CaveColumn(
        val floor: Int,
        val ceiling: Int,
    ) {
        val height: Int get() = ceiling - floor - 1
    }

    private data class GeodePoint(
        val position: DecorationPos,
        val offset: Int,
    )

    private fun DecorationPos.offset(
        x: Int,
        y: Int,
        z: Int,
    ): DecorationPos = DecorationPos(this.x + x, this.y + y, this.z + z)

    private fun DecorationPos.offset(direction: Direction): DecorationPos =
        offset(direction.x, direction.y, direction.z)

    private fun DecorationPos.distanceSquared(other: DecorationPos): Double {
        val dx = x - other.x
        val dy = y - other.y
        val dz = z - other.z
        return (dx * dx + dy * dy + dz * dz).toDouble()
    }

    private fun inverseSqrt(value: Double): Double = 1.0 / sqrt(value)

    private fun uniformFloat(
        random: BasaltRandom,
        min: Float,
        max: Float,
    ): Float = min + random.nextFloat() * (max - min)

    private fun speleothemHeight(
        suppliedDistance: Double,
        radius: Double,
        scale: Double,
        bluntness: Double,
    ): Double {
        val distance = maxOf(suppliedDistance, bluntness)
        val ratio = distance / radius * 0.384
        val part1 = 0.75 * ratio.pow(1.3333333333333333)
        val part2 = ratio.pow(0.6666666666666666)
        val part3 = 0.3333333333333333 * ln(ratio)
        return maxOf(scale * (part1 - part2 - part3), 0.0) / 0.384 * radius
    }

    private fun amethystPlacementState(
        block: String,
        facing: String,
        waterlogged: Boolean,
    ): Int =
        Blocks.stateIdOf(
            Identifier.vanilla(block),
            linkedMapOf("facing" to facing, "waterlogged" to waterlogged.toString()),
        ) ?: error("missing state for minecraft:$block facing=$facing waterlogged=$waterlogged")

    internal fun canAmethystClusterGrowAtState(stateId: Int): Boolean =
        Blocks.isAir(stateId) || amethystWaterLevel(stateId) in FULL_WATER_LEVELS

    internal fun isSourceAmethystWater(stateId: Int): Boolean = amethystWaterLevel(stateId) == SOURCE_WATER_LEVEL

    private fun amethystWaterLevel(stateId: Int): Int? {
        val state = Blocks.byStateId(stateId) ?: return null
        if (state.block.path != "water") return null
        return state.properties["level"]?.toIntOrNull()
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private const val LOCAL_MODIFICATIONS_STEP = 2
    private const val AMETHYST_GEODE_INDEX = 2
    private const val LARGE_DRIPSTONE_INDEX = 3
    private val amethystBlockStateId = stateId("amethyst_block")
    private val buddingAmethystStateId = stateId("budding_amethyst")
    private val calciteStateId = stateId("calcite")
    private val smoothBasaltStateId = stateId("smooth_basalt")
    private val dripstoneBlockStateId = stateId("dripstone_block")
    private val AMETHYST_PLACEMENTS =
        listOf("small_amethyst_bud", "medium_amethyst_bud", "large_amethyst_bud", "amethyst_cluster")
    private val GEODE_CANNOT_REPLACE =
        setOf("bedrock", "spawner", "chest", "end_portal_frame", "reinforced_deepslate", "trial_spawner", "vault")
    private val GEODE_INVALID_BLOCKS = setOf("bedrock", "water", "lava", "ice", "packed_ice", "blue_ice")
    private val FULL_WATER_LEVELS = setOf(0, 8)
    private const val SOURCE_WATER_LEVEL = 0
    private val BASE_STONE_OVERWORLD = setOf("stone", "granite", "diorite", "andesite", "tuff", "deepslate")
    private val DIRECTIONS =
        listOf(
            Direction("down", 0, -1, 0),
            Direction("up", 0, 1, 0),
            Direction("north", 0, 0, -1),
            Direction("south", 0, 0, 1),
            Direction("west", -1, 0, 0),
            Direction("east", 1, 0, 0),
        )
    private val OVERWORLD_BIOMES =
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

    private data class Direction(
        val name: String,
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private class LocalModificationRegion(
        val chunk: Chunk,
        private val externalSurfaceHeight: ((x: Int, z: Int) -> Int?)?,
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

        fun safeSet(
            position: DecorationPos,
            stateId: Int,
        ): Boolean {
            val current = get(position) ?: return false
            if (Blocks.byStateId(current)?.block?.path in GEODE_CANNOT_REPLACE) return false
            set(position, stateId)
            return true
        }

        fun blockPath(position: DecorationPos): String? = get(position)?.let(Blocks::byStateId)?.block?.path

        fun isAir(position: DecorationPos): Boolean = get(position)?.let(Blocks::isAir) == true

        fun isEmptyOrWater(position: DecorationPos): Boolean = isAir(position) || blockPath(position) == "water"

        fun isEmptyOrWaterOrLava(position: DecorationPos): Boolean =
            isAir(position) || blockPath(position) == "water" || blockPath(position) == "lava"

        fun isDripstoneEdge(position: DecorationPos): Boolean =
            blockPath(position) == "dripstone_block" ||
                blockPath(position) == "lava" ||
                blockPath(position) in BASE_STONE_OVERWORLD

        fun isCircleMostlyEmbedded(
            center: DecorationPos,
            radius: Int,
        ): Boolean {
            if (isEmptyOrWaterOrLava(center)) return false
            val angleIncrement = 6.0f / radius
            var angle = 0.0f
            while (angle < (PI * 2.0).toFloat()) {
                val dx = (Mth.cos(angle) * radius).toInt()
                val dz = (Mth.sin(angle) * radius).toInt()
                if (isEmptyOrWaterOrLava(center.offset(dx, 0, dz))) return false
                angle += angleIncrement
            }
            return true
        }

        fun worldSurfaceHeight(
            x: Int,
            z: Int,
        ): Int =
            if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
                chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE)
            } else {
                externalSurfaceHeight?.invoke(x, z) ?: WorldHeight.MAX_Y + 1
            }

        fun scheduleFluidTick(
            position: DecorationPos,
            fluid: Identifier,
        ) {
            if (position.x shr 4 != chunk.chunkX || position.z shr 4 != chunk.chunkZ) return
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
            chunk.scheduleFluidTick(
                ChunkScheduledTick(fluid, position.x and 15, position.y, position.z and 15, delay = 0),
            )
        }
    }
}
