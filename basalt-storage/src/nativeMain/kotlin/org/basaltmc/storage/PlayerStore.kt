package org.basaltmc.storage

import app.cash.sqldelight.db.SqlDriver
import app.cash.sqldelight.driver.native.NativeSqliteDriver
import app.cash.sqldelight.driver.native.wrapConnection
import co.touchlab.sqliter.DatabaseConfiguration
import co.touchlab.sqliter.JournalMode
import co.touchlab.sqliter.SynchronousFlag
import org.basaltmc.storage.db.PlayerDatabase

/**
 * The one player database, opened once for the whole server.
 *
 * One file, not one per player: a per-file layout cannot answer "who has not logged in for a month"
 * without opening every file, and it multiplies the number of things a crash can leave half-written
 * by the number of players. WAL mode is what makes a single file safe to write concurrently:
 * readers never block the writer and the writer never blocks readers, so a save on a connection
 * thread does not stall anything else.
 *
 * The SQLite underneath is compiled into this binary (see `basalt.sqlite-native`), so opening a
 * database has no dependency on what the host machine happens to have installed.
 */
public class PlayerStore private constructor(
    private val driver: SqlDriver,
    public val database: PlayerDatabase,
) : AutoCloseable {
    public val players: PlayerRepository = PlayerRepository(database)

    override fun close() {
        // No explicit checkpoint: SQLite folds the write-ahead log back into the database file and
        // removes it when the last connection to it closes, which this is. Asking for one by hand
        // is both unnecessary and awkward, since `PRAGMA wal_checkpoint` answers with a row and the
        // driver refuses to run a row-returning statement as a plain execute.
        driver.close()
    }

    public companion object {
        /**
         * Opens (creating if needed) the database at [path].
         *
         * `synchronous = NORMAL` is the documented companion to WAL: fsync on checkpoint rather
         * than on every commit. The failure it accepts is losing the last transactions if the
         * *machine* loses power; a crash of this process alone still cannot corrupt the file, which
         * is the property that matters here and the one vanilla's `.dat` writes do not have.
         */
        public fun open(path: String): PlayerStore {
            // The driver takes a directory and a file name, not a path, so a caller can keep
            // thinking in paths: `players.db` next to the server, `worlds/` beside it.
            val separator = path.lastIndexOf('/')
            val directory = if (separator >= 0) path.substring(0, separator).ifEmpty { "/" } else "."
            val fileName = if (separator >= 0) path.substring(separator + 1) else path

            val schema = PlayerDatabase.Schema
            val configuration =
                DatabaseConfiguration(
                    name = fileName,
                    version = schema.version.toInt(),
                    journalMode = JournalMode.WAL,
                    create = { connection -> wrapConnection(connection) { schema.create(it) } },
                    upgrade = { connection, from, to ->
                        // SQLDelight's own migration path: `.sqm` files are applied in order between
                        // the version recorded in the file and the one this build was compiled with.
                        wrapConnection(connection) { schema.migrate(it, from.toLong(), to.toLong()) }
                    },
                    extendedConfig =
                        DatabaseConfiguration.Extended(
                            basePath = directory,
                            synchronousFlag = SynchronousFlag.NORMAL,
                            foreignKeyConstraints = true,
                        ),
                )
            val driver = NativeSqliteDriver(configuration)
            return PlayerStore(driver, PlayerDatabase(driver))
        }
    }
}
