package org.basaltmc.assets

import org.basaltmc.io.FileIO
import org.basaltmc.text.LangTable

/** Extracts a locale's lang JSON from the cached `client.jar`: the only place these ship, they're never in `server.jar`. */
public object LangAssets {
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
