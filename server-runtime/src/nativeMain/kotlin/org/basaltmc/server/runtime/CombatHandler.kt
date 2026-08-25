package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.ItemArmour
import org.basaltmc.server.registry.ItemWeapons
import org.basaltmc.server.registry.ParticleTypes
import org.basaltmc.server.runtime.entity.Breeding
import org.basaltmc.server.runtime.entity.DamageSource
import org.basaltmc.server.runtime.entity.EntityCombat
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.Taming
import org.basaltmc.server.runtime.entity.Villagers
import org.basaltmc.server.runtime.entity.Wardens
import org.basaltmc.server.runtime.entity.ai.Navigation
import org.basaltmc.server.runtime.player.Combat
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.registry.VanillaRegistryData
import org.basaltmc.server.runtime.world.DamageProtection
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.DetectorRails
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.server.runtime.world.RealDamageTypeTags
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.WeaponDamage
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredStats
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.AttackPacket
import org.basaltmc.wire.protocol.play.DamageEventPacket
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.HurtAnimationPacket
import org.basaltmc.wire.protocol.play.InteractPacket
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.PlayerCombatKillPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEntityMotionPacket
import org.basaltmc.wire.protocol.play.SetPassengersPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.resolvePlainText
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random

/**
 * Melee combat, mounting, taming, fall damage and the player-facing half of damage: everything
 * downstream of a hit or a fall. [ClientSession.hurt], [ClientSession.heal] and
 * [ClientSession.killPlayer] stay as thin delegates onto this class, since their callers (fire,
 * lava, drowning, commands, other systems) are too wide to move wholesale.
 */
internal class CombatHandler(
    private val owner: ClientSession,
) {
    /**
     * parity: `Entity.fallDistance`, added up from the movement rather than asked of the client.
     *
     * Read and written by [trackFall] and [handleAttack]'s critical-hit check, and cleared by
     * [ClientSession.teleportTo].
     */
    internal var fallDistance = 0.0

    /** Tells everyone who can see it that something happened to an entity. */
    private fun broadcastEntityEvent(
        entityId: Int,
        event: Int,
    ) {
        val packet = EntityEventPacket(entityId, event.toByte())
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(EntityEventPacket.ID) { packet.write(it) }
        }
    }

    /** The red flash of a hit on something that is not a player, and who is blamed for it. */
    private fun broadcastDamageEvent(
        entity: ServerEntity,
        damageType: Identifier,
        causeEntityId: Int,
    ) {
        val registryId = VanillaRegistryData.damageTypes.indexOfFirst { it.first == damageType.path }
        if (registryId < 0) return
        val packet =
            DamageEventPacket(
                entityId = entity.entityId,
                damageType = registryId,
                causeEntityId = causeEntityId,
                directEntityId = causeEntityId,
            )
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        }
    }

    /** How fast an entity is going now, which is the only way a knocked-back mob moves on a client. */
    private fun broadcastMotion(entity: ServerEntity) {
        val packet =
            SetEntityMotionPacket(entity.entityId, entity.velocityX, entity.velocityY, entity.velocityZ)
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(SetEntityMotionPacket.ID) { packet.write(it) }
        }
    }

    /**
     * What a killed entity leaves behind.
     *
     * parity: `LivingEntity.die`. The dying animation is the client's own half, off the entity
     * event; the loot is rolled by the tick loop, which is what [ServerEntity.killed] is for.
     */
    private fun killEntity(entity: ServerEntity) {
        owner.broadcastSound(EntityCombat.deathSound(entity.type), entity.x, entity.y, entity.z, SoundSource.HOSTILE)
        broadcastEntityEvent(entity.entityId, EntityEventPacket.LIVING_DEATH)
        entity.killed = true
        entity.removed = true
    }

    /**
     * Hitting a minecart.
     *
     * parity: `VehicleEntity.hurtServer` into `destroy`: the cart drops itself as an item, and a
     * creative player's hit discards it without dropping anything at all.
     */
    private fun breakVehicle(
        session: PlayerSession,
        vehicle: ServerEntity,
    ) {
        broadcastDamageEvent(vehicle, DamageTypes.PLAYER_ATTACK, session.entityId)
        if (session.gamemode != Gamemode.CREATIVE) {
            vehicle.vehicleDamage += ClientSession.VEHICLE_DAMAGE_PER_POINT
            if (vehicle.vehicleDamage <= ClientSession.VEHICLE_BREAK_DAMAGE) return
            owner.blockInteraction.spawnDroppedItem(vehicle.type, 1, vehicle.x, vehicle.y, vehicle.z)
        }
        for (passenger in vehicle.passengerIds.toList()) {
            owner.playersInSameWorld().firstOrNull { it.entityId == passenger }?.let { it.connection.dismount(it) }
        }
        vehicle.removed = true
    }

    /**
     * Taming an animal, and telling one that is already yours to sit.
     *
     * parity: `Wolf.mobInteract` and `Cat.mobInteract`: the food goes whether it takes or not, a
     * wolf takes one bone in three, and a click with anything else on an animal that is already
     * yours sits it down or stands it up.
     */
    private fun tameOrSit(
        session: PlayerSession,
        entity: ServerEntity,
    ): Boolean {
        if (!Taming.isTamable(entity.type)) return false
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val item = held?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
        val owner = entity.owner
        if (owner != null) {
            if (owner != session.uuid.toString()) return true
            entity.sitting = !entity.sitting
            Navigation.stop(entity)
            broadcastTameFlags(entity)
            return true
        }
        if (item == null || !Taming.tamedBy(entity.type, item)) return false
        // Not through playerEvent: this is an entity event that happens to have somebody doing it,
        // so the entity is first and the player comes after the world.
        val taming =
            this.owner.pluginEvents.emit(PluginEventTypes.ENTITY_TAME) { event ->
                event.set(0, entity.entityId)
                event.set(1, entity.type.toString())
                event.set(2, this.owner.currentWorld?.name)
                event.set(3, session.name)
                event.set(4, session.uuid.toString())
            }
        if (taming?.cancelled == true) {
            taming.cancelMessage?.let { this.owner.sendPluginNotice(it) }
            return true
        }
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            this.owner.sendInventorySlot(heldSlot)
        }
        if (Random.nextInt(Taming.TAME_ODDS) != 0) {
            broadcastEntityEvent(entity.entityId, Taming.EVENT_TAMING_FAILED)
            return true
        }
        entity.owner = session.uuid.toString()
        entity.sitting = true
        entity.targetId = null
        Navigation.stop(entity)
        broadcastEntityEvent(entity.entityId, Taming.EVENT_TAMING_SUCCEEDED)
        broadcastTameFlags(entity)
        return true
    }

    /** parity: `TamableAnimal`'s own flags byte, which is what draws a sitting pet. */
    private fun broadcastTameFlags(entity: ServerEntity) {
        val packet =
            SetEntityDataPacket(
                entity.entityId,
                listOf(SetEntityDataPacket.Entry.Bytes(Taming.FLAGS_INDEX, Taming.flagsOf(entity).toByte())),
            )
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    /** parity: `Entity.startRiding`, which is the passenger list and the packet that carries it. */
    private fun mount(
        session: PlayerSession,
        vehicle: ServerEntity,
    ) {
        if (session.vehicleId != null) return
        val mounting =
            owner.pluginEvents.emit(PluginEventTypes.ENTITY_MOUNT) { event ->
                event.set(0, vehicle.entityId)
                event.set(1, vehicle.type.toString())
                event.set(2, owner.currentWorld?.name)
                event.set(3, session.name)
                event.set(4, session.uuid.toString())
                event.set(5, true)
            }
        if (mounting?.cancelled == true) return
        session.vehicleId = vehicle.entityId
        vehicle.passengerIds += session.entityId
        broadcastPassengers(vehicle)
    }

    /** parity: `Entity.stopRiding`: off the cart, standing where it left them. */
    internal fun dismount(session: PlayerSession) {
        val world = owner.currentWorld ?: return
        val vehicle = session.vehicleId?.let { world.state.entity(it) }
        session.vehicleId = null
        if (vehicle == null) return
        vehicle.passengerIds -= session.entityId
        broadcastPassengers(vehicle)
        // The client puts itself back on the ground, but the server's own copy of where they are
        // came from the cart, so it is told the same thing rather than left a tick behind.
        owner.teleportTo(session.x, session.y + ClientSession.DISMOUNT_LIFT, session.z, session.yaw, session.pitch)
    }

    private fun broadcastPassengers(vehicle: ServerEntity) {
        val packet = SetPassengersPacket(vehicle.entityId, vehicle.passengerIds.toList())
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(SetPassengersPacket.ID) { packet.write(it) }
        }
    }

    /**
     * A player hitting something.
     *
     * parity: `Player.attack`. The damage is the attribute plus whatever is in hand, scaled by how
     * far the swing has recharged (a fifth of it at worst), half again for a critical, and a
     * sprinting hit pushes half as much again.
     */
    internal fun handleAttack(attack: AttackPacket) {
        val session = owner.playerSession ?: return
        if (session.dead || session.gamemode == Gamemode.SPECTATOR) return
        val world = owner.currentWorld ?: return
        val target = world.state.entity(attack.entityId) ?: return
        if (target.removed) return
        if (!owner.withinAttackReach(session, target)) return
        // parity: `VehicleEntity.hurtServer`, which is not health at all: a cart takes ten a point
        // and comes apart past forty, or at once for a creative player.
        if (DetectorRails.isMinecart(target.type)) {
            breakVehicle(session, target)
            return
        }
        if (!EntityAttributes.isLiving(target.type)) return

        val weapon = session.inventory.heldItem?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
        val stats = ItemWeapons.of(weapon)
        val strength =
            Combat.attackStrengthScale(
                session.ticksSinceSwing,
                ItemWeapons.BASE_ATTACK_SPEED + (stats?.attackSpeed ?: 0.0),
            )
        session.ticksSinceSwing = 0

        val base = (ItemWeapons.BASE_ATTACK_DAMAGE + (stats?.attackDamage ?: 0.0)).toFloat()
        // parity: what the enchantments add rides on the swing but not on the critical, and is put
        // back on top after the charge and the critical have had the base to themselves.
        val enchanted = WeaponDamage.of(session.inventory.heldItem, base) { owner.enchantments?.definitionOf(it) }
        val boost = strength * (enchanted - base)
        var damage = base * Combat.chargeScale(strength)
        val fullStrength = strength > ClientSession.FULL_STRENGTH
        // parity: `canCriticalAttack`, which is the falling half of a jump and nothing else.
        val critical = fullStrength && fallDistance > 0.0 && !session.onGround && !session.sprinting
        if (critical) damage *= Combat.CRITICAL_MULTIPLIER
        damage += boost
        if (damage <= 0f) return

        val knockbackAttack = session.sprinting && fullStrength
        if (knockbackAttack) {
            owner.broadcastSound(
                ClientSession.SOUND_ATTACK_KNOCKBACK,
                session.x,
                session.y,
                session.z,
                SoundSource.PLAYERS,
            )
        }
        val result =
            EntityCombat.hurt(
                target,
                damage,
                session.x,
                session.z,
                events = owner.pluginEvents,
                worldName = owner.currentWorld?.name,
                cause = "attack",
                attackerName = session.name,
            )
        if (!result.landed) {
            owner.broadcastSound(
                ClientSession.SOUND_ATTACK_NO_DAMAGE,
                session.x,
                session.y,
                session.z,
                SoundSource.PLAYERS,
            )
            return
        }
        // parity: `causeExtraKnockback`, which is the half a sprinting hit adds and nothing else
        // without enchantments: the 0.4 every hit is worth was already taken inside the hit.
        if (knockbackAttack) {
            EntityCombat.knockback(target, ClientSession.SPRINT_KNOCKBACK, session.x - target.x, session.z - target.z)
        }
        // parity: `isSweepAttack`: a sword, on the ground, walking, and a swing that is neither a
        // critical nor a sprinting one.
        val sweep =
            fullStrength &&
                !critical &&
                !knockbackAttack &&
                session.onGround &&
                owner.lastStepSquared < ClientSession.SWEEP_SPEED_LIMIT * ClientSession.SWEEP_SPEED_LIMIT &&
                weapon?.path?.endsWith(ClientSession.SWORD_SUFFIX) == true
        if (sweep) sweepAround(session, target, world, strength)
        broadcastDamageEvent(target, DamageTypes.PLAYER_ATTACK, session.entityId)
        broadcastMotion(target)
        // parity: `HurtByTargetGoal` and `PanicGoal` both read the last thing that hurt it: one
        // sends a monster after whoever swung, the other sends an animal running.
        target.hurtBy = session.entityId
        // parity: `Warden.hurtServer`, which adds the angry threshold plus twenty to whoever hit it.
        if (Wardens.isWarden(target.type)) Wardens.anger(target, session.entityId, Wardens.HURT)
        target.panicTicks = ClientSession.PANIC_TICKS
        // parity: `OwnerHurtTargetGoal`, which sends a pet after whatever its owner just hit.
        for (pet in world.state.entities()) {
            if (pet.owner == session.uuid.toString()) pet.ownerHurt = target.entityId
        }
        if (critical) {
            owner.broadcastSound(ClientSession.SOUND_ATTACK_CRIT, session.x, session.y, session.z, SoundSource.PLAYERS)
            // parity: `ServerPlayer.crit`, which is an animation on the target rather than particles.
            broadcastAnimation(target.entityId, AnimatePacket.CRITICAL_HIT)
        }
        if (!critical && !sweep) {
            val sound = if (fullStrength) ClientSession.SOUND_ATTACK_STRONG else ClientSession.SOUND_ATTACK_WEAK
            owner.broadcastSound(sound, session.x, session.y, session.z, SoundSource.PLAYERS)
        }
        session.stats.addDamageDealt(result.damage)
        session.food.addExhaustion(ClientSession.ATTACK_EXHAUSTION)
        if (result.died) {
            killEntity(target)
        } else {
            owner.broadcastSound(
                EntityCombat.hurtSound(target.type),
                target.x,
                target.y,
                target.z,
                SoundSource.HOSTILE,
            )
        }
    }

    /**
     * The rest of the swing, for everything else standing around the target.
     *
     * parity: `Player.doSweepAttack`: a point of damage each, the same push, and only what is
     * within a block of the target and three of the player.
     */
    private fun sweepAround(
        session: PlayerSession,
        target: ServerEntity,
        world: LoadedWorld,
        strength: Float,
    ) {
        owner.broadcastSound(ClientSession.SOUND_ATTACK_SWEEP, session.x, session.y, session.z, SoundSource.PLAYERS)
        val damage = ClientSession.SWEEP_DAMAGE * strength
        for (nearby in world.state.entities()) {
            if (nearby.entityId == target.entityId || nearby.removed) continue
            if (!EntityAttributes.isLiving(nearby.type)) continue
            if (abs(nearby.x - target.x) > ClientSession.SWEEP_RANGE + nearby.width / 2.0) continue
            if (abs(nearby.z - target.z) > ClientSession.SWEEP_RANGE + nearby.width / 2.0) continue
            if (nearby.y > target.y + target.height + ClientSession.SWEEP_HEIGHT) continue
            if (nearby.y + nearby.height < target.y - ClientSession.SWEEP_HEIGHT) continue
            val dx = nearby.x - session.x
            val dz = nearby.z - session.z
            if (dx * dx + dz * dz >= ClientSession.SWEEP_PLAYER_RANGE) continue
            val result =
                EntityCombat.hurt(
                    nearby,
                    damage,
                    session.x,
                    session.z,
                    events = owner.pluginEvents,
                    worldName = owner.currentWorld?.name,
                    cause = "sweep",
                    attackerName = session.name,
                )
            if (!result.landed) continue
            broadcastDamageEvent(nearby, DamageTypes.PLAYER_ATTACK, session.entityId)
            broadcastMotion(nearby)
            if (result.died) {
                killEntity(nearby)
            } else {
                nearby.hurtBy = session.entityId
                if (Wardens.isWarden(nearby.type)) Wardens.anger(nearby, session.entityId, Wardens.HURT)
                nearby.panicTicks = ClientSession.PANIC_TICKS
            }
        }
        // parity: the sweep's own particle, a block ahead of where the player is looking.
        val yaw = session.yaw * PI.toFloat() / ClientSession.HALF_CIRCLE
        broadcastParticle(
            ClientSession.PARTICLE_SWEEP,
            session.x - sin(yaw).toDouble(),
            session.y + ClientSession.SWEEP_PARTICLE_HEIGHT,
            session.z + cos(yaw).toDouble(),
            count = 0,
        )
    }

    /** One animation on an entity, for everyone in the world. */
    private fun broadcastAnimation(
        entityId: Int,
        animation: Int,
    ) {
        val packet = AnimatePacket(entityId, animation)
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(AnimatePacket.ID) { packet.write(it) }
        }
    }

    /** One particle effect where it happened, for everyone in the world. */
    private fun broadcastParticle(
        particle: String,
        x: Double,
        y: Double,
        z: Double,
        count: Int,
    ) {
        val id = ParticleTypes[Identifier.vanilla(particle)]?.protocolId ?: return
        val packet = LevelParticlesPacket(particleTypeId = id, x = x, y = y, z = z, count = count)
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(LevelParticlesPacket.ID) { packet.write(it) }
        }
    }

    /**
     * Right-clicking an entity.
     *
     * parity: `Animal.mobInteract`, the half of it this build has: the right food puts an animal in
     * love, and a hand that is not holding it does nothing. What a villager or a boat does with the
     * same click is their own business and waits for them.
     */
    internal fun handleInteract(interact: InteractPacket) {
        val session = owner.playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        // An NPC is not in the world's entity table, so it is answered before anything looks there.
        // Only the main hand: an empty-handed right click sends one packet per hand, so answering
        // both is how one click becomes two.
        if (interact.hand == InteractPacket.MAIN_HAND &&
            owner.npcs?.clicked(interact.entityId, session.uuid.toString()) == true
        ) {
            return
        }
        if (owner.npcs?.of(interact.entityId) != null) return
        val world = owner.currentWorld ?: return
        val entity = world.state.entity(interact.entityId) ?: return
        if (entity.removed) return
        val interacted =
            owner.playerEvent(PluginEventTypes.PLAYER_INTERACT_ENTITY) { event ->
                event.set(3, entity.entityId)
                event.set(4, entity.type.toString())
                event.set(5, "interact")
                event.set(6, if (interact.hand == 1) "off" else "main")
            }
        if (interacted?.cancelled == true) {
            interacted.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        // parity: `Villager.mobInteract` opens its trades whatever is in the player's hand.
        if (entity.type == Villagers.TYPE) {
            owner.villagerTradeController.openTrades(entity)
            return
        }
        // parity: `AbstractMinecart.interact`, which seats whoever clicked it.
        if (DetectorRails.isMinecart(entity.type)) {
            mount(session, entity)
            return
        }
        if (tameOrSit(session, entity)) return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val item = Identifier.parse(held.id)
        if (!Breeding.isFood(entity.type, item)) return
        if (!Breeding.canFallInLove(entity)) return
        Breeding.feed(entity)
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            owner.sendInventorySlot(heldSlot)
        }
    }

    /** parity: `getDamageAfterArmorAbsorb` then `getDamageAfterMagicAbsorb`, in that order. */
    internal fun reduced(
        session: PlayerSession,
        amount: Float,
        damageType: Identifier?,
    ): Float {
        val tags = owner.damageTypeTags
        if (tags == null || damageType == null) return amount
        var damage = amount
        if (!tags.isIn(RealDamageTypeTags.BYPASSES_ARMOUR, damageType)) {
            var armour = 0f
            var toughness = 0f
            for (stack in wornArmour(session)) {
                val piece = stack?.let { ItemArmour.of(Identifier.parse(it.id)) } ?: continue
                armour += piece.armour
                toughness += piece.toughness
            }
            if (armour > 0f) damage = Combat.afterArmour(damage, armour, toughness)
        }
        if (tags.isIn(RealDamageTypeTags.BYPASSES_EFFECTS, damageType)) return damage
        val resistance = session.effects[MobEffects.RESISTANCE]
        if (resistance != null && !tags.isIn(RealDamageTypeTags.BYPASSES_RESISTANCE, damageType)) {
            damage = Combat.afterResistance(damage, resistance.amplifier)
        }
        if (damage <= 0f || tags.isIn(RealDamageTypeTags.BYPASSES_ENCHANTMENTS, damageType)) return damage
        val enchantments = owner.enchantments ?: return damage
        val protection =
            DamageProtection.of(wornArmour(session), damageType, tags) { enchantments.definitionOf(it) }
        return if (protection > 0f) Combat.afterProtection(damage, protection) else damage
    }

    private fun wornArmour(session: PlayerSession): List<StoredItemStack?> =
        (PlayerInventory.ARMOUR_START until PlayerInventory.ARMOUR_START + PlayerInventory.ARMOUR_SLOTS)
            .map { session.inventory[it] }

    /**
     * How far this player has fallen, and what it costs them when they land.
     *
     * parity: `Entity.checkFallDamage` plus `LivingEntity.calculateFallDamage`, which is
     * `floor(distance - 3)`: the distance is added up from the movement itself, not asked of the
     * client, and anything that cancels a fall in vanilla (water, a ladder, flying, creative)
     * clears it instead.
     */
    internal fun trackFall(
        session: PlayerSession,
        y: Double,
        onGround: Boolean,
        feetState: Int?,
    ) {
        val cancelled =
            session.flying ||
                session.gamemode == Gamemode.CREATIVE ||
                session.gamemode == Gamemode.SPECTATOR ||
                (feetState != null && (Fluids.of(feetState) != Submersion.NONE || isClimbable(feetState)))
        if (cancelled) {
            fallDistance = 0.0
            return
        }
        if (!onGround && y < session.y) fallDistance += session.y - y
        if (onGround && !session.onGround) {
            applyFallDamage(session)
            fallDistance = 0.0
        }
        if (onGround) fallDistance = 0.0
    }

    private fun applyFallDamage(session: PlayerSession) {
        if (owner.currentWorld?.gameRules?.get(GameRuleRegistry.FALL_DAMAGE) == false) return
        val damage = Combat.fallDamage(fallDistance)
        if (damage <= 0) return
        session.stats.addFallen(fallDistance)
        owner.hurt(damage.toFloat(), ClientSession.DEATH_FALL, DamageTypes.FALL)
    }

    /** parity: `BlockTags.CLIMBABLE`, which is what stops a fall without stopping the player. */
    private fun isClimbable(stateId: Int): Boolean =
        Blocks.byStateId(stateId)?.block?.path in ClientSession.CLIMBABLE_BLOCKS

    /** The red flash and the hurt sound, which the client plays off this and nothing else. */
    internal fun sendDamageEvent(
        session: PlayerSession,
        damageType: Identifier?,
        causeEntityId: Int = -1,
    ) {
        val registryId = VanillaRegistryData.damageTypes.indexOfFirst { it.first == (damageType?.path ?: "generic") }
        if (registryId < 0) return
        val packet =
            DamageEventPacket(
                entityId = session.entityId,
                damageType = registryId,
                causeEntityId = causeEntityId,
                directEntityId = causeEntityId,
            )
        owner.sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        }
    }

    /**
     * The shove a hit is worth, and the lean the client puts on the screen.
     *
     * parity: `LivingEntity.dealDefaultKnockback` and `ServerPlayer.indicateDamage`. A client moves
     * its own player, so the push only happens because the motion packet says so; the server has no
     * copy of what the player was already doing, so the push starts from a standstill.
     */
    internal fun pushAway(
        session: PlayerSession,
        source: DamageSource,
    ) {
        val towardsX = source.x - session.x
        val towardsZ = source.z - session.z
        val moved =
            Combat.knockback(
                power = EntityCombat.DEFAULT_KNOCKBACK,
                towardsX = towardsX,
                towardsZ = towardsZ,
                velocityX = 0.0,
                velocityY = 0.0,
                velocityZ = 0.0,
                onGround = session.onGround,
            )
        owner.sendPlayPacket(SetEntityMotionPacket.ID) {
            SetEntityMotionPacket(session.entityId, moved[0], moved[1], moved[2]).write(it)
        }
        val angle = (atan2(towardsZ, towardsX) * ClientSession.RADIANS_TO_DEGREES).toFloat() - session.yaw
        owner.sendPlayPacket(HurtAnimationPacket.ID) { HurtAnimationPacket(session.entityId, angle).write(it) }
    }

    /**
     * parity: `LivingEntity.hurt`, minus absorption, which no effect here grants yet.
     *
     * [damageType] is what decides how much of this armour and enchantments stop; a hit that does
     * not name one is taken as going straight through, which is what almost every source in this
     * build does anyway (`bypasses_armor` holds starving, drowning, withering and the rest).
     */
    internal fun hurt(
        amount: Float,
        deathMessageId: String,
        damageType: Identifier? = null,
        source: DamageSource? = null,
    ) {
        val session = owner.playerSession ?: return
        if (session.dead || session.invulnerable) return
        // A player is an entity too, so it is the same event; what changes is that the choke point
        // for a player is here rather than in EntityCombat.
        var amount = amount
        val pluginEvents = owner.pluginEvents
        if (pluginEvents != null && pluginEvents.listening(PluginEventTypes.ENTITY_DAMAGE)) {
            val event = pluginEvents.event(PluginEventTypes.ENTITY_DAMAGE)
            event.set(0, session.entityId)
            event.set(1, "minecraft:player")
            event.set(2, owner.currentWorld?.name)
            event.set(3, damageType?.path ?: deathMessageId)
            event.set(4, amount.toDouble())
            event.set(5, source?.entityId?.toString())
            if (!pluginEvents.fire(event)) return
            amount = ((event.get("amount") as? Double) ?: amount.toDouble()).toFloat()
            if (amount <= 0f) return
        }
        // parity: `LivingEntity.hurt`'s cooldown: inside the half second after a hit, only a bigger
        // one lands, and only by the difference. It is what stops lava and burning stacking.
        var incoming = amount
        val cooling = session.invulnerableTicks > ClientSession.COOLDOWN_HALF_TICKS
        if (cooling) {
            if (amount <= session.lastHurt) return
            incoming = amount - session.lastHurt
            session.lastHurt = amount
        } else {
            session.lastHurt = amount
            session.invulnerableTicks = ClientSession.HURT_COOLDOWN_TICKS
        }
        val reduced = reduced(session, incoming, damageType)
        if (reduced <= 0f) return
        if (!cooling) {
            sendDamageEvent(session, damageType, source?.entityId ?: -1)
            if (source != null) pushAway(session, source)
        }
        // parity: `LivingEntity.hurt` takes it out of absorption first, and only the rest off health.
        val absorbed = minOf(session.absorption, reduced)
        if (absorbed > 0f) {
            session.absorption -= absorbed
            owner.uiState.syncAbsorption()
        }
        val damage = reduced - absorbed
        if (damage <= 0f) return
        session.stats.addDamageTaken(damage)
        session.health = maxOf(session.health - damage, 0f)
        if (session.health <= 0f) die(deathMessageId) else owner.uiState.syncHealth()
    }

    /** parity: `LivingEntity.heal`, capped at the maximum and reported like any other change. */
    internal fun heal(amount: Float) {
        val healing =
            owner.playerSession?.let { session ->
                owner.pluginEvents.emit(PluginEventTypes.ENTITY_HEAL) { event ->
                    event.set(0, session.entityId)
                    event.set(1, "minecraft:player")
                    event.set(2, owner.currentWorld?.name)
                    event.set(3, amount.toDouble())
                    event.set(4, "regen")
                }
            }
        if (healing?.cancelled == true) return
        @Suppress("NAME_SHADOWING")
        val amount = ((healing?.get("amount") as? Double) ?: amount.toDouble()).toFloat()
        val session = owner.playerSession ?: return
        if (session.dead) return
        val healed = minOf(session.health + amount, session.maxHealth)
        if (healed == session.health) return
        session.health = healed
        owner.uiState.syncHealth()
    }

    /** parity: `Entity.kill`, which is unblockable: creative mode does not survive it. */
    internal fun killPlayer(deathMessageId: String) {
        val session = owner.playerSession ?: return
        if (session.dead) return
        session.health = 0f
        die(deathMessageId)
    }

    /**
     * parity: `ServerPlayer.die`. The death message goes to everyone (`showDeathMessages` is on by
     * default) and the death screen itself is [PlayerCombatKillPacket]: without it the client never
     * learns it died and keeps playing.
     */
    private fun die(deathMessageId: String) {
        owner.playerSession?.let { session ->
            owner.pluginEvents.emit(PluginEventTypes.PLAYER_DEATH) { event ->
                event.set(0, session.name)
                event.set(1, session.uuid.toString())
                event.set(2, owner.currentWorld?.name)
                event.set(3, session.x)
                event.set(4, session.y)
                event.set(5, session.z)
                event.set(6, deathMessageId)
            }
        }
        val session = owner.playerSession ?: return
        session.dead = true
        session.health = 0f
        // parity: `LivingEntity.die` empties the absorption pool, so respawning starts without it.
        session.absorption = 0f
        session.stats.increment(StoredStats.DEATHS)
        owner.uiState.syncHealth()
        val message = Component.translatable("death.attack.$deathMessageId", Component.text(session.name))
        owner.sendCompressed(PlayerCombatKillPacket.ID) {
            PlayerCombatKillPacket(session.entityId, message).write(it)
        }
        owner.logger.info { message.resolvePlainText(owner.consoleLang) }
        for (player in owner.playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(SystemChatPacket.ID) {
                SystemChatPacket(message, isActionBar = false).write(it)
            }
        }
    }
}
