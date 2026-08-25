package org.basaltmc.server.world.terrain.carver

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.OverworldAquifer
import org.basaltmc.server.world.terrain.surface.RuleSource
import org.basaltmc.server.world.terrain.surface.SurfaceBuilder
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Mth
import kotlin.math.abs

/**
 * // parity: `NoiseBasedChunkGenerator.applyCarvers` plus the builtin `cave`,
 * `cave_extra_underground`, and `canyon` configured carvers used by every 26.2 Overworld biome.
 */
public object OverworldCarvers {
    /** Applies all three carvers from every source chunk in vanilla's 8-chunk neighborhood. */
    public fun carve(
        chunk: Chunk,
        seed: Long,
        aquifer: OverworldAquifer,
        environment: SurfaceRuleEnvironment,
        surfaceRule: RuleSource,
    ) {
        val random = WorldgenRandom(LegacyRandom(0L))
        val mask = BooleanArray(WorldHeight.HEIGHT * 16 * 16)
        for (dx in -SOURCE_RANGE..SOURCE_RANGE) {
            for (dz in -SOURCE_RANGE..SOURCE_RANGE) {
                val sourceChunkX = chunk.chunkX + dx
                val sourceChunkZ = chunk.chunkZ + dz
                CAVE_CONFIGS.forEachIndexed { index, configuration ->
                    random.setLargeFeatureSeed(seed + index, sourceChunkX, sourceChunkZ)
                    if (random.nextFloat() <= configuration.probability) {
                        carveCaveSource(
                            chunk,
                            random,
                            sourceChunkX,
                            sourceChunkZ,
                            configuration,
                            aquifer,
                            environment,
                            surfaceRule,
                            mask,
                        )
                    }
                }
                random.setLargeFeatureSeed(seed + CAVE_CONFIGS.size, sourceChunkX, sourceChunkZ)
                if (random.nextFloat() <= CANYON_PROBABILITY) {
                    carveCanyonSource(
                        chunk,
                        random,
                        sourceChunkX,
                        sourceChunkZ,
                        aquifer,
                        environment,
                        surfaceRule,
                        mask,
                    )
                }
            }
        }
    }

    private fun carveCaveSource(
        chunk: Chunk,
        random: BasaltRandom,
        sourceChunkX: Int,
        sourceChunkZ: Int,
        configuration: CaveConfiguration,
        aquifer: OverworldAquifer,
        environment: SurfaceRuleEnvironment,
        surfaceRule: RuleSource,
        mask: BooleanArray,
    ) {
        val caveCount = random.nextInt(random.nextInt(random.nextInt(CAVE_BOUND) + 1) + 1)
        repeat(caveCount) {
            val x = sourceChunkX * 16 + random.nextInt(16).toDouble()
            val y = random.nextInt(configuration.maxY - CAVE_MIN_Y + 1) + CAVE_MIN_Y
            val z = sourceChunkZ * 16 + random.nextInt(16).toDouble()
            val horizontalRadiusMultiplier = uniformFloat(random, 0.7f, 1.4f).toDouble()
            val verticalRadiusMultiplier = uniformFloat(random, 0.8f, 1.3f).toDouble()
            val floorLevel = uniformFloat(random, -1.0f, -0.4f)
            var tunnels = 1
            if (random.nextInt(4) == 0) {
                val yScale = uniformFloat(random, 0.1f, 0.9f).toDouble()
                val thickness = 1.0f + random.nextFloat() * 6.0f
                createCaveRoom(
                    chunk,
                    aquifer,
                    environment,
                    surfaceRule,
                    x,
                    y.toDouble(),
                    z,
                    thickness,
                    yScale,
                    floorLevel,
                    mask,
                )
                tunnels += random.nextInt(4)
            }

            repeat(tunnels) {
                val horizontalRotation = random.nextFloat() * TWO_PI
                val verticalRotation = (random.nextFloat() - 0.5f) / 4.0f
                var thickness = random.nextFloat() * 2.0f + random.nextFloat()
                if (random.nextInt(10) == 0) {
                    thickness *= random.nextFloat() * random.nextFloat() * 3.0f + 1.0f
                }
                val distance = MAX_DISTANCE - random.nextInt(MAX_DISTANCE / 4)
                createCaveTunnel(
                    chunk,
                    random.nextLong(),
                    aquifer,
                    environment,
                    surfaceRule,
                    x,
                    y.toDouble(),
                    z,
                    horizontalRadiusMultiplier,
                    verticalRadiusMultiplier,
                    thickness,
                    horizontalRotation,
                    verticalRotation,
                    0,
                    distance,
                    1.0,
                    floorLevel,
                    mask,
                )
            }
        }
    }

    private fun createCaveRoom(
        chunk: Chunk,
        aquifer: OverworldAquifer,
        environment: SurfaceRuleEnvironment,
        surfaceRule: RuleSource,
        x: Double,
        y: Double,
        z: Double,
        thickness: Float,
        yScale: Double,
        floorLevel: Float,
        mask: BooleanArray,
    ) {
        val horizontalRadius = 1.5 + Mth.sin(HALF_PI) * thickness
        carveEllipsoid(
            chunk,
            aquifer,
            environment,
            surfaceRule,
            x + 1.0,
            y,
            z,
            horizontalRadius,
            horizontalRadius * yScale,
            mask,
        ) { xd, yd, zd, _ -> yd <= floorLevel || xd * xd + yd * yd + zd * zd >= 1.0 }
    }

    @Suppress("LongParameterList")
    private fun createCaveTunnel(
        chunk: Chunk,
        tunnelSeed: Long,
        aquifer: OverworldAquifer,
        environment: SurfaceRuleEnvironment,
        surfaceRule: RuleSource,
        xStart: Double,
        yStart: Double,
        zStart: Double,
        horizontalRadiusMultiplier: Double,
        verticalRadiusMultiplier: Double,
        thickness: Float,
        horizontalRotationStart: Float,
        verticalRotationStart: Float,
        step: Int,
        distance: Int,
        yScale: Double,
        floorLevel: Float,
        mask: BooleanArray,
    ) {
        val random = LegacyRandom(tunnelSeed)
        val splitPoint = random.nextInt(distance / 2) + distance / 4
        val steep = random.nextInt(6) == 0
        var x = xStart
        var y = yStart
        var z = zStart
        var horizontalRotation = horizontalRotationStart
        var verticalRotation = verticalRotationStart
        var verticalRotationVelocity = 0.0f
        var horizontalRotationVelocity = 0.0f

        for (currentStep in step until distance) {
            val horizontalRadius =
                1.5 + Mth.sin(PI_FLOAT * currentStep.toFloat() / distance.toFloat()) * thickness
            val verticalRadius = horizontalRadius * yScale
            val cosVertical = Mth.cos(verticalRotation)
            x += (Mth.cos(horizontalRotation) * cosVertical).toDouble()
            y += Mth.sin(verticalRotation).toDouble()
            z += (Mth.sin(horizontalRotation) * cosVertical).toDouble()
            verticalRotation *= if (steep) 0.92f else 0.7f
            verticalRotation += verticalRotationVelocity * 0.1f
            horizontalRotation += horizontalRotationVelocity * 0.1f
            verticalRotationVelocity *= 0.9f
            horizontalRotationVelocity *= 0.75f
            verticalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0f
            horizontalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0f

            if (currentStep == splitPoint && thickness > 1.0f) {
                createCaveTunnel(
                    chunk,
                    random.nextLong(),
                    aquifer,
                    environment,
                    surfaceRule,
                    x,
                    y,
                    z,
                    horizontalRadiusMultiplier,
                    verticalRadiusMultiplier,
                    random.nextFloat() * 0.5f + 0.5f,
                    horizontalRotation - HALF_PI,
                    verticalRotation / 3.0f,
                    currentStep,
                    distance,
                    1.0,
                    floorLevel,
                    mask,
                )
                createCaveTunnel(
                    chunk,
                    random.nextLong(),
                    aquifer,
                    environment,
                    surfaceRule,
                    x,
                    y,
                    z,
                    horizontalRadiusMultiplier,
                    verticalRadiusMultiplier,
                    random.nextFloat() * 0.5f + 0.5f,
                    horizontalRotation + HALF_PI,
                    verticalRotation / 3.0f,
                    currentStep,
                    distance,
                    1.0,
                    floorLevel,
                    mask,
                )
                return
            }

            if (random.nextInt(4) != 0) {
                if (!canReach(chunk, x, z, currentStep, distance, thickness)) return
                carveEllipsoid(
                    chunk,
                    aquifer,
                    environment,
                    surfaceRule,
                    x,
                    y,
                    z,
                    horizontalRadius * horizontalRadiusMultiplier,
                    verticalRadius * verticalRadiusMultiplier,
                    mask,
                ) { xd, yd, zd, _ -> yd <= floorLevel || xd * xd + yd * yd + zd * zd >= 1.0 }
            }
        }
    }

    private fun carveCanyonSource(
        chunk: Chunk,
        random: BasaltRandom,
        sourceChunkX: Int,
        sourceChunkZ: Int,
        aquifer: OverworldAquifer,
        environment: SurfaceRuleEnvironment,
        surfaceRule: RuleSource,
        mask: BooleanArray,
    ) {
        val x = sourceChunkX * 16 + random.nextInt(16).toDouble()
        val y = random.nextInt(CANYON_MAX_Y - CANYON_MIN_Y + 1) + CANYON_MIN_Y
        val z = sourceChunkZ * 16 + random.nextInt(16).toDouble()
        val horizontalRotation = random.nextFloat() * TWO_PI
        val verticalRotation = uniformFloat(random, -0.125f, 0.125f)
        val thickness = trapezoidFloat(random, 0.0f, 6.0f, 2.0f)
        val distance = (MAX_DISTANCE * uniformFloat(random, 0.75f, 1.0f)).toInt()
        createCanyonTunnel(
            chunk,
            random.nextLong(),
            aquifer,
            environment,
            surfaceRule,
            x,
            y.toDouble(),
            z,
            thickness,
            horizontalRotation,
            verticalRotation,
            distance,
            mask,
        )
    }

    @Suppress("LongParameterList")
    private fun createCanyonTunnel(
        chunk: Chunk,
        tunnelSeed: Long,
        aquifer: OverworldAquifer,
        environment: SurfaceRuleEnvironment,
        surfaceRule: RuleSource,
        xStart: Double,
        yStart: Double,
        zStart: Double,
        thickness: Float,
        horizontalRotationStart: Float,
        verticalRotationStart: Float,
        distance: Int,
        mask: BooleanArray,
    ) {
        val random = LegacyRandom(tunnelSeed)
        val widthFactors = initCanyonWidthFactors(random)
        var x = xStart
        var y = yStart
        var z = zStart
        var horizontalRotation = horizontalRotationStart
        var verticalRotation = verticalRotationStart
        var verticalRotationVelocity = 0.0f
        var horizontalRotationVelocity = 0.0f

        for (currentStep in 0 until distance) {
            var horizontalRadius =
                1.5 + Mth.sin(currentStep.toFloat() * PI_FLOAT / distance.toFloat()) * thickness
            var verticalRadius = horizontalRadius * CANYON_Y_SCALE
            horizontalRadius *= uniformFloat(random, 0.75f, 1.0f)
            val verticalMultiplier = 1.0f - abs(0.5f - currentStep.toFloat() / distance.toFloat()) * 2.0f
            val verticalFactor = 1.0f + 0.0f * verticalMultiplier
            verticalRadius *= verticalFactor * uniformFloat(random, 0.75f, 1.0f)
            val cosVertical = Mth.cos(verticalRotation)
            x += (Mth.cos(horizontalRotation) * cosVertical).toDouble()
            y += Mth.sin(verticalRotation).toDouble()
            z += (Mth.sin(horizontalRotation) * cosVertical).toDouble()
            verticalRotation *= 0.7f
            verticalRotation += verticalRotationVelocity * 0.05f
            horizontalRotation += horizontalRotationVelocity * 0.05f
            verticalRotationVelocity *= 0.8f
            horizontalRotationVelocity *= 0.5f
            verticalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0f
            horizontalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0f

            if (random.nextInt(4) != 0) {
                if (!canReach(chunk, x, z, currentStep, distance, thickness)) return
                carveEllipsoid(
                    chunk,
                    aquifer,
                    environment,
                    surfaceRule,
                    x,
                    y,
                    z,
                    horizontalRadius,
                    verticalRadius,
                    mask,
                ) { xd, yd, zd, worldY ->
                    val widthFactor = widthFactors[worldY - WorldHeight.MIN_Y - 1]
                    (xd * xd + zd * zd) * widthFactor + yd * yd / 6.0 >= 1.0
                }
            }
        }
    }

    private fun initCanyonWidthFactors(random: BasaltRandom): FloatArray {
        val factors = FloatArray(WorldHeight.HEIGHT)
        var factor = 1.0f
        for (index in factors.indices) {
            if (index == 0 || random.nextInt(CANYON_WIDTH_SMOOTHNESS) == 0) {
                factor = 1.0f + random.nextFloat() * random.nextFloat()
            }
            factors[index] = factor * factor
        }
        return factors
    }

    @Suppress("LongParameterList")
    private fun carveEllipsoid(
        chunk: Chunk,
        aquifer: OverworldAquifer,
        environment: SurfaceRuleEnvironment,
        surfaceRule: RuleSource,
        x: Double,
        y: Double,
        z: Double,
        horizontalRadius: Double,
        verticalRadius: Double,
        mask: BooleanArray,
        shouldSkip: (xd: Double, yd: Double, zd: Double, y: Int) -> Boolean,
    ) {
        val centerX = chunk.chunkX * 16 + 8.0
        val centerZ = chunk.chunkZ * 16 + 8.0
        val maxDelta = 16.0 + horizontalRadius * 2.0
        if (abs(x - centerX) > maxDelta || abs(z - centerZ) > maxDelta) return

        val chunkMinX = chunk.chunkX * 16
        val chunkMinZ = chunk.chunkZ * 16
        val minLocalX = maxOf(Mth.floor(x - horizontalRadius) - chunkMinX - 1, 0)
        val maxLocalX = minOf(Mth.floor(x + horizontalRadius) - chunkMinX, 15)
        val minY = maxOf(Mth.floor(y - verticalRadius) - 1, WorldHeight.MIN_Y + 1)
        val maxY =
            minOf(
                Mth.floor(y + verticalRadius) + 1,
                WorldHeight.MIN_Y + WorldHeight.HEIGHT - 1 - PROTECTED_BLOCKS_ON_TOP,
            )
        val minLocalZ = maxOf(Mth.floor(z - horizontalRadius) - chunkMinZ - 1, 0)
        val maxLocalZ = minOf(Mth.floor(z + horizontalRadius) - chunkMinZ, 15)

        for (localX in minLocalX..maxLocalX) {
            val worldX = chunkMinX + localX
            val xd = (worldX + 0.5 - x) / horizontalRadius
            for (localZ in minLocalZ..maxLocalZ) {
                val worldZ = chunkMinZ + localZ
                val zd = (worldZ + 0.5 - z) / horizontalRadius
                if (xd * xd + zd * zd >= 1.0) continue
                var hasGrass = false
                for (worldY in maxY downTo minY + 1) {
                    val yd = (worldY - 0.5 - y) / verticalRadius
                    if (shouldSkip(xd, yd, zd, worldY)) continue
                    val maskIndex = ((worldY - WorldHeight.MIN_Y) * 16 + localZ) * 16 + localX
                    if (mask[maskIndex]) continue
                    mask[maskIndex] = true
                    val oldStateId = chunk.getBlockStateId(localX, worldY, localZ)
                    val oldPath = Blocks.byStateId(oldStateId)?.block?.path
                    if (oldPath == "grass_block" || oldPath == "mycelium") hasGrass = true
                    if (oldPath !in REPLACEABLE_BLOCKS) continue

                    val stateId =
                        if (worldY <= LAVA_LEVEL) {
                            Blocks.LAVA.stateId
                        } else {
                            aquifer.computeSubstance(worldX, worldY, worldZ, 0.0)?.stateId ?: continue
                        }
                    chunk.setBlockStateId(localX, worldY, localZ, stateId)
                    if (hasGrass && worldY - 1 >= WorldHeight.MIN_Y) {
                        val belowStateId = chunk.getBlockStateId(localX, worldY - 1, localZ)
                        if (Blocks.byStateId(belowStateId)?.block?.path == "dirt") {
                            val topMaterial =
                                SurfaceBuilder.topMaterial(
                                    environment,
                                    surfaceRule,
                                    worldX,
                                    worldY - 1,
                                    worldZ,
                                    Blocks.hasFluid(stateId),
                                )
                            if (topMaterial != null) {
                                chunk.setBlockStateId(localX, worldY - 1, localZ, topMaterial)
                            }
                        }
                    }
                }
            }
        }
    }

    private fun canReach(
        chunk: Chunk,
        x: Double,
        z: Double,
        currentStep: Int,
        totalSteps: Int,
        thickness: Float,
    ): Boolean {
        val xd = x - (chunk.chunkX * 16 + 8.0)
        val zd = z - (chunk.chunkZ * 16 + 8.0)
        val remaining = (totalSteps - currentStep).toDouble()
        val radius = (thickness + 2.0f + 16.0f).toDouble()
        return xd * xd + zd * zd - remaining * remaining <= radius * radius
    }

    private fun uniformFloat(
        random: BasaltRandom,
        min: Float,
        maxExclusive: Float,
    ): Float = random.nextFloat() * (maxExclusive - min) + min

    private fun trapezoidFloat(
        random: BasaltRandom,
        min: Float,
        max: Float,
        plateau: Float,
    ): Float {
        val range = max - min
        val plateauStart = (range - plateau) / 2.0f
        val plateauEnd = range - plateauStart
        return min + random.nextFloat() * plateauEnd + random.nextFloat() * plateauStart
    }

    private data class CaveConfiguration(
        val probability: Float,
        val maxY: Int,
    )

    private const val SOURCE_RANGE = 8
    private const val CAVE_BOUND = 15
    private const val CAVE_MIN_Y = -56
    private const val CANYON_MIN_Y = 10
    private const val CANYON_MAX_Y = 67
    private const val CANYON_PROBABILITY = 0.01f
    private const val CANYON_Y_SCALE = 3.0
    private const val CANYON_WIDTH_SMOOTHNESS = 3
    private const val MAX_DISTANCE = 112
    private const val LAVA_LEVEL = -56
    private const val PROTECTED_BLOCKS_ON_TOP = 7
    private val PI_FLOAT: Float = kotlin.math.PI.toFloat()
    private val HALF_PI: Float = (kotlin.math.PI / 2.0).toFloat()
    private val TWO_PI: Float = (kotlin.math.PI * 2.0).toFloat()
    private val CAVE_CONFIGS = listOf(CaveConfiguration(0.15f, 180), CaveConfiguration(0.07f, 47))

    private val TERRACOTTA_COLORS =
        listOf(
            "",
            "white",
            "orange",
            "magenta",
            "light_blue",
            "yellow",
            "lime",
            "pink",
            "gray",
            "light_gray",
            "cyan",
            "purple",
            "blue",
            "brown",
            "green",
            "red",
            "black",
        )

    private val REPLACEABLE_BLOCKS: Set<String> =
        buildSet {
            addAll(
                listOf(
                    "stone",
                    "granite",
                    "diorite",
                    "andesite",
                    "tuff",
                    "deepslate",
                    "dirt",
                    "coarse_dirt",
                    "rooted_dirt",
                    "mud",
                    "muddy_mangrove_roots",
                    "moss_block",
                    "pale_moss_block",
                    "grass_block",
                    "podzol",
                    "mycelium",
                    "sand",
                    "red_sand",
                    "suspicious_sand",
                    "iron_ore",
                    "deepslate_iron_ore",
                    "copper_ore",
                    "deepslate_copper_ore",
                    "snow",
                    "snow_block",
                    "powder_snow",
                    "water",
                    "gravel",
                    "suspicious_gravel",
                    "sandstone",
                    "red_sandstone",
                    "calcite",
                    "packed_ice",
                    "raw_iron_block",
                    "raw_copper_block",
                    "cinnabar",
                    "sulfur",
                    "potent_sulfur",
                ),
            )
            TERRACOTTA_COLORS.forEach { color -> add(if (color.isEmpty()) "terracotta" else "${color}_terracotta") }
        }
}
