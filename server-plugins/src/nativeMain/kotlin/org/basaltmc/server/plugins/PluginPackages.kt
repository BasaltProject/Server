package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.io.ZipFormatException
import org.basaltmc.wire.io.ZipReader

/** A plugin being worked on: a folder under `plugins/`, read from disk every time. */
public class FolderPackage(
    private val root: String,
) : PluginPackage {
    override val origin: String get() = root

    override val editable: Boolean get() = true

    override val workingFolder: String get() = "$root/${PackageLayout.RESOURCES.trimEnd('/')}"

    override fun read(path: String): ByteArray? = FileIO.readBytes("$root/$path")

    override fun list(prefix: String): List<String> {
        val out = mutableListOf<String>()
        walk(prefix.trimEnd('/'), out)
        return out.sorted()
    }

    private fun walk(
        relative: String,
        into: MutableList<String>,
    ) {
        val absolute = if (relative.isEmpty()) root else "$root/$relative"
        if (!FileIO.directoryExists(absolute)) return
        for (name in FileIO.list(absolute)) {
            val child = if (relative.isEmpty()) name else "$relative/$name"
            if (FileIO.directoryExists("$root/$child")) walk(child, into) else into += child
        }
    }
}

/** A built package: one zip, read once, entries pulled out as they are asked for. */
public class ArchivePackage(
    private val path: String,
    bytes: ByteArray,
) : PluginPackage {
    private val zip = ZipReader(bytes)

    override val origin: String get() = path

    override val editable: Boolean get() = false

    override fun read(path: String): ByteArray? =
        try {
            zip.readEntry(path)
        } catch (e: ZipFormatException) {
            throw PluginLoadException("${this.path} is damaged: ${e.message}")
        }

    override fun list(prefix: String): List<String> =
        zip.entryNames
            .filter { it.startsWith(prefix) && !it.endsWith("/") }
            .sorted()
}

/** Something the loader could not get past, said in one line that names the package. */
public class PluginLoadException(
    message: String,
) : Exception(message)

/**
 * What is sitting in `plugins/`, folders and packages alike.
 *
 * A folder and a package with the same id is not resolved by picking one: it is refused, because
 * guessing which one the operator meant is how a server ends up running last week's code.
 */
public fun discoverPackages(directory: String): List<PluginPackage> {
    if (!FileIO.directoryExists(directory)) return emptyList()
    val found = mutableListOf<PluginPackage>()
    for (name in FileIO.list(directory).sorted()) {
        val path = "$directory/$name"
        when {
            FileIO.directoryExists(path) ->
                if (FileIO.exists("$path/${PackageLayout.MANIFEST}")) found += FolderPackage(path)

            name.endsWith(PackageLayout.EXTENSION) -> {
                val bytes =
                    FileIO.readBytes(path)
                        ?: throw PluginLoadException("$path could not be read")
                found += ArchivePackage(path, bytes)
            }
        }
    }
    return found
}
