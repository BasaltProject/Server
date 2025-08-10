package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.registry.AttributeIds
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.ConsumeEffect
import org.basaltmc.server.registry.CustomStats
import org.basaltmc.server.runtime.command.PlayerCommandSource
import org.basaltmc.server.runtime.command.timePacket
import org.basaltmc.server.runtime.placeholder.PlaceholderSlots
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.EnchantmentAttributes
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.server.runtime.world.WorldWeather
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.protocol.play.AwardStatsPacket
import org.basaltmc.wire.protocol.play.ClientboundPlayerAbilitiesPacket
import org.basaltmc.wire.protocol.play.CommandsPacket
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.GameEventPacket
import org.basaltmc.wire.protocol.play.Pose
import org.basaltmc.wire.protocol.play.RemoveMobEffectPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetExperiencePacket
import org.basaltmc.wire.protocol.play.SetHealthPacket
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.protocol.play.StatCounter
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.UpdateMobEffectPacket
import kotlin.random.Random

/**
 * The HUD-facing sync surface: health, effects, absorption, experience, abilities, the command
 * tree, statistics and the sidebar/scoreboard/boss-bar redraw.
 *
 * A back-reference to the owning session rather than narrow callbacks, the same pattern as
 * [ItemUseTimers]: this state was already tightly coupled to [ClientSession] before the
 * extraction, and every method here still needs a handful of its collaborators.
 */
internal class UiStateSync(
    private val owner: ClientSession,
) {
    /**
     * The shared flags byte: everything about how this player is standing that fits in eight bits.
     *
     * parity: `Entity.DATA_SHARED_FLAGS_ID`. One byte for all of them, which is why it is composed
     * in one place: whoever writes only the bit they care about erases the others.
     */
    private fun sharedFlags(session: PlayerSession): Int {
        var flags = 0
        if (session.fireTicks > 0) flags = flags or SetEntityDataPacket.FLAG_ON_FIRE
        if (session.sneaking) flags = flags or SetEntityDataPacket.FLAG_SNEAKING
        if (session.sprinting) flags = flags or SetEntityDataPacket.FLAG_SPRINTING
        return flags
    }

    /**
     * How this player looks to everyone else: crouched, sprinting, burning, and the pose that goes
     * with it. Sent only when it changes, which for a crouch is a handful of times a minute.
     */
    internal fun syncPosture() {
        val session = owner.playerSession ?: return
        if (session.sleepingBed != null) return
        val flags = sharedFlags(session)
        // parity: `Player.updatePlayerPose`, of which this build has the two poses it can tell
        // apart: crouching and standing.
        val pose = if (session.sneaking) Pose.CROUCHING else Pose.STANDING
        if (flags == owner.entityTracker.lastSharedFlags && pose == owner.entityTracker.lastPose) return
        owner.entityTracker.lastSharedFlags = flags
        owner.entityTracker.lastPose = pose
        val packet = SetEntityDataPacket.posture(session.entityId, flags, pose)
        owner.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    /** The air left and whether this player burns, to them and to everyone who can see them. */
    internal fun syncSubmersion() {
        val session = owner.playerSession ?: return
        val packet = SetEntityDataPacket.submersion(session.entityId, session.airSupply, sharedFlags(session))
        owner.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    internal fun syncHealth() {
        val session = owner.playerSession ?: return
        owner.sendCompressed(SetHealthPacket.ID) {
            SetHealthPacket(session.health, session.food.foodLevel, session.food.saturationLevel).write(it)
        }
    }

    /** parity: `LivingEntity.addEffect`, for the effects a mob puts on somebody rather than a drink. */
    internal fun addEffect(
        effect: Identifier,
        durationTicks: Int,
        amplifier: Int,
    ) {
        val session = owner.playerSession ?: return
        val given =
            owner.playerEvent(PluginEventTypes.PLAYER_EFFECT) { event ->
                event.set(3, effect.toString())
                event.set(4, amplifier)
                event.set(5, durationTicks)
            }
        if (given?.cancelled == true) return
        val active = ActiveEffect(effect = effect, amplifier = amplifier, remainingTicks = durationTicks)
        session.effects[effect] = active
        sendEffect(active)
    }

    /** Sends one effect, or its removal when it has run out. parity: the two mob effect packets. */
    internal fun sendEffect(effect: ActiveEffect) {
        val session = owner.playerSession ?: return
        val id = MobEffects.protocolIdOf(effect.effect) ?: return
        // parity: `AbsorptionMobEffect.onEffectStarted` adds its hearts on top of whatever is left.
        if (effect.effect == MobEffects.ABSORPTION) {
            session.absorption += ClientSession.ABSORPTION_PER_LEVEL * (effect.amplifier + 1)
            syncAbsorption()
        }
        owner.sendPlayPacket(UpdateMobEffectPacket.ID) {
            UpdateMobEffectPacket(
                entityId = session.entityId,
                effect = id,
                amplifier = effect.amplifier,
                durationTicks = effect.remainingTicks,
                ambient = effect.ambient,
                visible = effect.visible,
                showIcon = effect.showIcon,
            ).write(it)
        }
    }

    internal fun sendEffectRemoved(effect: Identifier) {
        val session = owner.playerSession ?: return
        val id = MobEffects.protocolIdOf(effect) ?: return
        // parity: `AbsorptionMobEffect.onEffectRemoved` takes back exactly what it gave, and here
        // nothing else gives any, so the pool empties.
        if (effect == MobEffects.ABSORPTION) {
            session.absorption = 0f
            syncAbsorption()
        }
        owner.sendPlayPacket(RemoveMobEffectPacket.ID) { RemoveMobEffectPacket(session.entityId, id).write(it) }
    }

    internal fun syncHeldAttributes() {
        val session = owner.playerSession ?: return
        val held = session.inventory[PlayerInventory.HOTBAR_START + session.inventory.heldSlot]
        val snapshots = EnchantmentAttributes.snapshotsFor(held) { owner.enchantments?.definitionOf(it) }
        // Whatever a plugin set goes out beside them, and wins where both name the same attribute:
        // the base a plugin chose is the base, and the enchantments are modifiers on top.
        val fromPlugins =
            session.attributeBases.mapNotNull { (name, base) ->
                AttributeIds.protocolId(name)?.let { id ->
                    UpdateAttributesPacket.AttributeSnapshot(attribute = id, base = base)
                }
            }
        val all =
            snapshots.filterNot { snapshot -> fromPlugins.any { it.attribute == snapshot.attribute } } +
                fromPlugins
        owner.sendPlayPacket(UpdateAttributesPacket.ID) {
            UpdateAttributesPacket(session.entityId, all).write(it)
        }
    }

    /** Redraws whatever is due. Failures are swallowed: decoration must never drop a connection. */
    internal fun tickUi() {
        val session = owner.playerSession ?: return
        // A plugin holding a surface is reason enough to draw, even with all four files switched off.
        if (!owner.ui().anyEnabled &&
            owner.pluginHoldsSidebar?.invoke(session.uuid.toString()) != true &&
            owner.pluginHoldsBossBar?.invoke() != true
        ) {
            return
        }
        val context = PlaceholderContext.EMPTY.with(PlaceholderSlots.PLAYER, session)
        runCatching { owner.playerUi.tick(owner.uiTick++, context) }
            .onFailure { owner.logger.debug { "UI update failed for ${session.name}: ${it.message}" } }
    }

    /** parity: absorption rides on the player's own entity data, not on `set_health`. */
    internal fun syncAbsorption() {
        val session = owner.playerSession ?: return
        owner.sendCompressed(SetEntityDataPacket.ID) {
            SetEntityDataPacket.playerAbsorption(session.entityId, session.absorption).write(it)
        }
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { buffer ->
                SetEntityDataPacket.playerAbsorption(session.entityId, session.absorption).write(buffer)
            }
        }
    }

    /** This world's clocks, as the join sequence and every `/time` send them. */
    internal fun sendTime(world: LoadedWorld) {
        val packet = world.timePacket()
        owner.sendPlayPacket(SetTimePacket.ID) { packet.write(it) }
    }

    /**
     * parity: `ServerPlayer.setPlayerWeather`/`PlayerList.sendLevelInfo`, which announces rain as a
     * start/stop event *and* the two levels: the events swap the client's weather renderer, the
     * levels set how heavy it is, and a client sent only one of the two shows the wrong thing.
     */
    internal fun sendWeather(weather: WorldWeather) {
        owner.sendPlayPacket(GameEventPacket.ID) {
            GameEventPacket(
                if (weather.raining) GameEventPacket.START_RAINING else GameEventPacket.STOP_RAINING,
                0f,
            ).write(it)
        }
        owner.sendPlayPacket(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.RAIN_LEVEL_CHANGE, weather.rainLevel).write(it)
        }
        owner.sendPlayPacket(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.THUNDER_LEVEL_CHANGE, weather.thunderLevel).write(it)
        }
    }

    internal fun syncExperience() {
        val session = owner.playerSession ?: return
        if (session.experience.level != owner.entityTracker.lastSyncedLevel) {
            val was = owner.entityTracker.lastSyncedLevel
            owner.entityTracker.lastSyncedLevel = session.experience.level
            owner.playerEvent(PluginEventTypes.PLAYER_LEVEL_CHANGE) { event ->
                event.set(3, session.experience.level)
                event.set(4, was)
            }
        }
        owner.sendCompressed(SetExperiencePacket.ID) {
            SetExperiencePacket(
                session.experience.progress,
                session.experience.level,
                session.experience.total,
            ).write(it)
        }
    }

    internal fun sendAbilities() {
        val session = owner.playerSession ?: return
        owner.sendCompressed(ClientboundPlayerAbilitiesPacket.ID) {
            ClientboundPlayerAbilitiesPacket(
                invulnerable = session.invulnerable,
                flying = session.flying,
                mayFly = session.mayFly,
                instabuild = session.instabuild,
            ).write(it)
        }
    }

    /**
     * Sends the command tree this player may use, which is what makes their client parse commands
     * at all: with no tree, everything after a slash is unknown client-side.
     *
     * Re-sent whenever their permission level changes, because the tree is filtered by it.
     */
    internal fun sendCommandTree() {
        val session = owner.playerSession ?: return
        val dispatcher = owner.commands ?: return
        sendPermissionLevel()
        val tree = dispatcher.treeFor(PlayerCommandSource(session, dispatcher.environment.groups))
        owner.sendCompressed(CommandsPacket.ID) { tree.write(it) }
    }

    /**
     * parity: `PlayerList.sendPlayerPermissionLevel`, which goes right before the command tree.
     *
     * The client parses commands locally against this: below gamemaster it refuses selectors, so
     * without it `/tellraw @a ...` is underlined in red even though the server runs it fine.
     */
    private fun sendPermissionLevel() {
        val session = owner.playerSession ?: return
        val dispatcher = owner.commands ?: return
        val level = PlayerCommandSource(session, dispatcher.environment.groups).permissionLevel
        val packet = EntityEventPacket.permissionLevel(session.entityId, level)
        owner.sendPlayPacket(EntityEventPacket.ID) { packet.write(it) }
    }

    /**
     * The statistics screen's contents.
     *
     * parity: `ServerStatsCounter.sendStats`, which answers the screen's own request. Only the
     * counters this build keeps go out: the screen shows a zero for everything else, which is what
     * vanilla shows for a counter nobody has moved either.
     */
    internal fun sendStats() {
        val session = owner.playerSession ?: return
        val counters = session.stats.snapshot(owner.playTimeTicks).counters
        val stats = mutableListOf<StatCounter>()
        for ((key, value) in counters) {
            val count = value.coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
            val counter =
                when {
                    key.startsWith(ClientSession.CUSTOM_STAT_PREFIX) ->
                        CustomStats.idOf(key.removePrefix(ClientSession.CUSTOM_STAT_PREFIX))?.let {
                            StatCounter(AwardStatsPacket.TYPE_CUSTOM, it, count)
                        }
                    key.startsWith(ClientSession.MINED_STAT_PREFIX) ->
                        Blocks.REGISTRY
                            .idOf(Identifier.parse(key.removePrefix(ClientSession.MINED_STAT_PREFIX)))
                            ?.let { StatCounter(AwardStatsPacket.TYPE_MINED, it, count) }
                    else -> null
                }
            counter?.let { stats += it }
        }
        owner.sendCompressed(AwardStatsPacket.ID) { AwardStatsPacket(stats).write(it) }
    }

    /** parity: `Consumable.onConsume`, which is what makes a golden apple worth eating. */
    internal fun applyConsumeEffects(
        session: PlayerSession,
        effects: List<ConsumeEffect>,
    ) {
        for (effect in effects) {
            when (effect) {
                is ConsumeEffect.ClearAll -> {
                    for (id in session.effects.keys.toList()) {
                        session.effects.remove(id)
                        sendEffectRemoved(id)
                    }
                }
                is ConsumeEffect.Remove -> {
                    if (session.effects.remove(effect.effect) != null) sendEffectRemoved(effect.effect)
                }
                is ConsumeEffect.Apply -> {
                    if (effect.probability < 1f && Random.nextFloat() >= effect.probability) continue
                    val active =
                        ActiveEffect(
                            effect = effect.effect,
                            amplifier = effect.amplifier,
                            remainingTicks = effect.durationTicks,
                        )
                    session.effects[effect.effect] = active
                    sendEffect(active)
                }
            }
        }
    }
}
