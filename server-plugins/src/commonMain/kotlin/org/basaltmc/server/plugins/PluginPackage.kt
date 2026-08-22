package org.basaltmc.server.plugins

/**
 * The files of one plugin, however they are stored.
 *
 * A packed `.bspkg` and an unpacked folder are the same thing to everything above this: the folder
 * form exists so that iterating on a plugin is saving a file, not building a zip.
 */
public interface PluginPackage {
    /** Where it came from, for log lines that have to be actionable. */
    public val origin: String

    /** Whether this is a folder being edited rather than a built package. */
    public val editable: Boolean

    /**
     * Where this plugin's own files already are, for a folder being worked on.
     *
     * A folder package is its own working folder: its `resources/` is what a built package would
     * have been copied out to, so it is used where it lies instead of being copied next to itself.
     * Null for a built package, which gets `plugins/<id>/`.
     */
    public val workingFolder: String?
        get() = null

    /** The bytes at [path], or null when there is nothing there. Paths are package-relative. */
    public fun read(path: String): ByteArray?

    /** Every path under [prefix], package-relative, in a stable order. */
    public fun list(prefix: String): List<String>

    public fun readText(path: String): String? = read(path)?.decodeToString()

    public fun exists(path: String): Boolean = read(path) != null
}

/** Where the parts of a package live. One place, so nothing else has to spell these out. */
public object PackageLayout {
    public const val MANIFEST: String = PluginManifest.FILE_NAME
    public const val SOURCE: String = "src/"
    public const val RESOURCES: String = "resources/"
    public const val CONFIG_FILE: String = "resources/config.toml"
    public const val CONFIG_DIRECTORY: String = "resources/config/"
    public const val LANG: String = "resources/lang/"
    public const val ASSETS: String = "resources/assets/"

    /** The extension of a built package. */
    public const val EXTENSION: String = ".bspkg"
}
