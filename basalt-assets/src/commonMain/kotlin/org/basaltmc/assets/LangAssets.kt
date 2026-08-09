package org.basaltmc.assets

import org.basaltmc.io.FileIO
import org.basaltmc.text.LangTable

/** Extracts a locale's lang JSON from the cached `client.jar`: the only place these ship, they're never in `server.jar`. */
public object LangAssets {
    /**
     * The locale table for [versionDir], or an empty one if it cannot be had without going to the
     * network.
     *
     * Reads an already-extracted `lang/<locale>.json` if there is one, otherwise extracts it from a
     * `client.jar` that is already in the cache and leaves it extracted so the next boot does not
     * reopen a jar of tens of megabytes. Deliberately never downloads: the server only fetches
     * `server.jar` on its own, and the fallback (an empty table, so translation keys print as
     * themselves) is a cosmetic loss in the console, not a broken server.
     */
    public fun loadCachedOrEmpty(
        versionDir: String,
        locale: String = "en_us",
    ): LangTable {
        val extracted = "$versionDir/lang/$locale.json"
        FileIO.readText(extracted)?.let { return LangTable.parse(it) }
        val clientJar = "$versionDir/client.jar"
        if ((FileIO.size(clientJar) ?: 0L) <= 0L) return LangTable.EMPTY
        val jarBytes = FileIO.readBytes(clientJar) ?: return LangTable.EMPTY
        val entryBytes = ZipReader(jarBytes).readEntry("assets/minecraft/lang/$locale.json") ?: return LangTable.EMPTY
        FileIO.createDirectories("$versionDir/lang")
        FileIO.writeBytes(extracted, entryBytes)
        return LangTable.parse(entryBytes.decodeToString())
    }

    public fun load(
        clientJarPath: String,
        locale: String = "en_us",
    ): LangTable {
        val jarBytes = FileIO.readBytes(clientJarPath) ?: throw AssetException("client.jar not found at $clientJarPath")
        val entryName = "assets/minecraft/lang/$locale.json"
        val entryBytes =
            ZipReader(jarBytes).readEntry(entryName)
                ?: throw AssetException("'$entryName' not found in $clientJarPath")
        return LangTable.parse(entryBytes.decodeToString())
    }
}
