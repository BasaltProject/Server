package org.basaltmc.server.world

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import org.basaltmc.io.Gzip
import org.basaltmc.storage.PlayerRepository
import org.basaltmc.storage.VanillaPlayerData
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/** What one import run did, so the command that triggered it can report something truthful. */
public data class ImportReport(
    val imported: Int,
    val skipped: Int,
    val failed: Int,
)

/**
 * Imports the players of an existing vanilla save into `players.db`, starting from
 * `usercache.json` rather than a listing of `playerdata/`: a `.dat` file holds the data but not
 * the name, so walking the directory would import everyone nameless.
 *
 * One-way and idempotent: a second run overwrites the same rows and never writes back into the
 * save it read.
 */
public object VanillaSaveImport {
    @OptIn(ExperimentalTime::class)
    public fun run(
        savePath: String,
        repository: PlayerRepository,
        worldName: String,
        logger: Logger = Logger("Import"),
    ): ImportReport {
        val cachePath = "$savePath/usercache.json"
        val cacheText =
            FileIO.readText(cachePath)
                ?: throw ImportException("$cachePath not found: is $savePath a vanilla server directory?")

        val entries =
            runCatching { Json.parseToJsonElement(cacheText) as JsonArray }
                .getOrElse { throw ImportException("$cachePath is not the JSON array vanilla writes") }

        val now = Clock.System.now().epochSeconds
        var imported = 0
        var skipped = 0
        var failed = 0

        for (element in entries) {
            val entry = runCatching { element.jsonObject }.getOrNull() ?: continue
            val uuid = entry["uuid"]?.jsonPrimitive?.content ?: continue
            val name = entry["name"]?.jsonPrimitive?.content ?: continue

            val dataPath = "$savePath/playerdata/$uuid.dat"
            val bytes = FileIO.readBytes(dataPath)
            if (bytes == null) {
                // Perfectly normal: the cache remembers everyone who ever resolved a name, including
                // players who never actually joined and so have no data file.
                skipped++
                continue
            }

            val player =
                runCatching {
                    VanillaPlayerData.read(
                        uuid = uuid,
                        name = name,
                        gzippedBytes = bytes,
                        decompress = Gzip::decompress,
                        nowEpochSeconds = now,
                        worldName = worldName,
                    )
                }.getOrElse { failure ->
                    // One unreadable file must not abort the other thousand: a save old enough to
                    // need importing is exactly the kind that has a corrupt `.dat` in it.
                    logger.warn { "Skipping $name ($uuid): ${failure.message}" }
                    failed++
                    null
                } ?: continue

            repository.save(player)
            imported++
        }

        logger.info { "Imported $imported player(s) from $savePath ($skipped without data, $failed unreadable)." }
        return ImportReport(imported, skipped, failed)
    }
}

/** The import could not start at all: a wrong path, or a `usercache.json` that is not one. */
public class ImportException(
    message: String,
) : Exception(message)
