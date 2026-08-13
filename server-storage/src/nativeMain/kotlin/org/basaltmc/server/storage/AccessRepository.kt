package org.basaltmc.server.storage

import org.basaltmc.server.storage.db.PlayerDatabase

/** Where one player respawns, as `/spawnpoint` set it. */
public data class StoredSpawnPoint(
    public val uuid: String,
    public val world: String,
    public val x: Double,
    public val y: Double,
    public val z: Double,
    public val yaw: Float,
)

/** One ban, whether it names a player or an address. */
public data class BanRecord(
    public val subject: String,
    public val kind: BanKind,
    public val name: String?,
    public val reason: String,
    public val source: String,
    public val createdAtEpochSeconds: Long,
    public val expiresAtEpochSeconds: Long?,
)

public enum class BanKind(
    public val storedName: String,
) {
    PLAYER("player"),
    IP("ip"),
}

public data class WhitelistEntry(
    public val uuid: String,
    public val name: String,
    public val addedAtEpochSeconds: Long,
)

/**
 * Who may connect. Reads happen on the login path, so every query here is indexed and answers
 * without a scan.
 */
public class AccessRepository(
    database: PlayerDatabase,
) {
    private val queries = database.accessQueries
    private val spawnQueries = database.spawnPointQueries

    public fun spawnPointOf(uuid: String): StoredSpawnPoint? =
        spawnQueries.selectSpawnPoint(uuid).executeAsOneOrNull()?.let {
            StoredSpawnPoint(it.uuid, it.world, it.x, it.y, it.z, it.yaw.toFloat())
        }

    public fun setSpawnPoint(point: StoredSpawnPoint) {
        spawnQueries.setSpawnPoint(point.uuid, point.world, point.x, point.y, point.z, point.yaw.toDouble())
    }

    public fun clearSpawnPoint(uuid: String) {
        spawnQueries.deleteSpawnPoint(uuid)
    }

    public fun banOf(
        subject: String,
        kind: BanKind,
    ): BanRecord? =
        queries
            .selectBan(subject, kind.storedName)
            .executeAsOneOrNull()
            ?.let { row ->
                BanRecord(
                    row.subject,
                    kind,
                    row.name,
                    row.reason,
                    row.source,
                    row.created_at,
                    row.expires_at,
                )
            }

    public fun bans(kind: BanKind): List<BanRecord> =
        queries.listBans(kind.storedName).executeAsList().map { row ->
            BanRecord(row.subject, kind, row.name, row.reason, row.source, row.created_at, row.expires_at)
        }

    public fun ban(record: BanRecord) {
        queries.insertBan(
            record.subject,
            record.kind.storedName,
            record.name,
            record.reason,
            record.source,
            record.createdAtEpochSeconds,
            record.expiresAtEpochSeconds,
        )
    }

    /** True when something was actually removed, which is what `/pardon` reports on. */
    public fun pardon(
        subject: String,
        kind: BanKind,
    ): Boolean {
        if (banOf(subject, kind) == null) return false
        queries.deleteBan(subject, kind.storedName)
        return true
    }

    public fun whitelist(): List<WhitelistEntry> =
        queries.listWhitelist().executeAsList().map { WhitelistEntry(it.uuid, it.name, it.added_at) }

    public fun isWhitelisted(uuid: String): Boolean = queries.selectWhitelisted(uuid).executeAsOneOrNull() != null

    public fun whitelistedByName(name: String): WhitelistEntry? =
        queries.selectWhitelistedByName(name).executeAsOneOrNull()?.let {
            WhitelistEntry(it.uuid, it.name, it.added_at)
        }

    public fun addToWhitelist(entry: WhitelistEntry) {
        queries.insertWhitelist(entry.uuid, entry.name, entry.addedAtEpochSeconds)
    }

    public fun removeFromWhitelist(uuid: String): Boolean {
        if (queries.selectWhitelisted(uuid).executeAsOneOrNull() == null) return false
        queries.deleteWhitelist(uuid)
        return true
    }
}
