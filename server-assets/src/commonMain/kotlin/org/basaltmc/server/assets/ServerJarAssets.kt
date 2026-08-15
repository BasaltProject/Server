package org.basaltmc.server.assets

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO

/**
 * The official server's `data/minecraft/...` entries and its language files, unpacked into
 * [dataRoot] once and read as plain files from then on. The archive is deleted right after:
 * keeping it meant re-inflating a much larger download on every boot for a few megabytes.
 * [source] is a lambda so a warm install never resolves it, let alone contacts the network.
 *
 * Vanilla entries lose the `data/minecraft/` prefix while callers keep the full names; other
 * namespaces keep their path and cannot collide. `assets/minecraft/lang/` comes out too, since the
 * console resolves the same translation keys it sends clients (see [LangAssets]).
 */
public class ServerJarAssets(
    versionId: String,
    private val dataRoot: String,
    private val logger: Logger = Logger("Assets"),
    source: () -> String,
) {
    private val entryNames: List<String>

    init {
        val marker = "$dataRoot/.unpacked"
        // The marker carries what was unpacked, not just which version: widening the set of
        // entries taken has to invalidate a cache that was written under the narrower one, or an
        // existing install silently keeps missing whatever was added.
        val stamp = "$versionId $UNPACK_FORMAT"
        entryNames =
            if (FileIO.readText(marker)?.trim() == stamp) {
                readIndex()
            } else {
                unpack(source(), versionId).also { names ->
                    FileIO.writeText("$dataRoot/.index", names.joinToString("\n"))
                    // Written last, so an interrupted unpack is redone rather than trusted.
                    FileIO.writeText(marker, stamp)
                }
            }
    }

    /** Reads one entry's raw bytes (e.g. `data/minecraft/worldgen/noise/continentalness.json`), or `null` if absent. */
    public fun readEntry(name: String): ByteArray? = FileIO.readBytes(pathFor(name))

    /** Reads and UTF-8 decodes one entry, throwing if it's absent: for datapack JSON callers expect to always exist. */
    public fun readText(name: String): String =
        readEntry(name)?.decodeToString() ?: throw AssetException("'$name' is missing from the $dataRoot data")

    /**
     * Every entry name under [prefix]: for enumerating a whole datapack directory (e.g. every
     * json file under `data/minecraft/worldgen/configured_feature/`) without knowing the file
     * list ahead of time. Served from the index written at unpack time, since `wire-io` has no
     * directory listing.
     */
    public fun entriesUnder(prefix: String): List<String> = entryNames.filter { it.startsWith(prefix) }

    /**
     * Every id under `data/<namespace>/<directory>/`, which is how one registry directory is
     * listed: what a command completes a registry argument with.
     */
    public fun dataIds(
        directory: String,
        extension: String,
    ): List<Identifier> =
        entriesUnder("data/").mapNotNull { entry ->
            val rest = entry.removePrefix("data/")
            val namespace = rest.substringBefore('/', "")
            val path = rest.removePrefix("$namespace/$directory/")
            if (namespace.isEmpty() || path == rest || !path.endsWith(extension)) {
                null
            } else {
                Identifier(namespace, path.removeSuffix(extension))
            }
        }

    private fun pathFor(name: String): String = "$dataRoot/${name.removePrefix(VANILLA_PREFIX)}"

    private fun readIndex(): List<String> =
        FileIO
            .readText("$dataRoot/.index")
            ?.lineSequence()
            ?.filter { it.isNotBlank() }
            ?.toList()
            .orEmpty()

    private fun unpack(
        archivePath: String,
        versionId: String,
    ): List<String> {
        val outerBytes =
            FileIO.readBytes(archivePath) ?: throw AssetException("$versionId data not found at $archivePath")
        // Since 1.18 the published download is a bundler wrapping the real server archive one level
        // down (confirmed against the real 26.2 download).
        val innerEntryName = "META-INF/versions/$versionId/server-$versionId.jar"
        val innerBytes =
            ZipReader(outerBytes).readEntry(innerEntryName)
                ?: throw AssetException("$archivePath does not contain $versionId's data where expected")

        logger.info { "Unpacking $versionId data into $dataRoot..." }
        val inner = ZipReader(innerBytes)
        val unpacked = mutableListOf<String>()
        for (name in inner.entryNames) {
            if (name.endsWith('/')) continue
            if (!name.startsWith(DATA_PREFIX) && !name.startsWith(LANG_PREFIX)) continue
            val bytes = inner.readEntry(name) ?: continue
            val path = pathFor(name)
            FileIO.createDirectories(path.substringBeforeLast('/'))
            FileIO.writeBytes(path, bytes)
            unpacked += name
        }
        FileIO.createDirectories(dataRoot)

        // The archive has served its only purpose. Keeping it would leave several times the size of
        // what was extracted sitting on disk forever, for data already on disk in a usable form.
        FileIO.delete(archivePath)
        FileIO.delete("$archivePath.verified")
        logger.info { "Unpacked ${unpacked.size} $versionId data file(s)." }
        return unpacked
    }

    private companion object {
        const val DATA_PREFIX = "data/"
        const val LANG_PREFIX = "assets/minecraft/lang/"
        const val VANILLA_PREFIX = "data/minecraft/"

        // Bumped whenever the set of entries unpacked above changes, so existing caches redo it.
        const val UNPACK_FORMAT = "v2-lang"
    }
}
