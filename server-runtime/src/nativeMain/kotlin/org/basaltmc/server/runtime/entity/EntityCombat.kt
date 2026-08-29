package org.basaltmc.server.runtime.entity

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.runtime.player.Combat
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * What a hit does to something that is not a player.
 *
 * parity: `LivingEntity.hurtServer` for the half that a mob has: the half second of invulnerability
 * after a hit, the bigger-hit-only rule inside it, armour off the mob's own attribute, and the push
 * the hit gives.
 */
public object EntityCombat {
    /** parity: `LivingEntity.invulnerableDuration`, ten ticks. */
    public const val HURT_COOLDOWN_TICKS: Int = 20

    /** parity: the half of that window where only a bigger hit gets through. */
    private const val COOLDOWN_HALF_TICKS = HURT_COOLDOWN_TICKS / 2

    /** parity: `LivingEntity.dealDefaultKnockback`, which every hit that lands is worth. */
    public const val DEFAULT_KNOCKBACK: Double = 0.4

    /** What a hit left behind: whether it landed, and whether it killed. */
    public class Result(
        public val landed: Boolean,
        public val died: Boolean,
        public val damage: Float,
    ) {
        public companion object {
            public val MISSED: Result = Result(landed = false, died = false, damage = 0f)
        }
    }

    /**
     * Hurts [entity] for [amount], pushed away from [sourceX]/[sourceZ], answering what happened.
     *
     * parity: `LivingEntity.hurtServer`. The cooldown is the part worth naming: inside it only a
     * bigger hit lands, and only by the difference, which is what stops a fast weapon from being
     * strictly better than a strong one.
     */
    public fun hurt(
        entity: ServerEntity,
        amount: Float,
        sourceX: Double = entity.x,
        sourceZ: Double = entity.z,
        /** Where plugin events go, and what to tell them about this hit. Null means no plugins. */
        events: PluginEventBus? = null,
        worldName: String? = null,
        cause: String = "attack",
        attackerName: String? = null,
    ): Result {
        if (entity.removed || entity.health <= 0.0) return Result.MISSED
        // Every hit a mob takes comes through here, so this is the one place that has to ask. A
        // plugin may stop it or change how much it is worth; what it cannot do is see a hit that
        // was never going to land, which is why this is below the checks above.
        var amount = amount
        if (events != null && events.listening(PluginEventTypes.ENTITY_DAMAGE)) {
            val event = events.event(PluginEventTypes.ENTITY_DAMAGE)
            event.set(0, entity.entityId)
            event.set(1, entity.type.toString())
            event.set(2, worldName)
            event.set(3, cause)
            event.set(4, amount.toDouble())
            event.set(5, attackerName)
            if (!events.fire(event)) return Result.MISSED
            amount = ((event.get("amount") as? Double) ?: amount.toDouble()).toFloat()
            if (amount <= 0f) return Result.MISSED
        }
        var incoming = amount
        val cooling = entity.hurtCooldown > COOLDOWN_HALF_TICKS
        if (cooling) {
            if (amount <= entity.lastHurt) return Result.MISSED
            incoming = amount - entity.lastHurt
            entity.lastHurt = amount
        } else {
            entity.lastHurt = amount
            entity.hurtCooldown = HURT_COOLDOWN_TICKS
        }
        val reduced = afterArmour(entity, incoming)
        if (reduced <= 0f) return Result.MISSED
        entity.health -= reduced.toDouble()
        val died = entity.health <= 0.0
        if (died) entity.health = 0.0
        // parity: the 0.4 every hit is worth, taken before the attacker's own extra knockback.
        if (!cooling) knockback(entity, DEFAULT_KNOCKBACK, sourceX - entity.x, sourceZ - entity.z)
        if (died && events != null && events.listening(PluginEventTypes.ENTITY_DEATH)) {
            val event = events.event(PluginEventTypes.ENTITY_DEATH)
            event.set(0, entity.entityId)
            event.set(1, entity.type.toString())
            event.set(2, worldName)
            event.set(3, entity.x)
            event.set(4, entity.y)
            event.set(5, entity.z)
            event.set(6, attackerName)
            events.fire(event)
        }
        return Result(landed = true, died = died, damage = reduced)
    }

    /** parity: a mob's own `armor` and `armor_toughness` attributes, which most of them leave at zero. */
    private fun afterArmour(
        entity: ServerEntity,
        amount: Float,
    ): Float {
        val armour = attribute(entity.type, EntityAttributes.ARMOR)
        if (armour <= 0f) return amount
        return Combat.afterArmour(amount, armour, attribute(entity.type, EntityAttributes.ARMOR_TOUGHNESS))
    }

    private fun attribute(
        type: Identifier,
        name: String,
    ): Float = EntityAttributes.baseValue(type, name)?.toFloat() ?: 0f

    /**
     * parity: `LivingEntity.knockback`. [towardsX]/[towardsZ] point at whoever swung, and the push
     * is taken off the speed, so the entity ends up going the other way.
     */
    public fun knockback(
        entity: ServerEntity,
        power: Double,
        towardsX: Double,
        towardsZ: Double,
    ) {
        val moved =
            Combat.knockback(
                power = power,
                towardsX = towardsX,
                towardsZ = towardsZ,
                velocityX = entity.velocityX,
                velocityY = entity.velocityY,
                velocityZ = entity.velocityZ,
                onGround = entity.onGround,
                knockbackResistance = attribute(entity.type, EntityAttributes.KNOCKBACK_RESISTANCE).toDouble(),
            )
        entity.velocityX = moved[0]
        entity.velocityY = moved[1]
        entity.velocityZ = moved[2]
    }

    /** What this kind of mob hits for. parity: `Attributes.ATTACK_DAMAGE`, zero for anything harmless. */
    public fun attackDamageOf(entity: ServerEntity): Float =
        entity.attributeWithModifiers(Attribute.ATTACK_DAMAGE).toFloat()

    /**
     * The sound a mob makes when hit, and when it dies.
     *
     * parity: `getHurtSound`/`getDeathSound`, which nearly every mob answers with its own name.
     * The handful that answer something else (an armour stand breaking, a wolf in armour) are not
     * worth a table until those mobs exist.
     */
    public fun hurtSound(type: Identifier): String = "entity.${type.path}.hurt"

    public fun deathSound(type: Identifier): String = "entity.${type.path}.death"

    /** Counts the invulnerability window down, once per tick. */
    public fun tick(entity: ServerEntity) {
        if (entity.hurtCooldown > 0) entity.hurtCooldown--
        if (entity.attackCooldown > 0) entity.attackCooldown--
    }
}

/** Who dealt a hit and where they stood, which is what decides the way it pushes. */
public class DamageSource(
    public val entityId: Int,
    public val x: Double,
    public val z: Double,
    /** Travels with the hit because only where it comes from knows it: below there is just an id. */
    public val name: Component? = null,
)

/** parity: `Entity.getDisplayName`, which is the name somebody gave it or the name of its kind. */
public fun ServerEntity.displayName(): Component =
    customName?.let { Component.text(it) } ?: Component.translatable("entity.minecraft.${type.path}")
