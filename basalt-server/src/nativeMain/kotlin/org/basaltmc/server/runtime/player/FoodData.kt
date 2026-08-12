package org.basaltmc.server.runtime.player

import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sqrt

/** The four difficulties, in vanilla's own order (the id the protocol uses is the ordinal). */
public enum class Difficulty {
    PEACEFUL,
    EASY,
    NORMAL,
    HARD,
}

/**
 * Hunger, saturation and exhaustion, ticked once per server tick per player.
 *
 * A direct reimplementation of vanilla's `FoodData`: the numbers here (4.0 exhaustion per
 * saturation point, regeneration at 18 food, starvation every 80 ticks, the 40.0 exhaustion cap)
 * are the mechanic, not tuning, and farms are built on their exact timing.
 *
 * [heal] and [hurt] are handed in rather than reached for, because health lives on the player and
 * this class is only about food.
 */
public class FoodData {
    public var foodLevel: Int = MAX_FOOD
        private set
    public var saturationLevel: Float = DEFAULT_SATURATION
        private set
    public var exhaustionLevel: Float = 0f
        private set

    private var tickTimer: Int = 0

    public fun addExhaustion(amount: Float) {
        exhaustionLevel = min(exhaustionLevel + amount, MAX_EXHAUSTION)
    }

    public fun eat(
        food: Int,
        saturation: Float,
    ) {
        foodLevel = (foodLevel + food).coerceIn(0, MAX_FOOD)
        saturationLevel = (saturationLevel + saturation).coerceIn(0f, foodLevel.toFloat())
    }

    public fun set(
        food: Int,
        saturation: Float,
    ) {
        foodLevel = food
        saturationLevel = saturation
    }

    /** Puts back exactly what was persisted, exhaustion included: a rejoin should not reset the hunger clock. */
    public fun restore(
        food: Int,
        saturation: Float,
        exhaustion: Float,
    ) {
        foodLevel = food
        saturationLevel = saturation
        exhaustionLevel = exhaustion
        tickTimer = 0
    }

    public fun reset() {
        foodLevel = MAX_FOOD
        saturationLevel = DEFAULT_SATURATION
        exhaustionLevel = 0f
        tickTimer = 0
    }

    /**
     * One tick. [health] is the player's current health, [heal] restores it, [hurt] applies
     * starvation damage; all three keep this class free of the rest of the player.
     *
     * parity: `net.minecraft.world.food.FoodData.tick`. `naturalRegeneration` is assumed on
     * (vanilla's own game rule default); game rules themselves are a documented gap.
     */
    public fun tick(
        difficulty: Difficulty,
        health: Float,
        maxHealth: Float,
        heal: (Float) -> Unit,
        hurt: (Float) -> Unit,
    ) {
        if (exhaustionLevel > EXHAUSTION_PER_POINT) {
            exhaustionLevel -= EXHAUSTION_PER_POINT
            if (saturationLevel > 0f) {
                saturationLevel = max(saturationLevel - 1f, 0f)
            } else if (difficulty != Difficulty.PEACEFUL) {
                foodLevel = max(foodLevel - 1, 0)
            }
        }

        val hurtAlready = health < maxHealth
        when {
            saturationLevel > 0f && hurtAlready && foodLevel >= MAX_FOOD -> {
                tickTimer++
                if (tickTimer >= SATURATED_REGEN_TICKS) {
                    val spent = min(saturationLevel, SATURATED_REGEN_COST)
                    heal(spent / SATURATED_REGEN_COST)
                    addExhaustion(spent)
                    tickTimer = 0
                }
            }
            foodLevel >= REGEN_FOOD_THRESHOLD && hurtAlready -> {
                tickTimer++
                if (tickTimer >= REGEN_TICKS) {
                    heal(1f)
                    addExhaustion(REGEN_EXHAUSTION)
                    tickTimer = 0
                }
            }
            foodLevel <= 0 -> {
                tickTimer++
                if (tickTimer >= STARVE_TICKS) {
                    val mayStarve =
                        health > STARVE_FLOOR_EASY ||
                            difficulty == Difficulty.HARD ||
                            (health > STARVE_FLOOR_HARD && difficulty == Difficulty.NORMAL)
                    if (mayStarve) hurt(1f)
                    tickTimer = 0
                }
            }
            else -> tickTimer = 0
        }
    }

    public companion object {
        public const val MAX_FOOD: Int = 20
        public const val DEFAULT_SATURATION: Float = 5f

        private const val MAX_EXHAUSTION = 40f
        private const val EXHAUSTION_PER_POINT = 4f
        private const val REGEN_FOOD_THRESHOLD = 18
        private const val REGEN_TICKS = 80
        private const val REGEN_EXHAUSTION = 6f
        private const val SATURATED_REGEN_TICKS = 10
        private const val SATURATED_REGEN_COST = 6f
        private const val STARVE_TICKS = 80
        private const val STARVE_FLOOR_EASY = 10f
        private const val STARVE_FLOOR_HARD = 1f

        /**
         * What moving [dx]/[dz] costs in exhaustion.
         *
         * parity: `ServerPlayer.checkMovementStatistics`. Walking and crouching really are free in
         * 26.2 (their multiplier is literally `0.0F` in the jar); sprinting costs 0.1 per block.
         * Swimming and jumping also cost there, and are a documented gap here: neither fluid state
         * nor a jump is observable from the packets this server handles yet.
         */
        public fun movementExhaustion(
            dx: Double,
            dz: Double,
            sprinting: Boolean,
            onGround: Boolean,
        ): Float {
            if (!onGround || !sprinting) return 0f
            val centimetres = (sqrt(dx * dx + dz * dz).toFloat() * 100f).roundToInt()
            if (centimetres <= 0) return 0f
            return SPRINT_EXHAUSTION_PER_CM * centimetres * 0.01f
        }

        private const val SPRINT_EXHAUSTION_PER_CM = 0.1f
    }
}
