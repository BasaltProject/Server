package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.EntityTypeShapes
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

    /** How fast it is going, in blocks a tick. parity: `Entity.deltaMovement`. */
    public var velocityX: Double = 0.0
    public var velocityY: Double = 0.0
    public var velocityZ: Double = 0.0

    /** Whether the last move ended against something below. parity: `Entity.onGround`. */
    public var onGround: Boolean = false

    /** Whether the last move was stopped sideways. parity: `Entity.horizontalCollision`. */
    public var horizontallyBlocked: Boolean = false

    /**
     * Its age. parity: `AgeableMob.age`: negative is a baby counting up to nothing, positive is a
     * grown animal counting down the wait before it can breed again, and zero is ready.
     *
     * For anything that is not an animal this is simply the ticks it has lived.
     */
    public var age: Int = 0

    /** parity: `Animal.inLove`, the thirty seconds a fed animal spends looking for a partner. */
    public var loveTicks: Int = 0

    /** parity: `BreedGoal.loveTime`, the three seconds two of them have to stay together. */
    public var courtshipTicks: Int = 0

    /** parity: `PARENT_AGE_AFTER_BREEDING`, the five minutes before either can breed again. */
    public var breedCooldown: Int = 0

    /** parity: `VillagerData.profession`, which decides what it offers. */
    public var profession: String = "farmer"

    /** parity: `VillagerData.level` and `Villager.villagerXp`, which is what unlocks the next tier. */
    public var villagerLevel: Int = 1
    public var villagerExperience: Int = 0

    /** Ticks before it goes off, or [NO_FUSE] for anything that is not primed. parity: `PrimedTnt.fuse`. */
    public var fuse: Int = NO_FUSE

    /** parity: `LivingEntity.invulnerableTime`, the half second after a hit when most others miss. */
    public var hurtCooldown: Int = 0

    /** parity: `LivingEntity.lastHurt`: inside that window only a bigger hit lands, and only the rest of it. */
    public var lastHurt: Float = 0f

    /** Who shot it, by entity id. parity: `Projectile.owner`, which is who a kill is blamed on. */
    public var ownerId: Int? = null

    /** parity: `AbstractArrow.isCritArrow`, set by a fully drawn bow and worth extra damage. */
    public var critical: Boolean = false

    /** Who it is angry at, by entity id, or null when nothing has hit it. parity: `Mob.target`. */
    public var targetId: Int? = null

    /** parity: `Mob.attackTime`, the ticks a mob waits between swings. */
    public var attackCooldown: Int = 0

    /** parity: `MeleeAttackGoal.ticksUntilNextPathRecalculation`, so a chase does not repath a tick. */
    public var repathCooldown: Int = 0

    /** Where it is walking, as x/y/z, or null when it is standing still. parity: `MoveControl`'s wanted position. */
    public var walkTarget: DoubleArray? = null

    /** The way there, block by block. parity: `PathNavigation.path`, walked one node at a time. */
    public var path: List<IntArray>? = null

    /** Which node of it comes next. parity: `Path.nextNodeIndex`. */
    public var pathIndex: Int = 0

    /**
     * Ticks with nobody near it. parity: `Mob.noActionTime`, which is what the random despawn waits
     * on, and what standing beside a mob resets.
     */
    public var idleTicks: Int = 0

    /** Whether it is here to stay. parity: `Mob.isPersistenceRequired`, set by naming or by a command. */
    public var persistent: Boolean = false

    /** Attribute base values that were changed from their default. parity: `AttributeMap`, without modifiers. */
    public val attributeValues: MutableMap<String, Double> = mutableMapOf()

    /**
     * Its health. Declared after [attributeValues] on purpose: a property initializer that reads a
     * property declared below it reads an uninitialized field, which on Kotlin/Native is a crash
     * rather than a null.
     */
    public var health: Double = EntityAttributes.baseValue(type, EntityAttributes.MAX_HEALTH) ?: DEFAULT_MAX_HEALTH

    /** Scoreboard tags, as `/tag` sets them on an entity. */
    public val tags: MutableSet<String> = mutableSetOf()

    /** What it is riding, and who is riding it. parity: `Entity.vehicle`/`passengers`. */
    public var vehicleId: Int? = null
    public val passengerIds: MutableSet<Int> = linkedSetOf()

    public var customName: String? = null

    /** Set when it has been killed or removed, so the sync pass tells clients to forget it. */
    public var removed: Boolean = false

    /**
     * Whether it was killed rather than simply forgotten.
     *
     * parity: `Entity.die` against `Entity.discard`, which is the difference between a cow that
     * drops beef and one that despawned because nobody was near it.
     */
    public var killed: Boolean = false

    /** Modifiers by attribute then by id. parity: `AttributeInstance`'s three operations. */
    public val attributeModifiers: MutableMap<String, MutableMap<String, AttributeModifier>> = mutableMapOf()

    /**
     * The base value of one attribute: what a command set, else what this kind of entity is born
     * with, else the attribute's own default.
     *
     * parity: `AttributeMap`, which is built from `DefaultAttributes.getSupplier(type)`: a cow walks
     * at 0.2 and a zombie at 0.23 because their own suppliers say so, not because of any global.
     */
    public fun attribute(id: String): Double =
        attributeValues[id]
            ?: EntityAttributes.baseValue(type, id)
            ?: Attribute.defaultFor(id)
            ?: 0.0

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

    /** parity: `EntityType`'s own box, which is what the world collides with. */
    public val width: Double get() = EntityTypeShapes.widthOf(type)
    public val height: Double get() = EntityTypeShapes.heightOf(type)

    public companion object {
        public const val DEFAULT_MAX_HEALTH: Double = 20.0

        /** parity: `PrimedTnt.NO_FUSE`. */
        public const val NO_FUSE: Int = -1
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
