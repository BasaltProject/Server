package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * One entity that is not a player: what `/summon` puts in the world and what `/damage`, `/ride`,
 * `/rotate`, `/swing` and `/attribute` then act on.
 *
 * Deliberately thin. There is no AI, no physics and no goal selector here, because there is none in
 * the server yet: this is the identity, the position and the state the commands change, which is
 * what makes those commands do something real rather than nothing. When mobs get behaviour it will
 * be built on the ECS in `server-entity` and this becomes its network-facing view.
 */
@OptIn(ExperimentalUuidApi::class)
public class ServerEntity(
    public val entityId: Int,
    public val uuid: Uuid,
    public val type: Identifier,
    public val protocolType: Int,
    public var x: Double,
    public var y: Double,
    public var z: Double,
    public var yaw: Float = 0f,
    public var pitch: Float = 0f,
) {
    /** What the spawn packet carries beyond the type: for a falling block, the state it holds. */
    public var objectData: Int = 0

    /** How fast it is going down, for the few entities this build moves itself. */
    public var velocityY: Double = 0.0

    /** Attribute base values that were changed from their default. parity: `AttributeMap`, without modifiers. */
    public val attributeValues: MutableMap<String, Double> = mutableMapOf()

    /**
     * Its health. Declared after [attributeValues] on purpose: a property initializer that reads a
     * property declared below it reads an uninitialized field, which on Kotlin/Native is a crash
     * rather than a null.
     */
    public var health: Double = DEFAULT_MAX_HEALTH

    /** Scoreboard tags, as `/tag` sets them on an entity. */
    public val tags: MutableSet<String> = mutableSetOf()

    /** What it is riding, and who is riding it. parity: `Entity.vehicle`/`passengers`. */
    public var vehicleId: Int? = null
    public val passengerIds: MutableSet<Int> = linkedSetOf()

    public var customName: String? = null

    /** Set when it has been killed or removed, so the sync pass tells clients to forget it. */
    public var removed: Boolean = false

    /** Modifiers by attribute then by id. parity: `AttributeInstance`'s three operations. */
    public val attributeModifiers: MutableMap<String, MutableMap<String, AttributeModifier>> = mutableMapOf()

    /** The base value of one attribute, or its vanilla default when nothing set it. */
    public fun attribute(id: String): Double = attributeValues[id] ?: Attribute.defaultFor(id) ?: 0.0

    /**
     * The value with modifiers applied. parity: `AttributeInstance.calculateValue`: every addition
     * first, then each multiply-base against the *base*, then each multiply-total against the
     * running total.
     */
    public fun attributeWithModifiers(id: String): Double {
        val base = attribute(id)
        val modifiers = attributeModifiers[id]?.values.orEmpty()
        var value = base
        for (modifier in modifiers.filter { it.operation == AttributeOperation.ADD_VALUE }) {
            value += modifier.amount
        }
        val afterAdditions = value
        for (modifier in modifiers.filter { it.operation == AttributeOperation.ADD_MULTIPLIED_BASE }) {
            value += afterAdditions * modifier.amount
        }
        for (modifier in modifiers.filter { it.operation == AttributeOperation.ADD_MULTIPLIED_TOTAL }) {
            value *= 1.0 + modifier.amount
        }
        return value
    }

    public companion object {
        public const val DEFAULT_MAX_HEALTH: Double = 20.0
    }
}

/** parity: `AttributeModifier.Operation`, whose serialized names are what `/attribute` takes. */
public enum class AttributeOperation(
    public val serializedName: String,
) {
    ADD_VALUE("add_value"),
    ADD_MULTIPLIED_BASE("add_multiplied_base"),
    ADD_MULTIPLIED_TOTAL("add_multiplied_total"),
    ;

    public companion object {
        public fun byName(name: String): AttributeOperation? = entries.firstOrNull { it.serializedName == name }
    }
}

/** One modifier on one attribute. parity: `AttributeModifier`, keyed by id as 1.21 made it. */
public data class AttributeModifier(
    public val id: String,
    public val amount: Double,
    public val operation: AttributeOperation,
)

/** The attribute ids commands name, as `minecraft:` ids. parity: `Attributes`. */
public object Attribute {
    public const val MAX_HEALTH: String = "minecraft:max_health"
    public const val MOVEMENT_SPEED: String = "minecraft:movement_speed"
    public const val ATTACK_DAMAGE: String = "minecraft:attack_damage"
    public const val ARMOR: String = "minecraft:armor"
    public const val SCALE: String = "minecraft:scale"

    /** parity: `Attributes`' own defaults for the ones a command is likely to touch. */
    public val DEFAULTS: Map<String, Double> =
        mapOf(
            MAX_HEALTH to 20.0,
            MOVEMENT_SPEED to 0.7,
            ATTACK_DAMAGE to 2.0,
            ARMOR to 0.0,
            SCALE to 1.0,
        )

    public fun defaultFor(id: String): Double? = DEFAULTS[id]
}
