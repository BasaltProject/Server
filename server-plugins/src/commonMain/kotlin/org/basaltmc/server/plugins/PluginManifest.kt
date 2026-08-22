package org.basaltmc.server.plugins

import org.basaltmc.wire.common.toml.Toml
import org.basaltmc.wire.common.toml.TomlFormatException
import org.basaltmc.wire.common.toml.TomlValue

/** A manifest that says something the loader cannot work with, named so the author can fix it. */
public class ManifestException(
    message: String,
) : Exception(message)

/** What a plugin may reach beyond its own package. Nothing is granted that is not asked for here. */
public data class PluginCapabilities(
    val storage: Boolean = false,
    val playersData: Boolean = false,
    val commands: Boolean = false,
    val files: List<String> = emptyList(),
    val network: List<String> = emptyList(),
    /**
     * Whether it may see packets.
     *
     * Never granted by asking nicely at runtime: somebody watching packets sees everybody's chat,
     * so it is written in the manifest where an operator reads it before installing anything.
     */
    val packets: Boolean = false,
) {
    public companion object {
        public val NONE: PluginCapabilities = PluginCapabilities()
    }
}

/**
 * What a plugin lets the others reach: its public surface, and nothing else is.
 *
 * Declared by the owner and only by the owner. There is no operator setting that forces the exports
 * of somebody else's plugin, and that is not prudishness: whoever ate the breakage after an update
 * would be the one who wrote neither of the two.
 */
public data class PluginExports(
    /** Files of `src/` another plugin may `require`, without the extension. `["*"]` opens all of it. */
    val modules: List<String> = emptyList(),
    /** Names this plugin may offer through `service(name, table)`. */
    val services: List<String> = emptyList(),
) {
    public val opensEverything: Boolean get() = modules.contains("*")

    public fun exports(module: String): Boolean = opensEverything || modules.contains(module)

    public companion object {
        public val NONE: PluginExports = PluginExports()
    }
}

/** One dependency, and how much of it will do. */
public data class PluginDependency(
    val id: String,
    /** `*`, `1.2.3` or `>=1.2`. Nothing fancier until a plugin needs it. */
    val requirement: String,
    val hard: Boolean,
) {
    public fun accepts(version: String): Boolean =
        when {
            requirement == "*" -> true
            requirement.startsWith(">=") -> compareVersions(version, requirement.removePrefix(">=").trim()) >= 0
            requirement.startsWith(">") -> compareVersions(version, requirement.removePrefix(">").trim()) > 0
            requirement.startsWith("<=") -> compareVersions(version, requirement.removePrefix("<=").trim()) <= 0
            requirement.startsWith("<") -> compareVersions(version, requirement.removePrefix("<").trim()) < 0
            else -> compareVersions(version, requirement.trim()) == 0
        }
}

/**
 * `plugin.toml`, which is the only file the loader reads before deciding whether to load anything.
 *
 * It lives at the root of the package rather than under `resources/` for that reason: knowing
 * whether a package can be loaded should not mean unpacking it.
 */
public data class PluginManifest(
    val id: String,
    val name: String,
    val version: String,
    val authors: List<String>,
    /** The major of the plugin API this was written against. */
    val api: Int,
    /** Path inside the package, `src/main.lua` unless it says otherwise. */
    val main: String,
    val dependencies: List<PluginDependency>,
    val capabilities: PluginCapabilities,
    val exports: PluginExports = PluginExports.NONE,
    /**
     * Paths under `resources/` that stay inside the package instead of being copied into the
     * plugin's folder: templates, seed data, anything the plugin reads and the operator has no
     * business editing.
     */
    val privateResources: List<String>,
) {
    public val hardDependencies: List<PluginDependency> get() = dependencies.filter { it.hard }

    public companion object {
        /** The API major this server speaks. A package asking for a different one does not load. */
        public const val API: Int = 1

        public const val FILE_NAME: String = "plugin.toml"

        private const val DEFAULT_MAIN = "src/main.lua"

        public fun parse(text: String): PluginManifest {
            val table =
                try {
                    Toml.parse(text)
                } catch (e: TomlFormatException) {
                    throw ManifestException("plugin.toml is not valid TOML: ${e.message}")
                }
            val id = table.stringOrNull("id") ?: throw ManifestException("plugin.toml has no 'id'")
            if (!id.matches(ID_SHAPE)) {
                throw ManifestException(
                    "'$id' is not a usable id: lowercase letters, digits and underscores, starting with a letter",
                )
            }
            val version = table.stringOrNull("version") ?: throw ManifestException("$id has no 'version'")
            val api =
                table.intOrNull("api")?.toInt()
                    ?: throw ManifestException("$id does not say which 'api' it was written for")
            return PluginManifest(
                id = id,
                name = table.stringOrNull("name") ?: id,
                version = version,
                authors = table.stringListOrNull("authors").orEmpty(),
                api = api,
                main = table.stringOrNull("main") ?: DEFAULT_MAIN,
                dependencies = dependenciesOf(table),
                capabilities = capabilitiesOf(table.tableOrNull("capabilities")),
                exports = exportsOf(table.tableOrNull("exports")),
                privateResources = table.tableOrNull("resources")?.stringListOrNull("private").orEmpty(),
            )
        }

        private fun dependenciesOf(table: TomlValue.Table): List<PluginDependency> {
            val out = mutableListOf<PluginDependency>()
            for ((key, hard) in listOf("depends" to true, "soft-depends" to false)) {
                val section = table.tableOrNull(key) ?: continue
                for (id in section.keys()) {
                    val requirement = section.stringOrNull(id) ?: "*"
                    out += PluginDependency(id, requirement, hard)
                }
            }
            return out
        }

        private fun exportsOf(table: TomlValue.Table?): PluginExports {
            if (table == null) return PluginExports.NONE
            return PluginExports(
                modules = table.stringListOrNull("modules").orEmpty(),
                services = table.stringListOrNull("services").orEmpty(),
            )
        }

        private fun capabilitiesOf(table: TomlValue.Table?): PluginCapabilities {
            if (table == null) return PluginCapabilities.NONE
            return PluginCapabilities(
                storage = table.boolOrNull("storage") ?: false,
                playersData = table.boolOrNull("players_data") ?: false,
                commands = table.boolOrNull("commands") ?: false,
                files = table.stringListOrNull("files").orEmpty(),
                network = table.stringListOrNull("network").orEmpty(),
                packets = table.boolOrNull("packets") ?: false,
            )
        }

        private val ID_SHAPE = Regex("[a-z][a-z0-9_]*")
    }
}

/** Compares two dotted versions numerically, treating a missing part as zero. */
internal fun compareVersions(
    left: String,
    right: String,
): Int {
    val a = left.split('.', '-')
    val b = right.split('.', '-')
    for (i in 0 until maxOf(a.size, b.size)) {
        val x = a.getOrNull(i)?.toLongOrNull() ?: 0
        val y = b.getOrNull(i)?.toLongOrNull() ?: 0
        if (x != y) return if (x < y) -1 else 1
    }
    return 0
}
