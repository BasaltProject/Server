package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO

/**
 * The folder one plugin owns, `plugins/<id>/`, and what the package puts in it.
 *
 * No shape is imposed: whatever the package has under `resources/`, at whatever depth, appears here
 * with the same shape. A layout forced on everyone to suit a few is how a folder ends up with three
 * empty directories in it.
 *
 * A file already there is left alone, so an update brings new files and never touches what the
 * operator edited.
 */
public class PluginFolder(
    private val pkg: PluginPackage,
    private val manifest: PluginManifest,
    private val root: String,
) {
    /** Puts the package's `resources/` into the folder, keeping its shape. */
    public fun materialize(report: (String) -> Unit) {
        FileIO.createDirectories(root)
        // A folder being worked on is already its own folder: copying it next to itself would give
        // the plugin two of every file, which is exactly what the one-name rule below forbids.
        if (pkg.workingFolder != null) return
        for (path in pkg.list(PackageLayout.RESOURCES)) {
            val relative = path.removePrefix(PackageLayout.RESOURCES)
            if (isPrivate(relative)) continue
            val live = "$root/$relative"
            if (FileIO.exists(live)) continue
            val bytes = pkg.read(path) ?: continue
            FileIO.createDirectories(live.substringBeforeLast('/'))
            FileIO.writeBytes(live, bytes)
            report("wrote $live")
        }
    }

    /** Whether the package asked for this one to stay inside it. */
    private fun isPrivate(relative: String): Boolean =
        manifest.privateResources.any { pattern ->
            val clean = pattern.removePrefix("/")
            if (clean.endsWith("/")) relative.startsWith(clean) else relative == clean
        }

    /**
     * Every file in the folder, by path.
     *
     * The addressable ones are checked for a repeated name here rather than where they are used:
     * `config.chat` means `chat.toml` wherever it is, so two of them is a question with no answer,
     * and finding that out at load with both paths named beats finding out from the wrong value.
     */
    public fun index(): Map<String, String> {
        val byPath = walk(root)
        val addressable = byPath.filter { it.substringAfterLast('.', "") in ADDRESSED_BY_NAME }
        val clashes =
            addressable
                .groupBy { it.substringAfterLast('/') }
                .filterValues { it.size > 1 }
        if (clashes.isNotEmpty()) {
            val (name, paths) = clashes.entries.first()
            throw PluginLoadException(
                "two files are called '$name' (${paths.joinToString(" and ")}), and one name has to mean one file",
            )
        }
        return byPath.associateBy { it.removePrefix("$root/") }
    }

    private fun walk(directory: String): List<String> {
        if (!FileIO.directoryExists(directory)) return emptyList()
        val out = mutableListOf<String>()
        for (name in FileIO.list(directory).sorted()) {
            val path = "$directory/$name"
            if (FileIO.directoryExists(path)) out += walk(path) else out += path
        }
        return out
    }

    private companion object {
        /** Extensions whose file name is an address: those are the ones that have to be unique. */
        val ADDRESSED_BY_NAME = setOf("toml", "json")
    }
}
