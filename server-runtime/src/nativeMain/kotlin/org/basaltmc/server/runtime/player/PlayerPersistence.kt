package org.basaltmc.server.runtime.player

import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.server.storage.StoredInventory
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.wire.protocol.play.Gamemode
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * Translates between the live player and the row that outlives them.
 *
 * The mapping lives here, in the server, rather than in `server-storage`: the storage layer owns
 * how a player is written down, and knowing what a `PlayerSession` is would make it depend on the
 * server it is meant to sit underneath. Inventories are not carried yet because there is no
 * inventory system to carry: the column and its codec exist, so wiring one in later is a mapping
 * change and not a schema change.
 */
@OptIn(ExperimentalTime::class)
public object PlayerPersistence {
    public fun toStored(
        session: PlayerSession,
        world: String,
        firstSeenEpochSeconds: Long,
        playTimeTicks: Long,
        nowEpochSeconds: Long = Clock.System.now().epochSeconds,
    ): StoredPlayer =
        StoredPlayer(
            uuid = session.uuid.toString(),
            name = session.name,
            lastSeenEpochSeconds = nowEpochSeconds,
            firstSeenEpochSeconds = firstSeenEpochSeconds,
            playTimeTicks = playTimeTicks,
            world = world,
            x = session.x,
            y = session.y,
            z = session.z,
            yaw = session.yaw,
            pitch = session.pitch,
            health = session.health,
            food = session.food.foodLevel,
            saturation = session.food.saturationLevel,
            exhaustion = session.food.exhaustionLevel,
            xpLevel = session.experience.level,
            xpProgress = session.experience.progress,
            xpTotal = session.experience.total,
            gamemode = StoredGamemode.entries[session.gamemode.ordinal],
            group = session.group,
            stats = session.stats.snapshot(playTimeTicks),
            inventory = session.inventory.snapshot(),
            enderChest = StoredInventory(session.enderChest.toList()),
            tags = session.tags.toSet(),
        )

    /** Puts a stored row back onto a freshly created session, in place. */
    public fun restore(
        session: PlayerSession,
        stored: StoredPlayer,
    ) {
        session.x = stored.x
        session.y = stored.y
        session.z = stored.z
        session.yaw = stored.yaw
        session.pitch = stored.pitch
        session.health = stored.health
        session.food.restore(stored.food, stored.saturation, stored.exhaustion)
        session.experience.restore(stored.xpLevel, stored.xpProgress, stored.xpTotal)
        session.gamemode = Gamemode.entries[stored.gamemode.ordinal]
        session.group = stored.group
        session.flying = session.gamemode == Gamemode.SPECTATOR
        session.stats.restore(stored.stats)
        session.inventory.restore(stored.inventory)
        for (slot in session.enderChest.indices) {
            session.enderChest[slot] = stored.enderChest.slots.getOrNull(slot)
        }
        session.tags.clear()
        session.tags += stored.tags
    }
}
