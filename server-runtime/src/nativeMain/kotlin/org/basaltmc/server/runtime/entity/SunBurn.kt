package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.common.Identifier

/**
 * The undead catching fire in daylight.
 *
 * parity: `Mob.burnUndead` over `isSunBurnTick`: the light where it stands has to be over a half,
 * the sky has to be open above it, and even then it is a roll against thirty. Eight seconds of fire
 * a time, which is what makes a zombie caught in the open die rather than walk it off.
 */
public object SunBurn {
    /** parity: the mobs whose `isSunSensitive` answers true. */
    private val BURNS =
        setOf(
            "zombie",
            "zombie_villager",
            "husk",
            "drowned",
            "skeleton",
            "stray",
            "bogged",
            "phantom",
            "giant",
        )

    /** parity: `igniteForSeconds(8)`. */
    public const val FIRE_TICKS: Int = 160

    /** parity: `Entity.baseTick`, which takes a point off every twenty ticks of burning. */
    public const val DAMAGE_INTERVAL: Int = 20
    public const val FIRE_DAMAGE: Float = 1f

    public fun burns(type: Identifier): Boolean = type.path in BURNS

    /**
     * Whether this is a tick it catches fire on. [light] is the light where it stands and [openSky]
     * whether anything is over it.
     */
    public fun shouldBurn(
        type: Identifier,
        day: Boolean,
        light: Int,
        openSky: Boolean,
        inWater: Boolean,
        random: BasaltRandom,
    ): Boolean {
        if (!day || !openSky || inWater || !burns(type)) return false
        // parity: `getLightLevelDependentMagicValue`, which is the light over fifteen, and the roll
        // against thirty that keeps a mob in dim light alive a while longer.
        val brightness = light / MAX_LIGHT
        if (brightness <= HALF) return false
        return random.nextFloat() * ROLL < (brightness - MARGIN) * 2f
    }

    private const val MAX_LIGHT = 15f
    private const val HALF = 0.5f
    private const val MARGIN = 0.4f
    private const val ROLL = 30f
}
