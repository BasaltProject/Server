package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.runtime.entity.DamageSource
import org.basaltmc.server.runtime.entity.EntityCombat
import org.basaltmc.server.runtime.entity.Projectiles
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.displayName
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.registry.VanillaRegistryData
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.ProjectileHits
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.DamageEventPacket
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.SetEntityMotionPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.text.Component
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.sqrt
import kotlin.random.Random

/**
 * Everything a projectile does in flight and on landing: what it hits, what that hurts, and the
 * flash and sound an entity being hit owes whoever is watching.
 */
public class CombatSystem(
    private val pluginEvents: PluginEventBus,
    private val broadcaster: BlockChangeBroadcaster,
    private val explosions: ExplosionSystem,
) {
    /**
     * One tick of something in flight, and whatever it hits on the way.
     *
     * parity: `Projectile.hitTargetOrDeflectSelf`: what sticks stays where it landed, what does not
     * is gone the moment it lands. What the block itself does about it is `onProjectileHit`.
     */
    public fun tickProjectile(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        living: List<ServerEntity>,
        entity: ServerEntity,
        kind: Projectiles.Kind,
    ) {
        val fromX = entity.x
        val fromY = entity.y
        val fromZ = entity.z
        val hit = Projectiles.tick(world.state, entity, kind)
        // parity: `AbstractHurtingProjectile.onHit`: a fireball goes off where it lands, whether
        // that is a block or somebody's face.
        if (kind == Projectiles.FIREBALL || kind == Projectiles.SMALL_FIREBALL) {
            val target = targetAlong(watchers, living, entity, fromX, fromY, fromZ)
            if (hit == null && target == null) return
            entity.removed = true
            if (kind == Projectiles.FIREBALL) {
                explosions.blowUp(
                    world,
                    watchers,
                    entity.x,
                    entity.y,
                    entity.z,
                    entity.explosionPower,
                    "tnt",
                    entity.entityId,
                )
                return
            }
            // parity: a small fireball burns whoever it hits for five seconds and takes five off.
            val (mob, player) = target ?: return
            if (mob != null) {
                mob.fireTicks = SMALL_FIREBALL_FIRE_TICKS
                val result =
                    EntityCombat.hurt(
                        mob,
                        SMALL_FIREBALL_DAMAGE,
                        fromX,
                        fromZ,
                        events = pluginEvents,
                        worldName = world.name,
                        cause = "fireball",
                    )
                if (result.landed) announceEntityHit(watchers, mob, DamageTypes.ON_FIRE, entity.entityId, result.died)
            }
            player?.connection?.hurt(
                SMALL_FIREBALL_DAMAGE,
                DEATH_ON_FIRE,
                DamageTypes.ON_FIRE,
                DamageSource(entity.entityId, fromX, fromZ, shooterName(watchers, living, entity)),
            )
            return
        }
        if (kind == Projectiles.ENDER_PEARL) {
            val target = targetAlong(watchers, living, entity, fromX, fromY, fromZ)
            if (hit != null || target != null) landEnderPearl(world, watchers, entity)
            return
        }
        if (kind == Projectiles.SPLASH_POTION) {
            val target = targetAlong(watchers, living, entity, fromX, fromY, fromZ)
            if (hit == null && target == null) return
            entity.removed = true
            splashPotion(watchers, entity)
            return
        }
        if (kind.sticks && hitEntityAlong(watchers, living, entity, fromX, fromY, fromZ)) return
        if (hit == null) return
        ProjectileHits.onHit(world, hit, byArrow = kind.sticks)?.let { change ->
            broadcaster.announceRedstone(world, watchers, change)
        }
        if (!kind.sticks) entity.removed = true
    }

    /**
     * What a bottle leaves where it broke.
     *
     * parity: `ThrownSplashPotion.applySplash`: four blocks of reach, and the further away you are
     * the less of the effect you get, down to nothing at the edge.
     */
    private fun splashPotion(
        watchers: List<PlayerSession>,
        bottle: ServerEntity,
    ) {
        val effect = bottle.potionEffect ?: return
        for (player in watchers) {
            val dx = player.x - bottle.x
            val dy = player.y - bottle.y
            val dz = player.z - bottle.z
            val distanceSquared = dx * dx + dy * dy + dz * dz
            if (distanceSquared > SPLASH_RANGE * SPLASH_RANGE) continue
            val share = 1.0 - sqrt(distanceSquared) / SPLASH_RANGE
            val ticks = (SPLASH_DURATION * share).toInt()
            if (ticks <= 0) continue
            player.connection.addEffect(Identifier.vanilla(effect), ticks, 0)
        }
        broadcaster.sendEffects(
            watchers,
            floor(bottle.x).toInt(),
            floor(bottle.y).toInt(),
            floor(bottle.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_SPLASH, SoundSource.NEUTRAL, 1f, 1f)),
        )
    }

    /**
     * Where a thrown pearl puts whoever threw it.
     *
     * parity: `ThrownEnderpearl.onHit`: the owner lands where the pearl stopped, takes five points
     * of fall damage, and both ends of the trip make the teleport sound.
     */
    private fun landEnderPearl(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        pearl: ServerEntity,
    ) {
        pearl.removed = true
        val owner = watchers.firstOrNull { it.entityId == pearl.ownerId } ?: return
        if (owner.dead) return
        broadcaster.sendEffects(
            watchers,
            floor(pearl.x).toInt(),
            floor(pearl.y).toInt(),
            floor(pearl.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_TELEPORT, SoundSource.PLAYERS, 1f, 1f)),
        )
        owner.connection.teleportTo(pearl.x, pearl.y, pearl.z, owner.yaw, owner.pitch)
        owner.connection.hurt(ENDER_PEARL_DAMAGE, DEATH_FALL, DamageTypes.ENDER_PEARL)
    }

    /** The nearest thing a projectile passed through this tick, as a mob or as a player. */
    private fun targetAlong(
        watchers: List<PlayerSession>,
        living: List<ServerEntity>,
        projectile: ServerEntity,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
    ): Pair<ServerEntity?, PlayerSession?>? {
        var nearest = Double.MAX_VALUE
        var mob: ServerEntity? = null
        var player: PlayerSession? = null
        for (candidate in living) {
            if (candidate.entityId == projectile.entityId || candidate.entityId == projectile.ownerId) continue
            if (!EntityAttributes.isLiving(candidate.type)) continue
            val at =
                clip(
                    projectile,
                    fromX,
                    fromY,
                    fromZ,
                    candidate.x,
                    candidate.y,
                    candidate.z,
                    candidate.width,
                    candidate.height,
                )
            if (at == null || at >= nearest) continue
            nearest = at
            mob = candidate
            player = null
        }
        for (candidate in watchers) {
            if (candidate.entityId == projectile.ownerId || candidate.dead) continue
            if (candidate.gamemode == Gamemode.CREATIVE || candidate.gamemode == Gamemode.SPECTATOR) continue
            val at =
                clip(
                    projectile,
                    fromX,
                    fromY,
                    fromZ,
                    candidate.x,
                    candidate.y,
                    candidate.z,
                    EntityTypeShapes.widthOf(PLAYER_TYPE),
                    EntityTypeShapes.heightOf(PLAYER_TYPE),
                )
            if (at == null || at >= nearest) continue
            nearest = at
            mob = null
            player = candidate
        }
        if (mob == null && player == null) return null
        return mob to player
    }

    /**
     * Whatever the arrow passed through on its way, hurt.
     *
     * parity: `AbstractArrow.onHitEntity`: the damage is the speed times the arrow's own two
     * points, rounded up, and a fully drawn bow adds a roll on top of it.
     */
    private fun hitEntityAlong(
        watchers: List<PlayerSession>,
        living: List<ServerEntity>,
        arrow: ServerEntity,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
    ): Boolean {
        val (mob, player) = targetAlong(watchers, living, arrow, fromX, fromY, fromZ) ?: return false
        val speed =
            sqrt(
                arrow.velocityX * arrow.velocityX +
                    arrow.velocityY * arrow.velocityY +
                    arrow.velocityZ * arrow.velocityZ,
            )
        var damage = ceil(speed * ARROW_BASE_DAMAGE).toInt()
        if (arrow.critical) damage += Random.nextInt(damage / 2 + 2)

        if (mob != null) {
            val result =
                EntityCombat.hurt(
                    mob,
                    damage.toFloat(),
                    fromX,
                    fromZ,
                    events = pluginEvents,
                    worldName = watchers.firstOrNull()?.worldName,
                    cause = "projectile",
                )
            if (!result.landed) {
                // parity: an arrow that found somebody still inside their hurt cooldown bounces off.
                arrow.velocityX *= ARROW_BOUNCE
                arrow.velocityY *= ARROW_BOUNCE
                arrow.velocityZ *= ARROW_BOUNCE
                return true
            }
            arrow.removed = true
            arrow.ownerId?.let { mob.hurtBy = it }
            mob.panicTicks = PANIC_TICKS
            announceEntityHit(watchers, mob, DamageTypes.ARROW, arrow.entityId, result.died)
            return true
        }
        arrow.removed = true
        player?.connection?.hurt(
            damage.toFloat(),
            DEATH_ARROW,
            DamageTypes.ARROW,
            DamageSource(arrow.entityId, fromX, fromZ, shooterName(watchers, living, arrow)),
        )
        return true
    }

    // parity: Projectile.getOwner, since an arrow kills for whoever shot it. The arrow itself is the
    // answer once its owner is gone.
    private fun shooterName(
        watchers: List<PlayerSession>,
        living: List<ServerEntity>,
        projectile: ServerEntity,
    ): Component {
        val ownerId = projectile.ownerId ?: return projectile.displayName()
        watchers.firstOrNull { it.entityId == ownerId }?.let { return Component.text(it.name) }
        return living.firstOrNull { it.entityId == ownerId }?.displayName() ?: projectile.displayName()
    }

    /** Where along its flight the arrow met this box, or null when it did not. */
    private fun clip(
        arrow: ServerEntity,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
        x: Double,
        y: Double,
        z: Double,
        width: Double,
        height: Double,
    ): Double? =
        Projectiles.clipBox(
            fromX,
            fromY,
            fromZ,
            arrow.x,
            arrow.y,
            arrow.z,
            x - width / 2.0,
            y,
            z - width / 2.0,
            x + width / 2.0,
            y + height,
            z + width / 2.0,
        )

    /** The flash, the push and the sound of an entity being hit by something that is not a player. */
    public fun announceEntityHit(
        watchers: List<PlayerSession>,
        entity: ServerEntity,
        damageType: Identifier,
        causeEntityId: Int,
        died: Boolean,
    ) {
        val registryId = VanillaRegistryData.damageTypes.indexOfFirst { it.first == damageType.path }
        val sound = if (died) EntityCombat.deathSound(entity.type) else EntityCombat.hurtSound(entity.type)
        for (watcher in watchers) {
            if (registryId >= 0) {
                watcher.connection.sendPlayPacket(DamageEventPacket.ID) {
                    DamageEventPacket(entity.entityId, registryId, causeEntityId, causeEntityId).write(it)
                }
            }
            watcher.connection.sendPlayPacket(SetEntityMotionPacket.ID) {
                SetEntityMotionPacket(entity.entityId, entity.velocityX, entity.velocityY, entity.velocityZ).write(it)
            }
            watcher.connection.sendPlayPacket(SoundPacket.ID) {
                SoundPacket(
                    sound,
                    SoundSource.HOSTILE,
                    entity.x,
                    entity.y,
                    entity.z,
                    1f,
                    1f,
                    Random.nextLong(),
                ).write(it)
            }
            if (died) {
                watcher.connection.sendPlayPacket(EntityEventPacket.ID) {
                    EntityEventPacket(entity.entityId, EntityEventPacket.LIVING_DEATH.toByte()).write(it)
                }
            }
        }
        if (!died) return
        entity.killed = true
        entity.removed = true
    }

    private companion object {
        /** parity: `SmallFireball.onHitEntity`, five points and five seconds of burning. */
        const val SMALL_FIREBALL_DAMAGE = 5f
        const val SMALL_FIREBALL_FIRE_TICKS = 100
        const val DEATH_ON_FIRE = "onFire"

        /** parity: `applySplash`, four blocks across and twenty seconds at the centre. */
        const val SPLASH_RANGE = 4.0
        const val SPLASH_DURATION = 400.0
        const val SOUND_SPLASH = "entity.splash_potion.break"

        const val SOUND_TELEPORT = "entity.player.teleport"
        const val DEATH_FALL = "fall"

        /** parity: `ThrownEnderpearl.onHit`, five points of fall damage on arrival. */
        const val ENDER_PEARL_DAMAGE = 5f

        /** parity: `EntityTypes.PLAYER`, whose box is what a mob's reach is measured against. */
        val PLAYER_TYPE: Identifier = Identifier.vanilla("player")

        const val ARROW_BASE_DAMAGE = 2.0

        /** parity: `AbstractArrow.onHitEntity`'s own `scale(0.2)` when the hit did not land. */
        const val ARROW_BOUNCE = 0.2

        /** parity: `PanicGoal`, which runs for as long as the last hit is remembered. */
        const val PANIC_TICKS = 40

        /** parity: `death.attack.arrow`, and `AbstractArrow`'s own two points of damage. */
        const val DEATH_ARROW = "arrow"
    }
}
