package org.basaltmc.assets

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO

/**
 * The real `data/minecraft/...` datapack entries the official server ships, unpacked into
 * [dataRoot] once and read as plain files from then on.
 *
 * The download is unpacked exactly once, and the archive it came from is deleted immediately
 * afterwards: what Basalt needs is a few megabytes of JSON and NBT out of a download several times
 * that size, and keeping the archive around meant re-reading and re-inflating all of it on every
 * boot just to reach them. [source] is a lambda, not a path, so a warm install never even resolves
 * where the download would come from, let alone contacts the network.
 *
 * Files land directly under [dataRoot] with the vanilla `data/minecraft/` prefix stripped
 * (`worldgen/noise/continentalness.json`, not `data/data/minecraft/worldgen/...`), while callers
 * keep using the full vanilla entry names. Anything outside the `minecraft` namespace keeps its
 * full path, so a non-vanilla namespace could not collide with a vanilla one.
 *
 * This class only ever reads entries already present in the official download: it has no embedded
 * values of its own, matching [org.basaltmc.registry.tags.TagFileJson]/`NoiseParametersJson`'s own
 * pattern for real datapack content elsewhere in this project. Unpacked files live in the same
 * gitignored cache and are never redistributed.
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
        entryNames =
            if (FileIO.readText(marker)?.trim() == versionId) {
                readIndex()
            } else {
                unpack(source(), versionId).also { names ->
                    FileIO.writeText("$dataRoot/.index", names.joinToString("\n"))
                    // Written last, so an interrupted unpack is redone rather than trusted.
                    FileIO.writeText(marker, versionId)
                }
            }
    }

    /** Reads one entry's raw bytes (e.g. `data/minecraft/worldgen/noise/continentalness.json`), or `null` if absent. */
    public fun readEntry(name: String): ByteArray? = FileIO.readBytes(pathFor(name))

    /** Reads and UTF-8 decodes one entry, throwing if it's absent: for datapack JSON callers expect to always exist. */
    public fun readText(name: String): String =
        readEntry(name)?.decodeToString() ?: throw AssetException("'$name' is missing from the ${dataRoot} data")

    /**
     * Every entry name under [prefix]: for enumerating a whole datapack directory (e.g. every
     * json file under `data/minecraft/worldgen/configured_feature/`) without knowing the file
     * list ahead of time. Served from the index written at unpack time, since `basalt-io` has no
     * directory listing.
     */
    public fun entriesUnder(prefix: String): List<String> = entryNames.filter { it.startsWith(prefix) }

    private fun pathFor(name: String): String = "$dataRoot/${name.removePrefix(VANILLA_PREFIX)}"

    private fun readIndex(): List<String> =
        FileIO.readText("$dataRoot/.index")?.lineSequence()?.filter { it.isNotBlank() }?.toList().orEmpty()

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
            if (!name.startsWith(DATA_PREFIX) || name.endsWith('/')) continue
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
        const val VANILLA_PREFIX = "data/minecraft/"
    }
}
