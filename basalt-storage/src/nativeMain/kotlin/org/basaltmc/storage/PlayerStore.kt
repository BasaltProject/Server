package org.basaltmc.storage

import app.cash.sqldelight.db.QueryResult
import app.cash.sqldelight.db.SqlDriver
import app.cash.sqldelight.driver.native.NativeSqliteDriver
import app.cash.sqldelight.driver.native.wrapConnection
import co.touchlab.sqliter.DatabaseConfiguration
import co.touchlab.sqliter.JournalMode
import co.touchlab.sqliter.SynchronousFlag
import org.basaltmc.io.FileIO
import org.basaltmc.storage.db.PlayerDatabase

/** Thrown when `players.db` was written by a build with a different schema, since there are no migrations. */
public class PlayerDatabaseSchemaException(
    message: String,
) : Exception(message)

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

    /**
     * False once [close] has run. Callers that may still be winding down on their own threads check
     * it instead of finding out through the driver, which reports a use-after-close as a bare
     * `IllegalStateException: Check failed` that says nothing about what happened.
     */
    public var isOpen: Boolean = true
        private set

    /**
     * Folds the write-ahead log back into the database file, then closes.
     *
     * The checkpoint is not optional, which is worth stating because the opposite looks true:
     * SQLite is documented to checkpoint when the last connection closes, but measured here it does
     * not happen through this driver. Without this call a clean shutdown left `players.db` at 4 KB
     * with every row sitting in a 24 KB `-wal` beside it. Nothing is lost (any SQLite reader
     * replays the log) but the database is only complete as a *set* of files, so copying
     * `players.db` on its own for a backup would silently copy an empty database.
     *
     * It goes through `executeQuery` rather than `execute` because `PRAGMA wal_checkpoint` answers
     * with a row, and the driver refuses to run a row-returning statement as a plain execute.
     */
    override fun close() {
        isOpen = false
        runCatching {
            driver.executeQuery(null, "PRAGMA wal_checkpoint(TRUNCATE)", { QueryResult.Value(Unit) }, 0)
        }
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
        private const val SCHEMA_CHECK_FILE = "basalt_schema_check.db"

        public fun open(path: String): PlayerStore {
            // The driver takes a directory and a file name, not a path, so a caller can keep
            // thinking in paths: `players.db` next to the server, `worlds/` beside it.
            val separator = path.lastIndexOf('/')
            val directory = if (separator >= 0) path.substring(0, separator).ifEmpty { "/" } else "."
            val fileName = if (separator >= 0) path.substring(separator + 1) else path
            // A configured path whose directory does not exist yet should work, not fail: the
            // driver opens the file but will not create the folder around it.
            FileIO.createDirectories(directory)

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
            verifySchema(driver, path, directory)
            return PlayerStore(driver, PlayerDatabase(driver))
        }

        /**
         * Refuses to run against a database this build cannot actually use.
         *
         * There are no migrations on purpose: nobody is running Basalt yet, so a schema change is
         * made cleanly and the old file is deleted. What must not happen is what did happen once,
         * which is that the mismatch stayed invisible until the first query that touched a new
         * column, and surfaced as a native stack trace in the middle of answering a server list
         * ping. Wrong at startup, by name, beats wrong later, anywhere.
         *
         * The expected columns come from creating the schema in memory rather than from a list
         * written out here, so this check cannot fall behind the `.sq` files it is checking.
         */
        private fun verifySchema(
            driver: SqlDriver,
            path: String,
            directory: String,
        ) {
            val expected = referenceColumns(directory)
            for ((table, columns) in expected) {
                val actual = columnsOf(driver, table)
                val missing = columns - actual
                if (missing.isEmpty()) continue
                throw PlayerDatabaseSchemaException(
                    "$path was written by an older build: its '$table' table has no " +
                        "${missing.sorted().joinToString(", ")} column. Basalt has no database " +
                        "migrations yet, so delete $path (and its -wal and -shm files) and start again. " +
                        "Players will be recreated on their next login.",
                )
            }
        }

        /**
         * The tables and columns a freshly created database has, read back from a throwaway one.
         *
         * A scratch file rather than sqliter's `inMemory`, which does not open memory through this
         * driver: it builds a `?mode=memory&cache=shared` *name* that gets opened as a plain file,
         * so the first version of this check quietly created a file with that name in the working
         * directory (and committed two of them). Deleted in a `finally`, and named `.db` so the
         * ignore rules already cover it if a crash ever leaves one behind.
         */
        private fun referenceColumns(directory: String): Map<String, Set<String>> {
            val schema = PlayerDatabase.Schema
            val reference =
                NativeSqliteDriver(
                    DatabaseConfiguration(
                        name = SCHEMA_CHECK_FILE,
                        version = schema.version.toInt(),
                        journalMode = JournalMode.WAL,
                        create = { connection -> wrapConnection(connection) { schema.create(it) } },
                        upgrade = { _, _, _ -> },
                        extendedConfig = DatabaseConfiguration.Extended(basePath = directory),
                    ),
                )
            return try {
                tablesOf(reference).associateWith { columnsOf(reference, it) }
            } finally {
                reference.close()
                for (suffix in listOf("", "-wal", "-shm")) FileIO.delete("$directory/$SCHEMA_CHECK_FILE$suffix")
            }
        }

        private fun tablesOf(driver: SqlDriver): List<String> =
            driver
                .executeQuery(null, "SELECT name FROM sqlite_master WHERE type = 'table'", { cursor ->
                    val names = mutableListOf<String>()
                    while (cursor.next().value) cursor.getString(0)?.let { names += it }
                    QueryResult.Value(names)
                }, 0)
                .value
                .filterNot { it.startsWith("sqlite_") }

        private fun columnsOf(
            driver: SqlDriver,
            table: String,
        ): Set<String> =
            driver
                .executeQuery(null, "PRAGMA table_info($table)", { cursor ->
                    val names = mutableSetOf<String>()
                    while (cursor.next().value) cursor.getString(1)?.let { names += it }
                    QueryResult.Value(names)
                }, 0)
                .value
    }
}
