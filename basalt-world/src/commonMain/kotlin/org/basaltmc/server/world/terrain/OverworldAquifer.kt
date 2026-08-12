package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockState
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.wire.common.Mth
import kotlin.math.abs

/**
 * // parity: Aquifer.NoiseBasedAquifer for the 26.2 Overworld settings. One instance belongs to
 * one chunk because vanilla's aquifer-center and fluid-status arrays are chunk-local caches.
 */
public class OverworldAquifer(
    private val router: OverworldNoiseRouter,
    seed: Long,
    chunkX: Int,
    chunkZ: Int,
    minY: Int,
    height: Int,
    private val densityCache: DensityCache? = null,
) {
    private data class AquiferPosition(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private data class FluidStatus(
        val fluidLevel: Int,
        val fluidType: BlockState,
    ) {
        fun at(y: Int): BlockState = if (y < fluidLevel) fluidType else Blocks.AIR
    }

    private val positionalRandomFactory =
        XoroshiroRandom(seed)
            .forkPositional()
            .fromHashOf("minecraft:aquifer")
            .forkPositional()
    private val preliminarySurfaceCache = HashMap<Long, Int>()
    private val minGridX = gridX(chunkX * 16 - 5)
    private val maxGridX = gridX(chunkX * 16 + 15 - 5) + 1
    private val gridSizeX = maxGridX - minGridX + 1
    private val minGridY = gridY(minY + 1) - 1
    private val maxGridY = gridY(minY + height + 1) + 1
    private val minGridZ = gridZ(chunkZ * 16 - 5)
    private val maxGridZ = gridZ(chunkZ * 16 + 15 - 5) + 1
    private val gridSizeZ = maxGridZ - minGridZ + 1
    private val locationCache = arrayOfNulls<AquiferPosition>(gridSizeX * (maxGridY - minGridY + 1) * gridSizeZ)
    private val statusCache = arrayOfNulls<FluidStatus>(locationCache.size)
    private val skipSamplingAboveY: Int

    public var shouldScheduleFluidUpdate: Boolean = false
        private set

    init {
        val maxSurface =
            maxPreliminarySurfaceLevel(
                fromGridX(minGridX, 0),
                fromGridZ(minGridZ, 0),
                fromGridX(maxGridX, 9),
                fromGridZ(maxGridZ, 9),
            ) + 8
        val skipSamplingAboveGridY = gridY(maxSurface + 12) + 1
        skipSamplingAboveY = fromGridY(skipSamplingAboveGridY, 11) - 1
    }

    /** Returns null for stone, otherwise the exact air/water/lava material selected by vanilla. */
    public fun computeSubstance(
        x: Int,
        y: Int,
        z: Int,
        density: Double,
    ): BlockState? {
        if (density > 0.0) {
            shouldScheduleFluidUpdate = false
            return null
        }

        val globalFluid = globalFluid(y)
        if (y > skipSamplingAboveY) {
            shouldScheduleFluidUpdate = false
            return globalFluid.at(y)
        }
        if (globalFluid.at(y) == Blocks.LAVA) {
            shouldScheduleFluidUpdate = false
            return Blocks.LAVA
        }

        val xAnchor = gridX(x - 5)
        val yAnchor = gridY(y + 1)
        val zAnchor = gridZ(z - 5)
        var distance1 = Int.MAX_VALUE
        var distance2 = Int.MAX_VALUE
        var distance3 = Int.MAX_VALUE
        var distance4 = Int.MAX_VALUE
        var closest1 = 0
        var closest2 = 0
        var closest3 = 0
        var closest4 = 0

        for (dxCell in 0..1) {
            for (dyCell in -1..1) {
                for (dzCell in 0..1) {
                    val cellX = xAnchor + dxCell
                    val cellY = yAnchor + dyCell
                    val cellZ = zAnchor + dzCell
                    val index = index(cellX, cellY, cellZ)
                    val location =
                        locationCache[index]
                            ?: positionalRandomFactory.at(cellX, cellY, cellZ).let { random ->
                                AquiferPosition(
                                    fromGridX(cellX, random.nextInt(10)),
                                    fromGridY(cellY, random.nextInt(9)),
                                    fromGridZ(cellZ, random.nextInt(10)),
                                ).also { locationCache[index] = it }
                            }
                    val deltaX = location.x - x
                    val deltaY = location.y - y
                    val deltaZ = location.z - z
                    val distance = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ
                    if (distance1 >= distance) {
                        closest4 = closest3
                        closest3 = closest2
                        closest2 = closest1
                        closest1 = index
                        distance4 = distance3
                        distance3 = distance2
                        distance2 = distance1
                        distance1 = distance
                    } else if (distance2 >= distance) {
                        closest4 = closest3
                        closest3 = closest2
                        closest2 = index
                        distance4 = distance3
                        distance3 = distance2
                        distance2 = distance
                    } else if (distance3 >= distance) {
                        closest4 = closest3
                        closest3 = index
                        distance4 = distance3
                        distance3 = distance
                    } else if (distance4 >= distance) {
                        closest4 = index
                        distance4 = distance
                    }
                }
            }
        }

        val status1 = status(closest1)
        val similarity12 = similarity(distance1, distance2)
        val fluidState = status1.at(y)
        if (similarity12 <= 0.0) {
            shouldScheduleFluidUpdate =
                if (similarity12 >= FLOWING_UPDATE_SIMILARITY) status1 != status(closest2) else false
            return fluidState
        }
        if (fluidState == Blocks.WATER && globalFluid(y - 1).at(y - 1) == Blocks.LAVA) {
            shouldScheduleFluidUpdate = true
            return fluidState
        }

        val context = FunctionContext(x, y, z, densityCache)
        val barrierNoiseValue = doubleArrayOf(Double.NaN)
        val status2 = status(closest2)
        val barrier12 = similarity12 * calculatePressure(context, barrierNoiseValue, status1, status2)
        if (density + barrier12 > 0.0) {
            shouldScheduleFluidUpdate = false
            return null
        }

        val status3 = status(closest3)
        val similarity13 = similarity(distance1, distance3)
        if (similarity13 > 0.0) {
            val barrier13 =
                similarity12 * similarity13 * calculatePressure(context, barrierNoiseValue, status1, status3)
            if (density + barrier13 > 0.0) {
                shouldScheduleFluidUpdate = false
                return null
            }
        }

        val similarity23 = similarity(distance2, distance3)
        if (similarity23 > 0.0) {
            val barrier23 =
                similarity12 * similarity23 * calculatePressure(context, barrierNoiseValue, status2, status3)
            if (density + barrier23 > 0.0) {
                shouldScheduleFluidUpdate = false
                return null
            }
        }

        val mayFlow12 = status1 != status2
        val mayFlow23 = similarity23 >= FLOWING_UPDATE_SIMILARITY && status2 != status3
        val mayFlow13 = similarity13 >= FLOWING_UPDATE_SIMILARITY && status1 != status3
        shouldScheduleFluidUpdate =
            if (!mayFlow12 && !mayFlow23 && !mayFlow13) {
                similarity13 >= FLOWING_UPDATE_SIMILARITY &&
                    similarity(distance1, distance4) >= FLOWING_UPDATE_SIMILARITY &&
                    status1 != status(closest4)
            } else {
                true
            }
        return fluidState
    }

    private fun calculatePressure(
        context: FunctionContext,
        barrierNoiseValue: DoubleArray,
        first: FluidStatus,
        second: FluidStatus,
    ): Double {
        val y = context.blockY
        val firstType = first.at(y)
        val secondType = second.at(y)
        if ((firstType == Blocks.LAVA && secondType == Blocks.WATER) ||
            (firstType == Blocks.WATER && secondType == Blocks.LAVA)
        ) {
            return 2.0
        }

        val fluidYDifference = abs(first.fluidLevel - second.fluidLevel)
        if (fluidYDifference == 0) return 0.0
        val averageFluidY = 0.5 * (first.fluidLevel + second.fluidLevel)
        val distanceAboveAverage = y + 0.5 - averageFluidY
        val distanceFromEdge = fluidYDifference / 2.0 - abs(distanceAboveAverage)
        val gradient =
            if (distanceAboveAverage > 0.0) {
                if (distanceFromEdge > 0.0) distanceFromEdge / 1.5 else distanceFromEdge / 2.5
            } else {
                val centerPoint = 3.0 + distanceFromEdge
                if (centerPoint > 0.0) centerPoint / 3.0 else centerPoint / 10.0
            }
        val noiseValue =
            if (gradient < -2.0 || gradient > 2.0) {
                0.0
            } else if (barrierNoiseValue[0].isNaN()) {
                router.barrierNoise.compute(context).also { barrierNoiseValue[0] = it }
            } else {
                barrierNoiseValue[0]
            }
        return 2.0 * (noiseValue + gradient)
    }

    private fun status(index: Int): FluidStatus =
        statusCache[index]
            ?: checkNotNull(locationCache[index]).let { location ->
                computeFluid(location.x, location.y, location.z).also { statusCache[index] = it }
            }

    private fun computeFluid(
        x: Int,
        y: Int,
        z: Int,
    ): FluidStatus {
        val globalFluid = globalFluid(y)
        var lowestPreliminarySurface = Int.MAX_VALUE
        val topOfAquiferCell = y + 12
        val bottomOfAquiferCell = y - 12
        var surfaceAtCenterIsUnderGlobalFluid = false

        for (offset in SURFACE_SAMPLING_OFFSETS_IN_CHUNKS) {
            val sampleX = x + offset[0] * 16
            val sampleZ = z + offset[1] * 16
            val preliminarySurface = preliminarySurfaceLevel(sampleX, sampleZ)
            val adjustedSurface = preliminarySurface + 8
            val center = offset[0] == 0 && offset[1] == 0
            if (center && bottomOfAquiferCell > adjustedSurface) return globalFluid

            val pokesAboveSurface = topOfAquiferCell > adjustedSurface
            if (pokesAboveSurface || center) {
                val globalFluidAtSurface = globalFluid(adjustedSurface)
                if (globalFluidAtSurface.at(adjustedSurface) != Blocks.AIR) {
                    if (center) surfaceAtCenterIsUnderGlobalFluid = true
                    if (pokesAboveSurface) return globalFluidAtSurface
                }
            }
            lowestPreliminarySurface = minOf(lowestPreliminarySurface, preliminarySurface)
        }

        val fluidSurface =
            computeSurfaceLevel(x, y, z, globalFluid, lowestPreliminarySurface, surfaceAtCenterIsUnderGlobalFluid)
        return FluidStatus(fluidSurface, computeFluidType(x, y, z, globalFluid, fluidSurface))
    }

    private fun computeSurfaceLevel(
        x: Int,
        y: Int,
        z: Int,
        globalFluid: FluidStatus,
        lowestPreliminarySurface: Int,
        surfaceAtCenterIsUnderGlobalFluid: Boolean,
    ): Int {
        val context = FunctionContext(x, y, z, densityCache)
        val partiallyFloodedness: Double
        val fullyFloodedness: Double
        if (router.erosion.compute(context) < -0.225f && router.depth.compute(context) > 0.9f) {
            partiallyFloodedness = -1.0
            fullyFloodedness = -1.0
        } else {
            val distanceBelowSurface = lowestPreliminarySurface + 8 - y
            val floodednessFactor =
                if (surfaceAtCenterIsUnderGlobalFluid) {
                    Mth.clampedMap(distanceBelowSurface.toDouble(), 0.0, 64.0, 1.0, 0.0)
                } else {
                    0.0
                }
            val floodednessNoise = Mth.clamp(router.fluidLevelFloodednessNoise.compute(context), -1.0, 1.0)
            val fullyFloodedThreshold = Mth.map(floodednessFactor, 1.0, 0.0, -0.3, 0.8)
            val partiallyFloodedThreshold = Mth.map(floodednessFactor, 1.0, 0.0, -0.8, 0.4)
            partiallyFloodedness = floodednessNoise - partiallyFloodedThreshold
            fullyFloodedness = floodednessNoise - fullyFloodedThreshold
        }
        return when {
            fullyFloodedness > 0.0 -> globalFluid.fluidLevel
            partiallyFloodedness > 0.0 -> randomizedFluidSurfaceLevel(x, y, z, lowestPreliminarySurface)
            else -> WAY_BELOW_MIN_Y
        }
    }

    private fun randomizedFluidSurfaceLevel(
        x: Int,
        y: Int,
        z: Int,
        lowestPreliminarySurface: Int,
    ): Int {
        val cellX = x.floorDiv(16)
        val cellY = y.floorDiv(40)
        val cellZ = z.floorDiv(16)
        val cellMiddleY = cellY * 40 + 20
        val spread = router.fluidLevelSpreadNoise.compute(FunctionContext(cellX, cellY, cellZ, densityCache)) * 10.0
        val quantizedSpread = Mth.floor(spread / 3.0) * 3
        return minOf(lowestPreliminarySurface, cellMiddleY + quantizedSpread)
    }

    private fun computeFluidType(
        x: Int,
        y: Int,
        z: Int,
        globalFluid: FluidStatus,
        fluidSurface: Int,
    ): BlockState {
        if (fluidSurface <= -10 && fluidSurface != WAY_BELOW_MIN_Y && globalFluid.fluidType != Blocks.LAVA) {
            val cellX = x.floorDiv(64)
            val cellY = y.floorDiv(40)
            val cellZ = z.floorDiv(64)
            val lavaNoise = router.lavaNoise.compute(FunctionContext(cellX, cellY, cellZ, densityCache))
            if (abs(lavaNoise) > 0.3) return Blocks.LAVA
        }
        return globalFluid.fluidType
    }

    private fun preliminarySurfaceLevel(
        x: Int,
        z: Int,
    ): Int {
        val quantizedX = (x shr 2) shl 2
        val quantizedZ = (z shr 2) shl 2
        val key = (quantizedX.toLong() and 0xFFFFFFFFL) or (quantizedZ.toLong() shl 32)
        return preliminarySurfaceCache.getOrPut(key) {
            Mth.floor(router.preliminarySurfaceLevel.compute(FunctionContext(quantizedX, 0, quantizedZ, densityCache)))
        }
    }

    private fun maxPreliminarySurfaceLevel(
        minX: Int,
        minZ: Int,
        maxX: Int,
        maxZ: Int,
    ): Int {
        var maximum = Int.MIN_VALUE
        var z = minZ
        while (z <= maxZ) {
            var x = minX
            while (x <= maxX) {
                maximum = maxOf(maximum, preliminarySurfaceLevel(x, z))
                x += 4
            }
            z += 4
        }
        return maximum
    }

    private fun index(
        gridX: Int,
        gridY: Int,
        gridZ: Int,
    ): Int = ((gridY - minGridY) * gridSizeZ + (gridZ - minGridZ)) * gridSizeX + (gridX - minGridX)

    private fun globalFluid(y: Int): FluidStatus = if (y < -54) LAVA_STATUS else SEA_STATUS

    private companion object {
        // parity: DimensionType.MIN_Y << 4, where the packed 12-bit Y range starts at -2032.
        const val WAY_BELOW_MIN_Y = -32_512
        const val FLOWING_UPDATE_SIMILARITY = -0.76

        val SEA_STATUS = FluidStatus(63, Blocks.WATER)
        val LAVA_STATUS = FluidStatus(-54, Blocks.LAVA)
        val SURFACE_SAMPLING_OFFSETS_IN_CHUNKS =
            arrayOf(
                intArrayOf(0, 0),
                intArrayOf(-2, -1),
                intArrayOf(-1, -1),
                intArrayOf(0, -1),
                intArrayOf(1, -1),
                intArrayOf(-3, 0),
                intArrayOf(-2, 0),
                intArrayOf(-1, 0),
                intArrayOf(1, 0),
                intArrayOf(-2, 1),
                intArrayOf(-1, 1),
                intArrayOf(0, 1),
                intArrayOf(1, 1),
            )

        fun gridX(block: Int): Int = block shr 4

        fun gridY(block: Int): Int = block.floorDiv(12)

        fun gridZ(block: Int): Int = block shr 4

        fun fromGridX(
            grid: Int,
            offset: Int,
        ): Int = (grid shl 4) + offset

        fun fromGridY(
            grid: Int,
            offset: Int,
        ): Int = grid * 12 + offset

        fun fromGridZ(
            grid: Int,
            offset: Int,
        ): Int = (grid shl 4) + offset

        fun similarity(
            firstDistance: Int,
            secondDistance: Int,
        ): Double = 1.0 - (secondDistance - firstDistance) / 25.0
    }
}
