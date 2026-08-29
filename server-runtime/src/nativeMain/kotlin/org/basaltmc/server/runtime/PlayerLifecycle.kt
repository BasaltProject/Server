package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.player.PlayerPersistence
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.server.storage.StoredStats
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.GameEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.GlobalPos
import org.basaltmc.wire.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.SpawnPositionPacket
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * The player tick, advancements, respawn, game mode, world changes, teleporting, leaving and the
 * player row's persistence. [ClientSession.setGamemode], [ClientSession.switchWorld],
 * [ClientSession.teleportTo] and [ClientSession.storedSnapshot] stay as thin delegates onto this
 * class, since their callers (commands, portals, the plugin bridge) are too wide to move wholesale.
 */
internal class PlayerLifecycle(
    private val owner: ClientSession,
) {
    /**
     * One player tick, driven from this connection's own outbound loop rather than [TickLoop].
     *
     * That loop already runs at 20 Hz on this connection's thread, which is where anything that
     * writes to this socket has to happen (see [ClientSession.startOutboundLoop]): hunger and
     * regeneration both end in a `set_health` packet, so ticking them from the shared tick thread
     * would park every other player behind a client that has stopped reading.
     */
    internal fun tickPlayer() {
        val session = owner.playerSession ?: return
        owner.playTimeTicks++
        if (session.sleepingBed == null) session.stats.increment(StoredStats.TIME_SINCE_REST)
        checkAdvancements(session)
        if (session.invulnerableTicks > 0) session.invulnerableTicks--
        // parity: `Player.tick`, which counts this up for as long as the player is not swinging.
        session.ticksSinceSwing++
        if (session.dead) return
        owner.tryPickupNearbyItems()
        owner.itemUseTimers.tickEating(session)
        owner.itemUseTimers.tickDrawing(session)
        if (session.sleepingBed != null) session.sleepTicks++
        owner.blockInteraction.tickDigging(session)
        if (session.gamemode == Gamemode.CREATIVE || session.gamemode == Gamemode.SPECTATOR) return

        val healthBefore = session.health
        val foodBefore = session.food.foodLevel
        val saturationBefore = session.food.saturationLevel
        session.food.tick(
            difficulty = owner.difficulty,
            health = session.health,
            maxHealth = session.maxHealth,
            heal = { amount -> session.health = minOf(session.health + amount, session.maxHealth) },
            hurt = { amount -> owner.hurt(amount, ClientSession.DEATH_STARVE, DamageTypes.STARVE) },
        )
        // Only when something actually moved: `set_health` every tick is 20 packets a second per
        // player for a bar that changes a few times a minute.
        if (session.food.foodLevel != foodBefore) {
            val change =
                owner.playerEvent(PluginEventTypes.PLAYER_FOOD_CHANGE) { event ->
                    event.set(3, session.food.foodLevel)
                    event.set(4, foodBefore)
                }
            if (change != null) {
                val wanted =
                    if (change.cancelled) {
                        foodBefore
                    } else {
                        (
                            (change.get("food") as? Long)?.toInt()
                                ?: session.food.foodLevel
                        )
                    }
                if (wanted != session.food.foodLevel) session.food.set(wanted, session.food.saturationLevel)
            }
        }
        if (session.health != healthBefore ||
            session.food.foodLevel != foodBefore ||
            session.food.saturationLevel != saturationBefore
        ) {
            owner.uiState.syncHealth()
        }
    }

    /**
     * Fires the inventory trigger once a tick, when something moved.
     *
     * parity: `ServerPlayer.inventoryMenu.broadcastChanges`, which is where vanilla checks it too:
     * once per tick after everything has moved, not once per item, so crafting a stack does not
     * check the same advancement sixty-four times.
     */
    private fun checkAdvancements(session: PlayerSession) {
        val triggers = owner.advancementTriggers ?: return
        if (!session.inventory.changed) return
        session.inventory.clearChanged()
        triggers.onInventoryChanged(session.name, session.uuid.toString(), session.inventory.snapshot().slots)
    }

    /**
     * parity: `PlayerList.respawn`, in the order it sends things: respawn, then position, then the
     * spawn point, then experience.
     *
     * The client throws its whole level away when it handles the respawn packet, so the chunks
     * around the spawn point have to be sent again; see [ChunkStreamer.resetForRespawn].
     */
    internal fun respawn() {
        val session = owner.playerSession ?: return
        // parity: `handleClientCommand` returns without doing anything if the player is still
        // alive, so a client that sends this spuriously cannot teleport itself to spawn.
        if (session.health > 0f) return
        // parity: `ServerPlayer.findRespawnPositionAndUseSpawnBlock`, minus the bed and anchor
        // checks: what `/spawnpoint` set wins, and the world spawn is the fallback.
        val stored =
            owner.playerStore
                ?.takeIf { it.isOpen }
                ?.access
                ?.spawnPointOf(session.uuid.toString())
        val respawnWorld = stored?.world?.let { owner.worlds[it] } ?: requireNotNull(owner.currentWorld)
        val (spawnX, spawnY, spawnZ) =
            if (stored != null) Triple(stored.x, stored.y, stored.z) else respawnWorld.spawn
        session.resetForRespawn()
        session.x = spawnX
        session.y = spawnY
        session.z = spawnZ
        owner.pluginEvents.emit(PluginEventTypes.PLAYER_RESPAWN) { event ->
            event.set(0, session.name)
            event.set(1, session.uuid.toString())
            event.set(2, respawnWorld.name)
            event.set(3, spawnX)
            event.set(4, spawnY)
            event.set(5, spawnZ)
        }

        owner.entityTracker.forgetEverythingDrawn()
        owner.sendCompressed(RespawnPacket.ID) {
            RespawnPacket(owner.spawnInfo(session.gamemode), dataToKeep = 0).write(it)
        }
        owner.sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(
                0,
                session.x,
                session.y,
                session.z,
                0.0,
                0.0,
                0.0,
                session.yaw,
                session.pitch,
                relativeFlags = 0,
            ).write(it)
        }
        // parity: `PlayerList.sendLevelInfo` again, which is what a respawn goes through too.
        owner.currentWorld?.let { respawnWorld ->
            owner.uiState.sendTime(respawnWorld)
            if (respawnWorld.weather.raining) owner.uiState.sendWeather(respawnWorld.weather)
        }
        owner.sendCompressed(SpawnPositionPacket.ID) {
            SpawnPositionPacket(
                GlobalPos(
                    requireNotNull(owner.currentWorld).levelKey,
                    BlockPos(spawnX.toInt(), spawnY.toInt(), spawnZ.toInt()),
                ),
                yaw = 0f,
                pitch = 0f,
            ).write(it)
        }
        owner.uiState.sendAbilities()
        owner.uiState.syncHealth()
        owner.uiState.syncExperience()

        owner.chunkStreamer.resetForRespawn(spawnX, spawnZ)

        for (other in owner.playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(session.entityId, spawnX, spawnY, spawnZ, 0f, 0f, onGround = true).write(it)
            }
        }
    }

    /**
     * parity: `ServerPlayer.setGameMode`, which tells the client through a game event rather than a
     * dedicated packet, then re-sends abilities because the new mode decides them.
     */
    internal fun setGamemode(mode: Gamemode) {
        val session = owner.playerSession ?: return
        val change =
            owner.playerEvent(PluginEventTypes.PLAYER_GAMEMODE_CHANGE) { event ->
                event.set(3, mode.name.lowercase())
                event.set(4, session.gamemode.name.lowercase())
            }
        if (change?.cancelled == true) {
            change.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        session.gamemode = mode
        if (!session.mayFly) session.flying = false
        if (mode == Gamemode.SPECTATOR) session.flying = true
        owner.sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.CHANGE_GAME_MODE, mode.ordinal.toFloat()).write(it)
        }
        owner.uiState.sendAbilities()
        // Broadcast to everyone, this player included, because the game event above only moves the
        // HUD: a client reads its *own* tab-list entry to decide whether it is a spectator, and
        // that is what turns off collision and hides it from other players. Sending one without the
        // other is what left spectator mode looking right and behaving like survival.
        for (player in owner.playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(PlayerInfoUpdatePacket.ID) {
                PlayerInfoUpdatePacket.gameMode(session.uuid, mode).write(it)
            }
        }
    }

    /**
     * Moves this player into another world, or does nothing if they are already in it. Each world
     * has its own [LoadedWorld.levelKey] so the respawn packet only rebuilds the client's chunk
     * cache when the key actually differs. What everyone *else* sees is deliberately not sent from
     * here: writing a removal straight to another socket left [syncPlayers]'s own tracking set out
     * of date, so this player never respawned for them again.
     */
    internal fun switchWorld(
        target: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val session = owner.playerSession ?: return false
        val from = owner.currentWorld?.name
        if (from != target.name) {
            owner.pluginEvents.emit(PluginEventTypes.PLAYER_WORLD_CHANGE) { event ->
                event.set(0, session.name)
                event.set(1, session.uuid.toString())
                event.set(2, target.name)
                event.set(3, from)
            }
        }
        val previous = owner.currentWorld
        if (previous == null || previous.name == target.name) return false

        owner.currentWorld = target
        session.worldName = target.name
        session.x = x
        session.y = y
        session.z = z

        owner.entityTracker.forgetEverythingDrawn()
        owner.sendCompressed(RespawnPacket.ID) {
            RespawnPacket(owner.spawnInfo(session.gamemode), dataToKeep = RespawnPacket.KEEP_ALL_DATA).write(it)
        }
        owner.sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(0, x, y, z, 0.0, 0.0, 0.0, session.yaw, session.pitch, relativeFlags = 0).write(it)
        }
        owner.uiState.sendAbilities()
        owner.uiState.syncHealth()
        owner.uiState.syncExperience()

        owner.chunkStreamer.resetForWorldSwitch(x, z, target.settings.viewDistance)

        owner.logger.info { "${session.name} moved to world '${target.name}'." }
        return true
    }

    internal fun teleportTo(
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        pitch: Float,
        /** What asked for it, for a plugin that treats a command differently from a portal. */
        cause: String = "command",
    ) {
        val session = owner.playerSession
        val teleport =
            owner.playerEvent(PluginEventTypes.PLAYER_TELEPORT) { event ->
                event.set(3, session?.x)
                event.set(4, session?.y)
                event.set(5, session?.z)
                event.set(6, x)
                event.set(7, y)
                event.set(8, z)
                event.set(9, cause)
            }
        if (teleport?.cancelled == true) {
            teleport.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        owner.sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(0, x, y, z, 0.0, 0.0, 0.0, yaw, pitch, relativeFlags = 0).write(it)
        }
        // parity: `Entity.teleportTo` clears the fall, so arriving is never a landing.
        owner.combatHandler.fallDistance = 0.0
        // Straight to updatePosition rather than waiting for the client to confirm: the world has
        // to agree the player is there now, so chunks around the destination start generating
        // immediately instead of one round trip later.
        owner.updatePosition(x, y, z, yaw, pitch, onGround = false)
    }

    internal fun leavePlayIfPresent() {
        val session = owner.playerSession ?: return
        val registry = owner.playerRegistry ?: return
        // Leaving while asleep would otherwise leave the bed occupied for ever.
        owner.wakeUp()
        // And leaving with a chest open would leave its lid up and a trapped chest powered.
        owner.menuController.closeOpenMenu(session)
        // Before removing them from the registry, so a save triggered elsewhere cannot race this
        // one into writing a player who is already half gone.
        savePlayerState()
        registry.remove(session.uuid)
        owner.progress?.unload(session.uuid.toString())
        owner.logger.info { "${session.name} left the game" }
        owner.pluginEvents.emit(PluginEventTypes.PLAYER_QUIT) { event ->
            event.set(0, session.name)
            event.set(1, session.uuid.toString())
            event.set(2, owner.currentWorld?.name)
        }
        // parity: `ServerGamePacketListenerImpl.onDisconnect`, the other half of the pair above.
        owner.broadcastToEveryone(
            Component
                .translatable("multiplayer.player.left", Component.text(session.name))
                .color(TextColor.YELLOW),
        )
        for (other in registry.snapshot()) {
            other.connection.sendPlayPacket(
                PlayerInfoRemovePacket.ID,
            ) { PlayerInfoRemovePacket(listOf(session.uuid)).write(it) }
        }
    }

    /**
     * Writes this player's row. Called on logout and from the periodic flush.
     *
     * Failures are logged and swallowed: a database that cannot be written is a serious problem,
     * but taking the connection down over it loses the session as well as the save.
     */
    internal fun savePlayerState() {
        val session = owner.playerSession ?: return
        val store = owner.playerStore ?: return
        if (!store.isOpen) {
            // The server is shutting down and already saved everyone before disconnecting them.
            // This connection's own thread is just catching up.
            owner.logger.debug { "Skipping ${session.name}'s save: the database is closed." }
            return
        }
        runCatching {
            store.players.save(
                PlayerPersistence.toStored(session, owner.worldName, owner.firstSeenEpochSeconds, owner.playTimeTicks),
            )
            // After the save, because there has to be a row to update.
            if (!owner.addressRecorded && owner.rememberAddress != null) {
                store.players.setLastIp(session.uuid.toString(), owner.rememberAddress)
                owner.addressRecorded = true
            }
        }.onFailure { owner.logger.error(it) { "Failed to save ${session.name}" } }
    }

    /** This player's row as it stands right now, for the batched periodic flush. */
    internal fun storedSnapshot(): StoredPlayer? {
        val session = owner.playerSession ?: return null
        return PlayerPersistence.toStored(session, owner.worldName, owner.firstSeenEpochSeconds, owner.playTimeTicks)
    }
}
