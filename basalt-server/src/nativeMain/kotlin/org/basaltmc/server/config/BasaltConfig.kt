package org.basaltmc.server.config

import org.basaltmc.common.logging.LogLevel
import org.basaltmc.io.FileIO

private const val DEFAULT_TOML = """# Basalt server configuration, generated once with vanilla-parity defaults.
# Delete a value and restart to have it regenerated.

[server]
# Port the server listens on.
port = 25565
# Message of the day, shown in the server list.
motd = "A Basalt Server"
# Maximum concurrent players.
max-players = 20
# Verify players against Mojang's session server. Disabling this accepts any
# claimed username with no verification (vanilla's "cracked" mode).
online-mode = true

[network]
# Reserved for future network tuning (compression threshold, rate limits).

[world]
# Name of the default world.
name = "world"

[performance]
# Reserved for future tick/performance tuning.

[plugins]
# Reserved for PSMod loader settings.

[logging]
# One of: trace, debug, info, warn, error.
level = "info"
"""

/** Parsed `basalt.toml`. Only the fields Basalt currently reads are exposed; everything else is a documented placeholder in the file itself. */
public data class BasaltConfig(
    public val port: Int,
    public val motd: String,
    public val maxPlayers: Int,
    public val onlineMode: Boolean,
    public val worldName: String,
    public val logLevel: LogLevel,
) {
    public companion object {
        private val DEFAULT =
            BasaltConfig(
                port = 25565,
                motd = "A Basalt Server",
                maxPlayers = 20,
                onlineMode = true,
                worldName = "world",
                logLevel = LogLevel.INFO,
            )

        /**
         * Loads `basalt.toml` from [path], generating a commented default on first run. If
         * `server.properties` exists and `basalt.toml` does not, imports the overlapping keys once;
         * it is not kept in sync afterward.
         */
        public fun load(
            path: String = "basalt.toml",
            legacyPropertiesPath: String = "server.properties",
        ): BasaltConfig {
            if (!FileIO.exists(path)) {
                val legacy = FileIO.readText(legacyPropertiesPath)
                if (legacy != null) {
                    println("Found $legacyPropertiesPath, importing recognized values into $path once.")
                    FileIO.writeText(path, mergeLegacyProperties(DEFAULT_TOML, parseJavaProperties(legacy)))
                } else {
                    FileIO.writeText(path, DEFAULT_TOML)
                }
            }
            val text = FileIO.readText(path) ?: throw TomlFormatException("Failed to read $path after writing it")
            return fromToml(Toml.parse(text))
        }

        private fun fromToml(root: TomlValue.Table): BasaltConfig {
            val server = root.tableOrNull("server")
            val world = root.tableOrNull("world")
            val logging = root.tableOrNull("logging")
            return BasaltConfig(
                port = server?.intOrNull("port")?.toInt() ?: DEFAULT.port,
                motd = server?.stringOrNull("motd") ?: DEFAULT.motd,
                maxPlayers = server?.intOrNull("max-players")?.toInt() ?: DEFAULT.maxPlayers,
                onlineMode = server?.boolOrNull("online-mode") ?: DEFAULT.onlineMode,
                worldName = world?.stringOrNull("name") ?: DEFAULT.worldName,
                logLevel = logging?.stringOrNull("level")?.let(::parseLogLevel) ?: DEFAULT.logLevel,
            )
        }

        private fun parseLogLevel(name: String): LogLevel =
            LogLevel.entries.find { it.name.equals(name, ignoreCase = true) } ?: DEFAULT.logLevel

        private fun parseJavaProperties(text: String): Map<String, String> =
            text
                .lineSequence()
                .map { it.trim() }
                .filter { it.isNotEmpty() && !it.startsWith("#") && !it.startsWith("!") }
                .mapNotNull { line ->
                    val separator = line.indexOfFirst { it == '=' || it == ':' }
                    if (separator < 0) return@mapNotNull null
                    line.substring(0, separator).trim() to line.substring(separator + 1).trim()
                }.toMap()

        private fun mergeLegacyProperties(
            template: String,
            legacy: Map<String, String>,
        ): String {
            var result = template
            legacy["server-port"]?.toIntOrNull()?.let {
                result = result.replace("port = 25565", "port = $it")
            }
            legacy["motd"]?.let {
                val escaped = it.replace("\"", "\\\"")
                result = result.replace("motd = \"A Basalt Server\"", "motd = \"$escaped\"")
            }
            legacy["max-players"]?.toIntOrNull()?.let {
                result = result.replace("max-players = 20", "max-players = $it")
            }
            legacy["online-mode"]?.toBooleanStrictOrNull()?.let {
                result = result.replace("online-mode = true", "online-mode = $it")
            }
            return result
        }
    }
}
