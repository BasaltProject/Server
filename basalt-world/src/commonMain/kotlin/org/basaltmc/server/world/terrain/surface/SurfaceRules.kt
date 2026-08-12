package org.basaltmc.server.world.terrain.surface

import org.basaltmc.server.world.terrain.BiomeTemperature
import org.basaltmc.wire.common.Mth

/** // parity: net.minecraft.world.level.levelgen.SurfaceRules.Condition. */
public fun interface Condition {
    public fun test(): Boolean
}

/** // parity: net.minecraft.world.level.levelgen.SurfaceRules.ConditionSource. */
public fun interface ConditionSource {
    public fun apply(context: SurfaceRuleContext): Condition
}

/** // parity: net.minecraft.world.level.levelgen.SurfaceRules.SurfaceRule. `null` means "no match, try the next rule". */
public fun interface SurfaceRule {
    public fun tryApply(
        x: Int,
        y: Int,
        z: Int,
    ): Int?
}

/** // parity: net.minecraft.world.level.levelgen.SurfaceRules.RuleSource. */
public fun interface RuleSource {
    public fun apply(context: SurfaceRuleContext): SurfaceRule
}

/** // parity: net.minecraft.world.level.levelgen.SurfaceRules, the static factory methods and node types. */
public object SurfaceRules {
    public val ON_FLOOR: ConditionSource = stoneDepthCheck(0, false, 0, ceiling = false)
    public val UNDER_FLOOR: ConditionSource = stoneDepthCheck(0, true, 0, ceiling = false)
    public val DEEP_UNDER_FLOOR: ConditionSource = stoneDepthCheck(0, true, 6, ceiling = false)
    public val VERY_DEEP_UNDER_FLOOR: ConditionSource = stoneDepthCheck(0, true, 30, ceiling = false)
    public val ON_CEILING: ConditionSource = stoneDepthCheck(0, false, 0, ceiling = true)
    public val UNDER_CEILING: ConditionSource = stoneDepthCheck(0, true, 0, ceiling = true)

    public fun stoneDepthCheck(
        offset: Int,
        addSurfaceDepth: Boolean,
        secondaryDepthRange: Int,
        ceiling: Boolean,
    ): ConditionSource =
        ConditionSource { context ->
            Condition {
                val stoneDepth = if (ceiling) context.stoneDepthBelow else context.stoneDepthAbove
                val surfaceDepth = if (addSurfaceDepth) context.surfaceDepth else 0
                val secondarySurfaceDepth =
                    if (secondaryDepthRange == 0) {
                        0
                    } else {
                        Mth.map(context.surfaceSecondary(), -1.0, 1.0, 0.0, secondaryDepthRange.toDouble()).toInt()
                    }
                stoneDepth <= 1 + offset + surfaceDepth + secondarySurfaceDepth
            }
        }

    public fun not(target: ConditionSource): ConditionSource =
        ConditionSource { context ->
            val targetCondition = target.apply(context)
            Condition { !targetCondition.test() }
        }

    public fun yBlockCheck(
        anchor: VerticalAnchor,
        surfaceDepthMultiplier: Int,
    ): ConditionSource = yCondition(anchor, surfaceDepthMultiplier, addStoneDepth = false)

    public fun yStartCheck(
        anchor: VerticalAnchor,
        surfaceDepthMultiplier: Int,
    ): ConditionSource = yCondition(anchor, surfaceDepthMultiplier, addStoneDepth = true)

    private fun yCondition(
        anchor: VerticalAnchor,
        surfaceDepthMultiplier: Int,
        addStoneDepth: Boolean,
    ): ConditionSource =
        ConditionSource { context ->
            val anchorY = anchor.resolveY(context.worldGenContext)
            Condition {
                context.blockY + (if (addStoneDepth) context.stoneDepthAbove else 0) >=
                    anchorY + context.surfaceDepth * surfaceDepthMultiplier
            }
        }

    public fun waterBlockCheck(
        offset: Int,
        surfaceDepthMultiplier: Int,
    ): ConditionSource = waterCondition(offset, surfaceDepthMultiplier, addStoneDepth = false)

    public fun waterStartCheck(
        offset: Int,
        surfaceDepthMultiplier: Int,
    ): ConditionSource = waterCondition(offset, surfaceDepthMultiplier, addStoneDepth = true)

    private fun waterCondition(
        offset: Int,
        surfaceDepthMultiplier: Int,
        addStoneDepth: Boolean,
    ): ConditionSource =
        ConditionSource { context ->
            Condition {
                context.waterHeight == Int.MIN_VALUE ||
                    context.blockY + (if (addStoneDepth) context.stoneDepthAbove else 0) >=
                    context.waterHeight + offset + context.surfaceDepth * surfaceDepthMultiplier
            }
        }

    public fun isBiome(vararg target: String): ConditionSource {
        val biomes = target.toSet()
        return ConditionSource { context -> Condition { context.biome() in biomes } }
    }

    public fun noiseCondition2d(
        noiseId: String,
        minRange: Double,
        maxRange: Double = Double.MAX_VALUE,
    ): ConditionSource =
        ConditionSource { context -> Condition { context.sample2d(noiseId).let { it >= minRange && it <= maxRange } } }

    public fun noiseCondition3d(
        noiseId: String,
        minRange: Double,
        maxRange: Double = Double.MAX_VALUE,
    ): ConditionSource =
        ConditionSource { context -> Condition { context.sample3d(noiseId).let { it >= minRange && it <= maxRange } } }

    public fun verticalGradient(
        randomName: String,
        trueAtAndBelow: VerticalAnchor,
        falseAtAndAbove: VerticalAnchor,
    ): ConditionSource =
        ConditionSource { context ->
            val trueY = trueAtAndBelow.resolveY(context.worldGenContext)
            val falseY = falseAtAndAbove.resolveY(context.worldGenContext)
            Condition {
                val y = context.blockY
                when {
                    y <= trueY -> true
                    y >= falseY -> false
                    else -> {
                        val probability = Mth.map(y.toDouble(), trueY.toDouble(), falseY.toDouble(), 1.0, 0.0)
                        context.verticalGradientRandom(randomName) < probability
                    }
                }
            }
        }

    public fun steep(): ConditionSource = ConditionSource { context -> Condition { context.isSteep() } }

    public fun hole(): ConditionSource = ConditionSource { context -> Condition { context.isHole() } }

    public fun abovePreliminarySurface(): ConditionSource =
        ConditionSource { context ->
            Condition {
                context.isAbovePreliminarySurface()
            }
        }

    /** // parity: SurfaceRules.temperature/Context.TemperatureHelperCondition, itself Biome.coldEnoughToSnow. */
    public fun temperature(): ConditionSource =
        ConditionSource { context ->
            Condition {
                BiomeTemperature.coldEnoughToSnow(context.biome(), context.blockX, context.blockY, context.blockZ)
            }
        }

    public fun ifTrue(
        condition: ConditionSource,
        thenRun: RuleSource,
    ): RuleSource =
        RuleSource { context ->
            val builtCondition = condition.apply(context)
            val builtRule = thenRun.apply(context)
            SurfaceRule { x, y, z -> if (builtCondition.test()) builtRule.tryApply(x, y, z) else null }
        }

    public fun sequence(vararg rules: RuleSource): RuleSource {
        require(rules.isNotEmpty()) { "Need at least 1 rule for a sequence" }
        return RuleSource { context ->
            val built = rules.map { it.apply(context) }
            SurfaceRule { x, y, z -> built.firstNotNullOfOrNull { it.tryApply(x, y, z) } }
        }
    }

    public fun state(stateId: Int): RuleSource = RuleSource { SurfaceRule { _, _, _ -> stateId } }

    public fun bandlands(): RuleSource = RuleSource { context -> SurfaceRule { x, y, z -> context.clayBand() } }
}
