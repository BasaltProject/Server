package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test

/** The frozen Kotlin as the oracle for what a plugin's folder holds and what the loader finds. */
class PluginPackageOracle {
    private val root = "build/test-data/oracle_plugins"

    @AfterTest
    fun cleanUp() {
        wipe(root)
    }

    /** No recursive delete in `wire-io`, and a test is not the place to add one. */
    private fun wipe(path: String) {
        if (FileIO.directoryExists(path)) {
            for (name in FileIO.list(path)) wipe("$path/$name")
        }
        FileIO.delete(path)
    }

    private fun write(
        path: String,
        text: String,
    ) {
        FileIO.createDirectories(path.substringBeforeLast('/'))
        FileIO.writeText(path, text)
    }

    private val manifestText =
        """
        id = "arena"
        version = "1.0.0"
        api = 1

        [resources]
        # A pattern with a leading slash, which is the same pattern.
        private = ["/templates/", "seed.json"]
        """.trimIndent()

    @Test
    fun dump() {
        wipe(root)

        // A folder plugin, with resources of every shape the copier has to keep.
        write("$root/arena/plugin.toml", manifestText)
        write("$root/arena/src/main.lua", "return {}")
        write("$root/arena/src/util.lua", "return {}")
        write("$root/arena/resources/config.toml", "balance = 100")
        write("$root/arena/resources/lang/en_us.json", "{}")
        write("$root/arena/resources/templates/map.json", "{}")
        write("$root/arena/resources/seed.json", "{}")
        // A name the private 'seed.json' must not swallow: it is a file name, not a prefix.
        write("$root/arena/resources/seed.json.bak", "{}")
        // A directory with no manifest is not a plugin, and neither is a loose file.
        write("$root/notes/readme.txt", "hello")
        write("$root/stray.txt", "hello")

        val packages = discoverPackages(root)
        println(
            "ORACLE discovered ${packages.map { "${it.origin.removePrefix("$root/")}/${it.editable}" }}",
        )

        val folder = packages.single() as FolderPackage
        println("ORACLE working ${folder.workingFolder?.removePrefix("$root/")}")
        println("ORACLE listAll ${folder.list("")}")
        println("ORACLE listSrc ${folder.list(PackageLayout.SOURCE)}")
        println("ORACLE listResources ${folder.list(PackageLayout.RESOURCES)}")
        println("ORACLE listSlashed ${folder.list("src")}")
        println("ORACLE read ${folder.readText("resources/config.toml")} missing=${folder.readText("nope")}")
        println("ORACLE exists ${folder.exists("src/main.lua")}/${folder.exists("src/nope.lua")}")

        val manifest = PluginManifest.parse(manifestText)
        // A folder plugin is its own working folder, so nothing is copied next to it.
        val written = mutableListOf<String>()
        PluginFolder(folder, manifest, "$root/arena-out").materialize { written += it.removePrefix("wrote $root/") }
        println("ORACLE materializeFolder ${written.sorted()}")

        // The live folder of a built package, which is what the copier really does. Faked here by
        // pointing the same reader at a destination that is not its own.
        val live = "$root/live"
        write("$live/config.toml", "balance = 999")
        val copied = mutableListOf<String>()
        PluginFolder(NotEditable(folder), manifest, live).materialize { copied += it.removePrefix("wrote $live/") }
        println("ORACLE materializePackage ${copied.sorted()}")
        // The file that was already there is untouched: an update never overwrites what was edited.
        println("ORACLE untouched ${FileIO.readText("$live/config.toml")}")

        val index = PluginFolder(folder, manifest, live).index()
        println("ORACLE index ${index.keys.sorted()}")

        // Two files addressed by the same name, at different depths.
        write("$live/nested/config.toml", "clash = true")
        val clash =
            try {
                PluginFolder(folder, manifest, live).index()
                "accepted"
            } catch (failure: PluginLoadException) {
                failure.message ?: "unknown"
            }
        println("ORACLE clash ${clash.replace(' ', '_').replace(root, "ROOT")}")

        // The same rule for json, which is addressed by name too.
        FileIO.delete("$live/nested/config.toml")
        write("$live/nested/en_us.json", "{}")
        val jsonClash =
            try {
                PluginFolder(folder, manifest, live).index()
                "accepted"
            } catch (failure: PluginLoadException) {
                failure.message ?: "unknown"
            }
        println("ORACLE clashJson ${jsonClash.replace(' ', '_').replace(root, "ROOT")}")

        // An empty plugins directory is made rather than refused.
        val fresh = "$root/fresh"
        println("ORACLE freshEmpty ${discoverPackages(fresh).size} made=${FileIO.directoryExists(fresh)}")
    }
}

/** The same files, saying it is a built package: what a `.bspkg` looks like to the copier. */
private class NotEditable(
    private val inner: PluginPackage,
) : PluginPackage {
    override val origin: String get() = inner.origin
    override val editable: Boolean get() = false
    override val workingFolder: String? get() = null

    override fun read(path: String): ByteArray? = inner.read(path)

    override fun list(prefix: String): List<String> = inner.list(prefix)
}
