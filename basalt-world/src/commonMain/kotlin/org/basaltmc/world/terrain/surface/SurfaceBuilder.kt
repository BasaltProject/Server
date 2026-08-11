package org.basaltmc.world.terrain.surface

import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.terrain.BiomeTemperature
import kotlin.math.abs
import kotlin.math.ceil

/**
 * // parity: net.minecraft.world.level.levelgen.SurfaceSystem.buildSurface (the per-column
 * top-down traversal plus the eroded-badlands and frozen-ocean extensions.
 *
 * Walks each column top-down from its real generated surface height down to the world floor,
 * tracking `stoneAboveDepth` (resets to 0 on air, increments on solid, mirroring vanilla exactly)
 * and a lookahead-cached `stoneBelowDepth` (distance down to the next non-stone block, the real
 * "how deep is the cave ceiling below me" measure `UNDER_CEILING`-style rules need), then replaces
 * every block that's still exactly [defaultBlockStateId] (real terrain, not bedrock or a block an
 * earlier column iteration already replaced) with whatever the rule tree returns, if anything.
 */
public object SurfaceBuilder {
    /**
     * // parity: `SurfaceSystem.topMaterial`, used by carvers to repair dirt immediately below a
     * grass or mycelium surface they opened. Vanilla evaluates the normal surface rule with both
     * stone depths fixed to one and only exposes the carved fluid as the synthetic water height.
     */
    public fun topMaterial(
        environment: SurfaceRuleEnvironment,
        ruleSource: RuleSource,
        x: Int,
        y: Int,
        z: Int,
        underFluid: Boolean,
        generationMinY: Int = WorldHeight.MIN_Y,
        generationHeight: Int = WorldHeight.HEIGHT,
    ): Int? {
        val context = SurfaceRuleContext(environment, WorldGenerationContext(generationMinY, generationHeight))
        val rule = ruleSource.apply(context)
        context.updateXZ(x, z)
        context.updateY(1, 1, if (underFluid) y + 1 else Int.MIN_VALUE, y)
        return rule.tryApply(x, y, z)
    }

    public fun buildSurface(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
        ruleSource: RuleSource,
        defaultBlockStateId: Int = Blocks.STONE.stateId,
        generationMinY: Int = chunk.verticalBounds.minY,
        generationHeight: Int = chunk.verticalBounds.height,
    ) {
        // parity: WorldGenerationContext intersects the dimension height with the chunk
        // generator's noise settings. Nether chunks are 256 blocks tall but generate only 128.
        val worldGenContext = WorldGenerationContext(generationMinY, generationHeight)
        val context = SurfaceRuleContext(environment, worldGenContext)
        val rule = ruleSource.apply(context)
        val minBlockX = chunk.chunkX * 16
        val minBlockZ = chunk.chunkZ * 16
        val endY = chunk.verticalBounds.minY

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val blockX = minBlockX + x
                val blockZ = minBlockZ + z
                val initialHeight = chunk.heightAt(x, z, Chunk.HeightmapType.WORLD_SURFACE)
                val startingHeight = initialHeight + 1
                val surfaceBiome = environment.biomeAt(blockX, startingHeight, blockZ)
                if (surfaceBiome == "minecraft:eroded_badlands") {
                    erodedBadlandsExtension(
                        chunk,
                        environment,
                        x,
                        z,
                        blockX,
                        blockZ,
                        startingHeight,
                        defaultBlockStateId,
                    )
                }
                val height = chunk.heightAt(x, z, Chunk.HeightmapType.WORLD_SURFACE)
                context.updateXZ(blockX, blockZ)

                var stoneAboveDepth = 0
                var waterHeight = Int.MIN_VALUE
                var nextCeilingStoneY = Int.MAX_VALUE

                var y = height
                while (y >= endY) {
                    val old = blockAt(chunk, x, y, z)
                    when {
                        Blocks.isAir(old) -> {
                            stoneAboveDepth = 0
                            waterHeight = Int.MIN_VALUE
                        }
                        isFluid(old) -> {
                            if (waterHeight == Int.MIN_VALUE) waterHeight = y + 1
                        }
                        else -> {
                            if (nextCeilingStoneY >= y) {
                                nextCeilingStoneY = WAY_BELOW_MIN_Y
                                var lookaheadY = y - 1
                                while (lookaheadY >= endY - 1) {
                                    if (!isStone(blockAt(chunk, x, lookaheadY, z))) {
                                        nextCeilingStoneY = lookaheadY + 1
                                        break
                                    }
                                    lookaheadY--
                                }
                            }
                            stoneAboveDepth++
                            val stoneBelowDepth = y - nextCeilingStoneY + 1
                            context.updateY(stoneAboveDepth, stoneBelowDepth, waterHeight, y)
                            if (old == defaultBlockStateId) {
                                val replacement = rule.tryApply(blockX, y, blockZ)
                                if (replacement != null) chunk.setBlockStateId(x, y, z, replacement)
                            }
                        }
                    }
                    y--
                }

                if (surfaceBiome == "minecraft:frozen_ocean" || surfaceBiome == "minecraft:deep_frozen_ocean") {
                    frozenOceanExtension(
                        chunk,
                        environment,
                        x,
                        z,
                        blockX,
                        blockZ,
                        startingHeight,
                        surfaceBiome,
                    )
                }
            }
        }
    }

    private fun erodedBadlandsExtension(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
        localX: Int,
        localZ: Int,
        blockX: Int,
        blockZ: Int,
        height: Int,
        defaultBlockStateId: Int,
    ) {
        val pillarBuffer =
            minOf(
                abs(environment.sample2dScaled("badlands_surface", blockX.toDouble(), blockZ.toDouble()) * 8.25),
                environment.sample2dScaled("badlands_pillar", blockX * 0.2, blockZ * 0.2) * 15.0,
            )
        if (pillarBuffer <= 0.0) return
        val pillarFloor =
            abs(environment.sample2dScaled("badlands_pillar_roof", blockX * 0.75, blockZ * 0.75) * 1.5)
        val extensionTop = 64.0 + minOf(pillarBuffer * pillarBuffer * 2.5, ceil(pillarFloor * 50.0) + 24.0)
        val startY = kotlin.math.floor(extensionTop).toInt()
        if (height > startY) return

        for (y in startY downTo chunk.verticalBounds.minY) {
            val stateId = blockAt(chunk, localX, y, localZ)
            if (stateId == defaultBlockStateId) break
            if (stateId == Blocks.WATER.stateId) return
        }
        var y = startY
        while (y >= chunk.verticalBounds.minY && Blocks.isAir(blockAt(chunk, localX, y, localZ))) {
            setBlock(chunk, localX, y, localZ, defaultBlockStateId)
            y--
        }
    }

    private fun frozenOceanExtension(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
        localX: Int,
        localZ: Int,
        blockX: Int,
        blockZ: Int,
        height: Int,
        surfaceBiome: String,
    ) {
        val iceberg =
            minOf(
                abs(environment.sample2dScaled("iceberg_surface", blockX.toDouble(), blockZ.toDouble()) * 8.25),
                environment.sample2dScaled("iceberg_pillar", blockX * 1.28, blockZ * 1.28) * 15.0,
            )
        if (iceberg <= 1.8) return

        val icebergRoof =
            abs(environment.sample2dScaled("iceberg_pillar_roof", blockX * 1.17, blockZ * 1.17) * 1.5)
        var top = minOf(iceberg * iceberg * 1.2, ceil(icebergRoof * 40.0) + 14.0)
        if (BiomeTemperature.shouldMeltFrozenOceanIcebergSlightly(surfaceBiome, blockX, SEA_LEVEL, blockZ)) top -= 2.0

        val extensionBottom: Double
        if (top > 2.0) {
            extensionBottom = SEA_LEVEL - top - 7.0
            top += SEA_LEVEL
        } else {
            top = 0.0
            extensionBottom = 0.0
        }

        val random = environment.positionalRandom(blockX, 0, blockZ)
        val maxSnowDepth = 2 + random.nextInt(4)
        val minSnowHeight = SEA_LEVEL + 18 + random.nextInt(10)
        var snowDepth = 0
        val minSurfaceLevel =
            environment.estimatedSurfaceHeight(blockX, blockZ) +
                environment.surfaceDepthNoise(blockX, blockZ) -
                8

        for (y in maxOf(height, top.toInt() + 1) downTo minSurfaceLevel) {
            val stateId = blockAt(chunk, localX, y, localZ)
            val fillAir = Blocks.isAir(stateId) && y < top.toInt() && random.nextDouble() > 0.01
            val fillWater =
                stateId == Blocks.WATER.stateId &&
                    y > extensionBottom.toInt() &&
                    y < SEA_LEVEL &&
                    extensionBottom != 0.0 &&
                    random.nextDouble() > 0.15
            if (fillAir || fillWater) {
                if (snowDepth <= maxSnowDepth && y > minSnowHeight) {
                    setBlock(chunk, localX, y, localZ, Blocks.SNOW_BLOCK.stateId)
                    snowDepth++
                } else {
                    setBlock(chunk, localX, y, localZ, Blocks.PACKED_ICE.stateId)
                }
            }
        }
    }

    private fun blockAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int =
        if (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.getBlockStateId(x, y, z)
        } else {
            Blocks.AIR.stateId
        }

    private fun setBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) chunk.setBlockStateId(x, y, z, stateId)
    }

    private fun isFluid(stateId: Int): Boolean = Blocks.hasFluid(stateId)

    private fun isStone(stateId: Int): Boolean = !Blocks.isAir(stateId) && !isFluid(stateId)

    // A sentinel far enough below any real Y that `stoneBelowDepth` computed from it reads as
    // "solid all the way down, no ceiling found": matches vanilla's own `DimensionType.WAY_BELOW_MIN_Y`
    // role without needing its exact value (only used as a very-negative marker, never read directly).
    private const val WAY_BELOW_MIN_Y = Int.MIN_VALUE / 2
    private const val SEA_LEVEL = 63
}
