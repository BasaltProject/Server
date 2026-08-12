package org.basaltmc.server.world.terrain.carver

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Mth
import kotlin.math.abs

/** // parity: `NetherWorldCarver` with the builtin `nether_cave` configuration. */
public object NetherCaveCarver {
    /** Applies every source chunk in `NoiseBasedChunkGenerator.applyCarvers`'s 8-chunk radius. */
    public fun carve(
        chunk: Chunk,
        seed: Long,
    ) {
        val random = WorldgenRandom(LegacyRandom(0L))
        val mask = BooleanArray(GENERATION_HEIGHT * 16 * 16)
        for (dx in -CARVER_RANGE..CARVER_RANGE) {
            for (dz in -CARVER_RANGE..CARVER_RANGE) {
                val sourceChunkX = chunk.chunkX + dx
                val sourceChunkZ = chunk.chunkZ + dz
                random.setLargeFeatureSeed(seed, sourceChunkX, sourceChunkZ)
                if (random.nextFloat() <= START_PROBABILITY) {
                    carveSource(chunk, random, sourceChunkX, sourceChunkZ, mask)
                }
            }
        }
    }

    private fun carveSource(
        chunk: Chunk,
        random: BasaltRandom,
        sourceChunkX: Int,
        sourceChunkZ: Int,
        mask: BooleanArray,
    ) {
        val caveCount = random.nextInt(random.nextInt(random.nextInt(CAVE_BOUND) + 1) + 1)
        repeat(caveCount) {
            val x = sourceChunkX * 16 + random.nextInt(16).toDouble()
            val y = random.nextInt(MAX_START_Y + 1).toDouble()
            val z = sourceChunkZ * 16 + random.nextInt(16).toDouble()
            var tunnels = 1
            if (random.nextInt(4) == 0) {
                val thickness = 1.0f + random.nextFloat() * 6.0f
                createRoom(chunk, x, y, z, thickness, mask)
                tunnels += random.nextInt(4)
            }

            repeat(tunnels) {
                val horizontalRotation = random.nextFloat() * TWO_PI
                val verticalRotation = (random.nextFloat() - 0.5f) / 4.0f
                val thickness = (random.nextFloat() * 2.0f + random.nextFloat()) * 2.0f
                val distance = MAX_DISTANCE - random.nextInt(MAX_DISTANCE / 4)
                createTunnel(
                    chunk = chunk,
                    tunnelSeed = random.nextLong(),
                    xStart = x,
                    yStart = y,
                    zStart = z,
                    thickness = thickness,
                    horizontalRotationStart = horizontalRotation,
                    verticalRotationStart = verticalRotation,
                    step = 0,
                    distance = distance,
                    yScale = NETHER_TUNNEL_Y_SCALE,
                    mask = mask,
                )
            }
        }
    }

    private fun createRoom(
        chunk: Chunk,
        x: Double,
        y: Double,
        z: Double,
        thickness: Float,
        mask: BooleanArray,
    ) {
        val horizontalRadius = 1.5 + Mth.sin(HALF_PI) * thickness
        carveEllipsoid(chunk, x + 1.0, y, z, horizontalRadius, horizontalRadius * ROOM_Y_SCALE, mask)
    }

    @Suppress("LongParameterList")
    private fun createTunnel(
        chunk: Chunk,
        tunnelSeed: Long,
        xStart: Double,
        yStart: Double,
        zStart: Double,
        thickness: Float,
        horizontalRotationStart: Float,
        verticalRotationStart: Float,
        step: Int,
        distance: Int,
        yScale: Double,
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
                createTunnel(
                    chunk,
                    random.nextLong(),
                    x,
                    y,
                    z,
                    random.nextFloat() * 0.5f + 0.5f,
                    horizontalRotation - HALF_PI,
                    verticalRotation / 3.0f,
                    currentStep,
                    distance,
                    1.0,
                    mask,
                )
                createTunnel(
                    chunk,
                    random.nextLong(),
                    x,
                    y,
                    z,
                    random.nextFloat() * 0.5f + 0.5f,
                    horizontalRotation + HALF_PI,
                    verticalRotation / 3.0f,
                    currentStep,
                    distance,
                    1.0,
                    mask,
                )
                return
            }

            if (random.nextInt(4) != 0) {
                if (!canReach(chunk, x, z, currentStep, distance, thickness)) return
                carveEllipsoid(chunk, x, y, z, horizontalRadius, verticalRadius, mask)
            }
        }
    }

    private fun carveEllipsoid(
        chunk: Chunk,
        x: Double,
        y: Double,
        z: Double,
        horizontalRadius: Double,
        verticalRadius: Double,
        mask: BooleanArray,
    ) {
        val centerX = chunk.chunkX * 16 + 8.0
        val centerZ = chunk.chunkZ * 16 + 8.0
        val maxDelta = 16.0 + horizontalRadius * 2.0
        if (abs(x - centerX) > maxDelta || abs(z - centerZ) > maxDelta) return

        val chunkMinX = chunk.chunkX * 16
        val chunkMinZ = chunk.chunkZ * 16
        val minLocalX = maxOf(Mth.floor(x - horizontalRadius) - chunkMinX - 1, 0)
        val maxLocalX = minOf(Mth.floor(x + horizontalRadius) - chunkMinX, 15)
        val minY = maxOf(Mth.floor(y - verticalRadius) - 1, MIN_GENERATION_Y + 1)
        val maxY =
            minOf(
                Mth.floor(y + verticalRadius) + 1,
                MIN_GENERATION_Y + GENERATION_HEIGHT - 1 - PROTECTED_BLOCKS_ON_TOP,
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
                for (worldY in maxY downTo minY + 1) {
                    val yd = (worldY - 0.5 - y) / verticalRadius
                    if (yd <= FLOOR_LEVEL || xd * xd + yd * yd + zd * zd >= 1.0) continue
                    val maskIndex = (worldY * 16 + localZ) * 16 + localX
                    if (mask[maskIndex]) continue
                    mask[maskIndex] = true
                    val oldState = chunk.getBlockStateId(localX, worldY, localZ)
                    if (canReplace(oldState)) {
                        chunk.setBlockStateId(
                            localX,
                            worldY,
                            localZ,
                            if (worldY <= MIN_GENERATION_Y + 31) Blocks.LAVA.stateId else Blocks.CAVE_AIR.stateId,
                        )
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

    private fun canReplace(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path in REPLACEABLE_BLOCKS

    private const val MIN_GENERATION_Y = 0
    private const val GENERATION_HEIGHT = 128
    private const val MAX_START_Y = 126
    private const val CARVER_RANGE = 8
    private const val CAVE_BOUND = 10
    private const val MAX_DISTANCE = 112
    private const val PROTECTED_BLOCKS_ON_TOP = 7
    private const val START_PROBABILITY = 0.2f
    private const val FLOOR_LEVEL = -0.7
    private const val ROOM_Y_SCALE = 0.5
    private const val NETHER_TUNNEL_Y_SCALE = 5.0
    private val PI_FLOAT: Float = kotlin.math.PI.toFloat()
    private val HALF_PI: Float = (kotlin.math.PI / 2.0).toFloat()
    private val TWO_PI: Float = (kotlin.math.PI * 2.0).toFloat()

    private val REPLACEABLE_BLOCKS: Set<String> =
        setOf(
            "stone",
            "granite",
            "diorite",
            "andesite",
            "tuff",
            "deepslate",
            "netherrack",
            "basalt",
            "blackstone",
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
            "crimson_nylium",
            "warped_nylium",
            "nether_wart_block",
            "warped_wart_block",
            "soul_sand",
            "soul_soil",
        )
}
