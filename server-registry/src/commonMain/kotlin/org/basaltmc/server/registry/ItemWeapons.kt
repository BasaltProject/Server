package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** What holding an item adds to the hand: how hard it hits and how fast it swings back. */
public class WeaponStats internal constructor(
    public val attackDamage: Double,
    public val attackSpeed: Double,
)

/**
 * parity: the `mainhand` half of every item's `minecraft:attribute_modifiers`.
 *
 * Both numbers are modifiers, not totals: a player swings at one damage and four speed with nothing
 * in hand, and the item is added to that.
 */
public object ItemWeapons {
    private val byItem: Map<Identifier, WeaponStats> =
        ALL_ITEM_WEAPONS.associate { encoded ->
            val fields = encoded.split('|')
            Identifier.vanilla(fields[0]) to WeaponStats(fields[1].toDouble(), fields[2].toDouble())
        }

    public fun of(item: Identifier?): WeaponStats? = item?.let { byItem[it] }

    /** parity: `Attributes.ATTACK_DAMAGE`'s own default for a player. */
    public const val BASE_ATTACK_DAMAGE: Double = 1.0

    /** parity: `Attributes.ATTACK_SPEED`'s default: four swings a second with an empty hand. */
    public const val BASE_ATTACK_SPEED: Double = 4.0
}
