package org.basaltmc.storage

import org.basaltmc.storage.db.PlayerDatabase
import org.basaltmc.storage.db.Player as PlayerRow

/** A player who has not been seen for a while, as the admin queries report them. */
public data class PlayerSummary(
    val uuid: String,
    val name: String,
    val value: Long,
)

/**
 * Reading and writing players, and the handful of questions an operator asks about them.
 *
 * Every write is one statement against one row, which SQLite runs in its own implicit transaction:
 * a player is either saved or not, never half-saved, and a crash mid-write cannot leave the
 * position updated with the inventory from before it. That is the property this whole layer exists
 * for, and the one that `playerdata/<uuid>.dat` cannot offer, since a truncated file is a lost
 * player.
 *
 * The admin queries deliberately select only columns, never blobs: counting players by game mode
 * should not deserialize a single inventory.
 */
public class PlayerRepository(
    private val database: PlayerDatabase,
) {
    private val queries = database.playerQueries

    public fun load(uuid: String): StoredPlayer? = queries.selectByUuid(uuid).executeAsOneOrNull()?.toStoredPlayer()

    public fun loadByName(name: String): StoredPlayer? =
        queries.selectByName(name).executeAsOneOrNull()?.toStoredPlayer()

    public fun exists(uuid: String): Boolean = queries.exists(uuid).executeAsOne()

    public fun save(player: StoredPlayer) {
        queries.upsert(
            uuid = player.uuid,
            name = player.name,
            last_seen = player.lastSeenEpochSeconds,
            first_seen = player.firstSeenEpochSeconds,
            play_time_ticks = player.playTimeTicks,
            world = player.world,
            x = player.x,
            y = player.y,
            z = player.z,
            yaw = player.yaw.toDouble(),
            pitch = player.pitch.toDouble(),
            health = player.health.toDouble(),
            food = player.food.toLong(),
            saturation = player.saturation.toDouble(),
            exhaustion = player.exhaustion.toDouble(),
            xp_level = player.xpLevel.toLong(),
            xp_progress = player.xpProgress.toDouble(),
            xp_total = player.xpTotal.toLong(),
            gamemode = player.gamemode.ordinal.toLong(),
            group_name = player.group,
            inventory = InventoryNbt.encode(player.inventory),
            ender_chest = InventoryNbt.encode(player.enderChest),
            stats = null,
        )
    }

    /**
     * Saves many players as one transaction, which is how the periodic flush writes.
     *
     * Not a micro-optimisation: without it SQLite commits (and, in WAL, appends and eventually
     * fsyncs) once per player, so a hundred online players cost a hundred commits every save
     * interval. It is also the crash-safety story for a shutdown save, since either the whole
     * batch lands or none of it does.
     */
    public fun saveAll(players: Collection<StoredPlayer>) {
        if (players.isEmpty()) return
        database.transaction {
            for (player in players) save(player)
        }
    }

    /**
     * Moves a player between groups without touching the rest of their row.
     *
     * A whole-player upsert would need every other column, which `op` on somebody who is offline
     * does not have and should not have to invent. Returns false when there is no such player: this
     * server can only administer people it has seen at least once, since it has no profile cache to
     * resolve a name it has never met.
     */
    public fun setGroup(
        uuid: String,
        group: String?,
    ): Boolean {
        if (!exists(uuid)) return false
        queries.setGroup(group, uuid)
        return true
    }

    public fun findInGroup(group: String): List<PlayerSummary> =
        queries
            .findByGroup(group)
            .executeAsList()
            .map { PlayerSummary(it.uuid, it.name, 0L) }

    public fun delete(uuid: String) {
        queries.delete(uuid)
    }

    // --- Admin queries: none of these touch a blob. ---

    public fun count(): Long = queries.countAll().executeAsOne()

    public fun countByGamemode(): Map<StoredGamemode, Long> =
        queries
            .countByGamemode()
            .executeAsList()
            .associate { StoredGamemode.byId(it.gamemode.toInt()) to it.total }

    public fun findLastSeenBefore(epochSeconds: Long): List<PlayerSummary> =
        queries
            .findByLastSeenBefore(epochSeconds)
            .executeAsList()
            .map { PlayerSummary(it.uuid, it.name, it.last_seen) }

    public fun findInWorld(world: String): List<PlayerSummary> =
        queries
            .findByWorld(world)
            .executeAsList()
            .map { PlayerSummary(it.uuid, it.name, 0L) }

    public fun findWithPlayTimeAtLeast(ticks: Long): List<PlayerSummary> =
        queries
            .findByPlayTimeAtLeast(ticks)
            .executeAsList()
            .map { PlayerSummary(it.uuid, it.name, it.play_time_ticks) }

    private fun PlayerRow.toStoredPlayer(): StoredPlayer =
        StoredPlayer(
            uuid = uuid,
            name = name,
            lastSeenEpochSeconds = last_seen,
            firstSeenEpochSeconds = first_seen,
            playTimeTicks = play_time_ticks,
            world = world,
            x = x,
            y = y,
            z = z,
            yaw = yaw.toFloat(),
            pitch = pitch.toFloat(),
            health = health.toFloat(),
            food = food.toInt(),
            saturation = saturation.toFloat(),
            exhaustion = exhaustion.toFloat(),
            xpLevel = xp_level.toInt(),
            xpProgress = xp_progress.toFloat(),
            xpTotal = xp_total.toInt(),
            gamemode = StoredGamemode.byId(gamemode.toInt()),
            group = group_name,
            inventory = inventory?.let(InventoryNbt::decode) ?: StoredInventory.empty(),
            enderChest =
                ender_chest?.let(InventoryNbt::decode)
                    ?: StoredInventory.empty(StoredPlayer.ENDER_CHEST_SLOTS),
        )
}
