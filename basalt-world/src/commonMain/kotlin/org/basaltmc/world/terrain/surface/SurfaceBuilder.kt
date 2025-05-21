package org.basaltmc.world.terrain.surface

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.SurfaceSystem.buildSurface (the per-column
 * top-down traversal only; `erodedBadlandsExtension`/`frozenOceanExtension` are biome-specific
 * post-passes Plains never triggers, not ported).
 *
 * Walks each column top-down from its real generated surface height down to the world floor,
 * tracking `stoneAboveDepth` (resets to 0 on air, increments on solid, mirroring vanilla exactly)
 * and a lookahead-cached `stoneBelowDepth` (distance down to the next non-stone block, the real
 * "how deep is the cave ceiling below me" measure `UNDER_CEILING`-style rules need), then replaces
 * every block that's still exactly [defaultBlockStateId] (real terrain, not bedrock or a block an
 * earlier column iteration already replaced) with whatever the rule tree returns, if anything.
 */
public object SurfaceBuilder {
    public fun buildSurface(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
        ruleSource: RuleSource,
        defaultBlockStateId: Int = Blocks.STONE.stateId,
    ) {
        val worldGenContext = WorldGenerationContext(WorldHeight.MIN_Y, WorldHeight.HEIGHT)
        val context = SurfaceRuleContext(environment, worldGenContext)
        val rule = ruleSource.apply(context)
        val heightmap = chunk.heightmap()

        val minBlockX = chunk.chunkX * 16
        val minBlockZ = chunk.chunkZ * 16
        val endY = WorldHeight.MIN_Y

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val blockX = minBlockX + x
                val blockZ = minBlockZ + z
                val height = heightmap[x + z * 16]
                context.updateXZ(blockX, blockZ)

                var stoneAboveDepth = 0
                var waterHeight = Int.MIN_VALUE
                var nextCeilingStoneY = Int.MAX_VALUE

                var y = height
                while (y >= endY) {
                    val old = blockAt(chunk, x, y, z)
                    when {
                        old == Blocks.AIR.stateId -> {
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
            }
        }
    }

    private fun blockAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int = if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) chunk.getBlockStateId(x, y, z) else Blocks.AIR.stateId

    // Every fluid this project models is water for now (no lava placement yet): matches this
    // generator's own documented "no aquifers/lava" gap elsewhere.
    private fun isFluid(stateId: Int): Boolean = stateId == WATER_STATE_ID

    private fun isStone(stateId: Int): Boolean = stateId != Blocks.AIR.stateId && !isFluid(stateId)

    private val WATER_STATE_ID = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("water"))?.stateId ?: -1

    // A sentinel far enough below any real Y that `stoneBelowDepth` computed from it reads as
    // "solid all the way down, no ceiling found": matches vanilla's own `DimensionType.WAY_BELOW_MIN_Y`
    // role without needing its exact value (only used as a very-negative marker, never read directly).
    private const val WAY_BELOW_MIN_Y = Int.MIN_VALUE / 2
}
