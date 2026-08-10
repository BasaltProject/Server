package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import kotlin.math.PI
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin

/** // parity: Minecraft 26.2 `IcebergFeature`, including its random draws and smoothing pass. */
internal object IcebergFeature {
    fun place(
        region: ChunkFeatureRegion,
        random: WorldgenRandom,
        suppliedOrigin: DecorationPos,
        mainStateId: Int,
    ) {
        val origin = DecorationPos(suppliedOrigin.x, SEA_LEVEL, suppliedOrigin.z)
        val snowOnTop = random.nextDouble() > 0.7
        val shapeAngle = random.nextDouble() * 2.0 * PI
        val ellipseA = 11 - random.nextInt(5)
        val ellipseC = 3 + random.nextInt(3)
        val ellipse = random.nextDouble() > 0.7
        var aboveWaterHeight = if (ellipse) random.nextInt(6) + 6 else random.nextInt(15) + 3
        if (!ellipse && random.nextDouble() > 0.9) aboveWaterHeight += random.nextInt(19) + 7
        val belowWaterHeight = minOf(aboveWaterHeight + random.nextInt(11), 18)
        val width = minOf(aboveWaterHeight + random.nextInt(7) - random.nextInt(5), 11)
        val a = if (ellipse) ellipseA else 11

        for (xOffset in -a until a) {
            for (zOffset in -a until a) {
                for (yOffset in 0 until aboveWaterHeight) {
                    val radius =
                        if (ellipse) {
                            heightDependentRadiusEllipse(yOffset, aboveWaterHeight, width)
                        } else {
                            heightDependentRadiusRound(random, yOffset, aboveWaterHeight, width)
                        }
                    if (ellipse || xOffset < radius) {
                        generateBlock(
                            region,
                            random,
                            origin,
                            aboveWaterHeight,
                            xOffset,
                            yOffset,
                            zOffset,
                            radius,
                            a,
                            ellipse,
                            ellipseC,
                            shapeAngle,
                            snowOnTop,
                            mainStateId,
                        )
                    }
                }
            }
        }

        smooth(region, origin, width, aboveWaterHeight, ellipse, ellipseA)

        for (xOffset in -a until a) {
            for (zOffset in -a until a) {
                var yOffset = -1
                while (yOffset > -belowWaterHeight) {
                    val newA =
                        if (ellipse) {
                            ceil(a * (1.0f - yOffset.toFloat().pow(2) / (belowWaterHeight * 8.0f))).toInt()
                        } else {
                            a
                        }
                    val radius = heightDependentRadiusSteep(random, -yOffset, belowWaterHeight, width)
                    if (xOffset < radius) {
                        generateBlock(
                            region,
                            random,
                            origin,
                            belowWaterHeight,
                            xOffset,
                            yOffset,
                            zOffset,
                            radius,
                            newA,
                            ellipse,
                            ellipseC,
                            shapeAngle,
                            snowOnTop,
                            mainStateId,
                        )
                    }
                    yOffset--
                }
            }
        }

        val cutOut = if (ellipse) random.nextDouble() > 0.1 else random.nextDouble() > 0.7
        if (cutOut) {
            generateCutOut(
                random,
                region,
                width,
                aboveWaterHeight,
                origin,
                ellipse,
                ellipseA,
                shapeAngle,
                ellipseC,
            )
        }
    }

    private fun generateCutOut(
        random: WorldgenRandom,
        region: ChunkFeatureRegion,
        width: Int,
        height: Int,
        globalOrigin: DecorationPos,
        ellipse: Boolean,
        ellipseA: Int,
        shapeAngle: Double,
        ellipseC: Int,
    ) {
        val signX = if (random.nextBoolean()) -1 else 1
        val signZ = if (random.nextBoolean()) -1 else 1
        var xOffset = random.nextInt(maxOf(width / 2 - 2, 1))
        if (random.nextBoolean()) xOffset = width / 2 + 1 - random.nextInt(maxOf(width - width / 2 - 1, 1))
        var zOffset = random.nextInt(maxOf(width / 2 - 2, 1))
        if (random.nextBoolean()) zOffset = width / 2 + 1 - random.nextInt(maxOf(width - width / 2 - 1, 1))
        if (ellipse) {
            val commonOffset = random.nextInt(maxOf(ellipseA - 5, 1))
            xOffset = commonOffset
            zOffset = commonOffset
        }
        val localOrigin = DecorationPos(signX * xOffset, 0, signZ * zOffset)
        val angle = if (ellipse) shapeAngle + PI / 2 else random.nextDouble() * 2.0 * PI

        for (yOffset in 0 until height - 3) {
            val radius = heightDependentRadiusRound(random, yOffset, height, width)
            carve(radius, yOffset, globalOrigin, region, false, angle, localOrigin, ellipseA, ellipseC)
        }
        var yOffset = -1
        while (yOffset > -height + random.nextInt(5)) {
            val radius = heightDependentRadiusSteep(random, -yOffset, height, width)
            carve(radius, yOffset, globalOrigin, region, true, angle, localOrigin, ellipseA, ellipseC)
            yOffset--
        }
    }

    private fun carve(
        radius: Int,
        yOffset: Int,
        globalOrigin: DecorationPos,
        region: ChunkFeatureRegion,
        underWater: Boolean,
        angle: Double,
        localOrigin: DecorationPos,
        ellipseA: Int,
        ellipseC: Int,
    ) {
        val a = radius + 1 + ellipseA / 3
        val c = minOf(radius - 3, 3) + ellipseC / 2 - 1
        for (xOffset in -a until a) {
            for (zOffset in -a until a) {
                if (signedDistanceEllipse(xOffset, zOffset, localOrigin, a, c, angle) < 0.0) {
                    val pos = globalOrigin.offset(xOffset, yOffset, zOffset)
                    val stateId = region.get(pos)
                    if (isIcebergState(stateId) || stateId == Blocks.SNOW_BLOCK.stateId) {
                        if (underWater) {
                            region.set(pos, Blocks.WATER.stateId)
                        } else {
                            region.set(pos, Blocks.AIR.stateId)
                            if (region.get(pos.up()) == Blocks.SNOW.stateId) region.set(pos.up(), Blocks.AIR.stateId)
                        }
                    }
                }
            }
        }
    }

    private fun generateBlock(
        region: ChunkFeatureRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
        height: Int,
        xOffset: Int,
        yOffset: Int,
        zOffset: Int,
        radius: Int,
        a: Int,
        ellipse: Boolean,
        ellipseC: Int,
        shapeAngle: Double,
        snowOnTop: Boolean,
        mainStateId: Int,
    ) {
        val signedDistance =
            if (ellipse) {
                signedDistanceEllipse(xOffset, zOffset, ZERO, a, getEllipseC(yOffset, height, ellipseC), shapeAngle)
            } else {
                signedDistanceCircle(xOffset, zOffset, ZERO, radius, random)
            }
        if (signedDistance >= 0.0) return
        val pos = origin.offset(xOffset, yOffset, zOffset)
        val compareValue = if (ellipse) -0.5 else (-6 - random.nextInt(3)).toDouble()
        if (signedDistance > compareValue && random.nextDouble() > 0.9) return
        setIcebergBlock(pos, region, random, height - yOffset, height, ellipse, snowOnTop, mainStateId)
    }

    private fun setIcebergBlock(
        pos: DecorationPos,
        region: ChunkFeatureRegion,
        random: WorldgenRandom,
        heightDifference: Int,
        height: Int,
        ellipse: Boolean,
        snowOnTop: Boolean,
        mainStateId: Int,
    ) {
        val stateId = region.get(pos)
        if (!Blocks.isAir(stateId) &&
            stateId != Blocks.SNOW_BLOCK.stateId &&
            stateId != Blocks.ICE.stateId &&
            stateId != Blocks.WATER.stateId
        ) {
            return
        }
        val randomness = !ellipse || random.nextDouble() > 0.05
        val divisor = if (ellipse) 3 else 2
        if (snowOnTop &&
            stateId != Blocks.WATER.stateId &&
            heightDifference.toDouble() <= random.nextInt(maxOf(1, height / divisor)) + height * 0.6 &&
            randomness
        ) {
            region.set(pos, Blocks.SNOW_BLOCK.stateId)
        } else {
            region.set(pos, mainStateId)
        }
    }

    private fun smooth(
        region: ChunkFeatureRegion,
        origin: DecorationPos,
        width: Int,
        height: Int,
        ellipse: Boolean,
        ellipseA: Int,
    ) {
        val a = if (ellipse) ellipseA else width / 2
        for (xOffset in -a..a) {
            for (zOffset in -a..a) {
                for (yOffset in 0..height) {
                    val pos = origin.offset(xOffset, yOffset, zOffset)
                    val stateId = region.get(pos)
                    if (isIcebergState(stateId) || stateId == Blocks.SNOW.stateId) {
                        if (Blocks.isAir(region.get(pos.down()))) {
                            region.set(pos, Blocks.AIR.stateId)
                            region.set(pos.up(), Blocks.AIR.stateId)
                        } else if (isIcebergState(stateId)) {
                            val nonIcebergSides =
                                HORIZONTAL_DIRECTIONS.count {
                                    !isIcebergState(region.get(pos.offset(it)))
                                }
                            if (nonIcebergSides >= 3) region.set(pos, Blocks.AIR.stateId)
                        }
                    }
                }
            }
        }
    }

    private fun signedDistanceCircle(
        xOffset: Int,
        zOffset: Int,
        origin: DecorationPos,
        radius: Int,
        random: WorldgenRandom,
    ): Double {
        val offset = 10.0f * random.nextFloat().coerceIn(0.2f, 0.8f) / radius
        return offset +
            (xOffset - origin.x).toDouble().pow(2) +
            (zOffset - origin.z).toDouble().pow(2) -
            radius.toDouble().pow(2)
    }

    private fun signedDistanceEllipse(
        xOffset: Int,
        zOffset: Int,
        origin: DecorationPos,
        a: Int,
        c: Int,
        angle: Double,
    ): Double =
        (((xOffset - origin.x) * cos(angle) - (zOffset - origin.z) * sin(angle)) / a).pow(2) +
            (((xOffset - origin.x) * sin(angle) + (zOffset - origin.z) * cos(angle)) / c).pow(2) -
            1.0

    private fun heightDependentRadiusRound(
        random: WorldgenRandom,
        yOffset: Int,
        height: Int,
        width: Int,
    ): Int {
        val k = 3.5f - random.nextFloat()
        var scale = (1.0f - yOffset.toFloat().pow(2) / (height * k)) * width
        if (height > 15 + random.nextInt(5)) {
            val adjustedY = if (yOffset < 3 + random.nextInt(6)) yOffset / 2 else yOffset
            scale = (1.0f - adjustedY / (height * k * 0.4f)) * width
        }
        return ceil(scale / 2.0f).toInt()
    }

    private fun heightDependentRadiusEllipse(
        yOffset: Int,
        height: Int,
        width: Int,
    ): Int {
        val scale = (1.0f - yOffset.toFloat().pow(2) / height) * width
        return ceil(scale / 2.0f).toInt()
    }

    private fun heightDependentRadiusSteep(
        random: WorldgenRandom,
        yOffset: Int,
        height: Int,
        width: Int,
    ): Int {
        val k = 1.0f + random.nextFloat() / 2.0f
        val scale = (1.0f - yOffset / (height * k)) * width
        return ceil(scale / 2.0f).toInt()
    }

    private fun getEllipseC(
        yOffset: Int,
        height: Int,
        ellipseC: Int,
    ): Int = if (yOffset > 0 && height - yOffset <= 3) ellipseC - (4 - (height - yOffset)) else ellipseC

    private fun isIcebergState(stateId: Int): Boolean =
        stateId == Blocks.PACKED_ICE.stateId ||
            stateId == Blocks.SNOW_BLOCK.stateId ||
            stateId == Blocks.BLUE_ICE.stateId

    private val ZERO = DecorationPos(0, 0, 0)
    private val HORIZONTAL_DIRECTIONS = ALL_DIRECTIONS.drop(2)
    private const val SEA_LEVEL = 63
}
