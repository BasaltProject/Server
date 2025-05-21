package org.basaltmc.assets

import org.basaltmc.io.FileIO

/**
 * Reads the real `data/minecraft/...` datapack entries bundled inside the official `server.jar`
 * (cached by [AssetCache] with [AssetCache.Artifact.SERVER], never committed or redistributed).
 * Since 1.18, the public `server.jar` is a bundler wrapping the real server jar at
 * `META-INF/versions/{version}/server-{version}.jar` (confirmed against the real 26.2 download);
 * this unwraps that one extra level before exposing entries, [LangAssets] doesn't need to because
 * `client.jar` was never bundled that way.
 *
 * This class only ever reads entries already present in the jar: it has no embedded values of its
 * own, matching [org.basaltmc.registry.tags.TagFileJson]/`NoiseParametersJson`'s own pattern for
 * real datapack content elsewhere in this project.
 */
public class ServerJarAssets(
    serverJarPath: String,
    versionId: String,
) {
    private val inner: ZipReader

    init {
        val outerBytes =
            FileIO.readBytes(serverJarPath) ?: throw AssetException("server.jar not found at $serverJarPath")
        val outer = ZipReader(outerBytes)
        val innerEntryName = "META-INF/versions/$versionId/server-$versionId.jar"
        val innerBytes =
            outer.readEntry(innerEntryName)
                ?: throw AssetException("'$innerEntryName' not found in $serverJarPath: not a bundler jar?")
        inner = ZipReader(innerBytes)
    }

    /** Reads one entry's raw bytes (e.g. `data/minecraft/worldgen/noise/continentalness.json`), or `null` if absent. */
    public fun readEntry(name: String): ByteArray? = inner.readEntry(name)

    /** Reads and UTF-8 decodes one entry, throwing if it's absent: for datapack JSON callers expect to always exist. */
    public fun readText(name: String): String =
        readEntry(name)?.decodeToString() ?: throw AssetException("'$name' not found in the real server jar")

    /**
     * Every entry name under [prefix]: for enumerating a whole datapack directory (e.g. every
     * json file under `data/minecraft/worldgen/configured_feature/`) without knowing the file
     * list ahead of time.
     */
    public fun entriesUnder(prefix: String): List<String> = inner.entryNames.filter { it.startsWith(prefix) }
}
