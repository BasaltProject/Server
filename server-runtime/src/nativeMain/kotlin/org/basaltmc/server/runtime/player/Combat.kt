package org.basaltmc.server.runtime.player

import kotlin.math.floor
import kotlin.math.sqrt

/**
 * How much of a hit actually lands: armour first, then resistance and protection.
 *
 * parity: `CombatRules` plus `LivingEntity.getDamageAfterArmorAbsorb`/`getDamageAfterMagicAbsorb`,
 * in that order, which is what makes armour worth less against a big hit than against a small one.
 */
public object Combat {
    public const val MAX_ARMOUR: Float = 20f
    private const val PROTECTION_DIVIDER = 25f
    private const val BASE_TOUGHNESS = 2f
    private const val MIN_ARMOUR_RATIO = 0.2f
    private const val TOUGHNESS_DIVIDER = 4f
    private const val RESISTANCE_PER_LEVEL = 5

    /** parity: `Attributes.SAFE_FALL_DISTANCE`, and the epsilon `calculateFallPower` adds. */
    public const val SAFE_FALL_DISTANCE: Double = 3.0
    private const val FALL_EPSILON = 1.0E-6

    /** parity: `CombatRules.getDamageAfterAbsorb`. */
    public fun afterArmour(
        damage: Float,
        armour: Float,
        toughness: Float,
    ): Float {
        val scale = BASE_TOUGHNESS + toughness / TOUGHNESS_DIVIDER
        val effective = (armour - damage / scale).coerceIn(armour * MIN_ARMOUR_RATIO, MAX_ARMOUR)
        return damage * (1f - effective / PROTECTION_DIVIDER)
    }

    /** parity: `CombatRules.getDamageAfterMagicAbsorb`, the enchantment half of the same idea. */
    public fun afterProtection(
        damage: Float,
        protection: Float,
    ): Float = damage * (1f - protection.coerceIn(0f, MAX_ARMOUR) / PROTECTION_DIVIDER)

    /**
     * What a fall of [fallDistance] blocks costs.
     *
     * parity: `LivingEntity.calculateFallDamage` over `calculateFallPower`: the distance minus the
     * three blocks of `Attributes.SAFE_FALL_DISTANCE`, floored, with vanilla's own epsilon so a
     * fall of exactly four blocks hurts.
     */
    public fun fallDamage(
        fallDistance: Double,
        safeFallDistance: Double = SAFE_FALL_DISTANCE,
    ): Int {
        val power = fallDistance + FALL_EPSILON - safeFallDistance
        return if (power <= 0.0) 0 else floor(power).toInt()
    }

    /**
     * How much of a swing has recharged, from zero to one.
     *
     * parity: `Player.getAttackStrengthScale(0.5)`, whose delay is twenty ticks over the attack
     * speed: a hand swings four times a second, an iron sword one and a half.
     */
    public fun attackStrengthScale(
        ticksSinceSwing: Int,
        attackSpeed: Double,
    ): Float {
        if (attackSpeed <= 0.0) return 1f
        val delay = 1.0 / attackSpeed * TICKS_PER_SECOND
        return ((ticksSinceSwing + SWING_HALF_TICK) / delay).toFloat().coerceIn(0f, 1f)
    }

    /**
     * parity: `Player.baseDamageScaleFactor`: a swing that has not recharged still lands a fifth of
     * the damage, and the rest comes back with the square of the charge.
     */
    public fun chargeScale(strength: Float): Float = MIN_CHARGE_DAMAGE + strength * strength * (1f - MIN_CHARGE_DAMAGE)

    /**
     * What the difficulty leaves of a hit dealt by something that is not a player.
     *
     * parity: `Player.hurtServer`: peaceful takes nothing, easy halves it and adds one back (never
     * more than the hit itself), normal is the hit, and hard is half again.
     */
    public fun afterDifficulty(
        damage: Float,
        difficulty: String,
    ): Float =
        when (difficulty) {
            "peaceful" -> 0f
            "easy" -> minOf(damage / 2f + 1f, damage)
            "hard" -> damage * 3f / 2f
            else -> damage
        }

    /** parity: `Player.attack`, where a critical hit is worth half again. */
    public const val CRITICAL_MULTIPLIER: Float = 1.5f

    /** parity: `getAttackStrengthScale(0.5)`'s own half tick, and the tick rate it divides. */
    private const val SWING_HALF_TICK = 0.5
    private const val TICKS_PER_SECOND = 20.0
    private const val MIN_CHARGE_DAMAGE = 0.2f

    /**
     * parity: `LivingEntity.knockback`: the speed keeps half of what it had, the push is taken off
     * it, and on the ground it also gets a lift capped at 0.4.
     */
    public fun knockback(
        power: Double,
        towardsX: Double,
        towardsZ: Double,
        velocityX: Double,
        velocityY: Double,
        velocityZ: Double,
        onGround: Boolean,
        knockbackResistance: Double = 0.0,
    ): DoubleArray {
        val strength = power * (1.0 - knockbackResistance)
        if (strength <= 0.0) return doubleArrayOf(velocityX, velocityY, velocityZ)
        val length = sqrt(towardsX * towardsX + towardsZ * towardsZ)
        if (length < KNOCKBACK_EPSILON) return doubleArrayOf(velocityX, velocityY, velocityZ)
        val pushX = towardsX / length * strength
        val pushZ = towardsZ / length * strength
        return doubleArrayOf(
            velocityX / 2.0 - pushX,
            if (onGround) minOf(MAX_KNOCKBACK_LIFT, velocityY / 2.0 + strength) else velocityY,
            velocityZ / 2.0 - pushZ,
        )
    }

    private const val KNOCKBACK_EPSILON = 1.0E-5
    private const val MAX_KNOCKBACK_LIFT = 0.4

    /**
     * parity: `LivingEntity.getDamageAfterMagicAbsorb`'s resistance case, which counts in fifths
     * and floors at zero rather than going negative at level four and above.
     */
    public fun afterResistance(
        damage: Float,
        amplifier: Int,
    ): Float {
        val level = (amplifier + 1) * RESISTANCE_PER_LEVEL
        val remaining = (PROTECTION_DIVIDER.toInt() - level).coerceAtLeast(0)
        return maxOf(damage * remaining / PROTECTION_DIVIDER, 0f)
    }
}
