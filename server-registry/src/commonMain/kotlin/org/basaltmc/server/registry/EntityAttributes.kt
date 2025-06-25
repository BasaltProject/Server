package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** One entity type's base value for one attribute. */
public class EntityAttributeEntry(
    public val entity: String,
    public val attribute: String,
    public val value: Double,
)

/**
 * What each kind of mob starts with: its health, how fast it walks, how far it notices you.
 *
 * parity: `DefaultAttributes`, which each entity class fills in through its own `createAttributes`.
 * A cow is not a zombie is not a rabbit, and every one of those numbers is observable.
 */
public object EntityAttributes {
    private val byEntity: Map<Identifier, Map<String, Double>> =
        ALL_ENTITY_ATTRIBUTES
            .groupBy { Identifier.vanilla(it.entity) }
            .mapValues { (_, rows) -> rows.associate { it.attribute to it.value } }

    /** The base value this type of entity has for [attribute], or null when it has no such attribute. */
    public fun baseValue(
        type: Identifier,
        attribute: String,
    ): Double? = byEntity[type]?.get(attribute.removePrefix("minecraft:"))

    public fun of(type: Identifier): Map<String, Double> = byEntity[type].orEmpty()

    /** Whether this type is alive at all, which is what having attributes means. */
    public fun isLiving(type: Identifier): Boolean = byEntity.containsKey(type)

    public const val MAX_HEALTH: String = "max_health"
    public const val MOVEMENT_SPEED: String = "movement_speed"
    public const val FOLLOW_RANGE: String = "follow_range"
    public const val ATTACK_DAMAGE: String = "attack_damage"
    public const val ARMOR: String = "armor"
    public const val ARMOR_TOUGHNESS: String = "armor_toughness"
    public const val KNOCKBACK_RESISTANCE: String = "knockback_resistance"
    public const val SAFE_FALL_DISTANCE: String = "safe_fall_distance"
}
