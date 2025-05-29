package org.basaltmc.assets

import org.basaltmc.io.FileIO
import org.basaltmc.text.LangTable

/**
 * The official language table, which is what lets the server console print the same messages it
 * sends to clients as sentences rather than as translation keys.
 *
 * A client resolves `commands.gamemode.success.self` against its own language; nothing does that
 * for a log file, so the server has to. The file comes out of the **server** jar, under
 * `assets/minecraft/lang/`: it ships there as well as in `client.jar`, so no extra download is
 * needed (only the outer bundler archive lacks it, which is what made an earlier version of this
 * class reach for `client.jar` instead). [ServerJarAssets] unpacks it alongside the datapack data.
 */
public object LangAssets {
    /** Extracts a locale's lang JSON straight from a jar, for a jar that has not been unpacked. */
    public fun load(
        jarPath: String,
        locale: String = "en_us",
    ): LangTable {
        val jarBytes = FileIO.readBytes(jarPath) ?: throw AssetException("jar not found at $jarPath")
        val entryName = "$LANG_PREFIX$locale.json"
        val entryBytes =
            ZipReader(jarBytes).readEntry(entryName)
                ?: throw AssetException("'$entryName' not found in $jarPath")
        return LangTable.parse(entryBytes.decodeToString())
    }

    /**
     * The locale table [ServerJarAssets] unpacked into [versionDir], or an empty one if it is not
     * there.
     *
     * Empty is a cosmetic loss (the console prints keys), never a failure, so a cache written
     * before language files were unpacked degrades instead of refusing to start.
     */
    public fun loadCachedOrEmpty(
        versionDir: String,
        locale: String = "en_us",
    ): LangTable {
        val path = "$versionDir/$LANG_PREFIX$locale.json"
        return FileIO.readText(path)?.let { LangTable.parse(it) } ?: LangTable.EMPTY
    }

    private const val LANG_PREFIX = "assets/minecraft/lang/"
}
