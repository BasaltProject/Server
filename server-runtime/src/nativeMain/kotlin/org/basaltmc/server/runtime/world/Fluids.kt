package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** What a player is standing in, which is all the drowning and burning rules need to know. */
public enum class Submersion {
    NONE,
    WATER,
    LAVA,
}

/**
 * Drowning and burning, the two things being under something does to a player.
 *
 * parity: `LivingEntity.baseTick` for the air supply, `Entity.lavaHurt` for the lava. Both are the
 * server's job: the client draws the bubbles from the air supply it is sent and the flames from the
 * shared flags, and does neither on its own.
 */
public object Fluids {
    private val WATER = Identifier.vanilla("water")
    private val LAVA = Identifier.vanilla("lava")

    /** parity: `Entity.getMaxAirSupply`, the three hundred ticks of air the bar is drawn from. */
    public const val MAX_AIR: Int = 300

    /** parity: `LivingEntity.shouldTakeDrowningDamage`: air runs twenty ticks past empty first. */
    public const val DROWNING_AIR: Int = -20

    public const val DROWN_DAMAGE: Float = 2f

    /** parity: `Entity.lavaHurt`, four hearts' worth and fifteen seconds alight. */
    public const val LAVA_DAMAGE: Float = 4f
    public const val LAVA_FIRE_TICKS: Int = 15 * 20
    public const val FIRE_DAMAGE: Float = 1f

    /** parity: `Entity.getEyeY`, near enough: this build has no poses, so a sneaking player is the low one. */
    public fun eyeHeight(sneaking: Boolean): Double = if (sneaking) 1.27 else 1.62

    public fun of(stateId: Int): Submersion {
        val block = Blocks.byStateId(stateId)?.block ?: return Submersion.NONE
        return when (block) {
            WATER -> Submersion.WATER
            LAVA -> Submersion.LAVA
            else -> Submersion.NONE
        }
    }

    /** parity: `LivingEntity.decreaseAirSupply`/`increaseAirSupply`: one tick down, four back up. */
    public fun airAfterTick(
        air: Int,
        eyesInWater: Boolean,
    ): Int =
        if (eyesInWater) {
            air - 1
        } else {
            minOf(air + 4, MAX_AIR)
        }
}
