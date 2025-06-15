package org.basaltmc.server.runtime.player

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
