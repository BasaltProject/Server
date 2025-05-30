package org.basaltmc.server.config

import org.basaltmc.common.logging.LogLevel
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO

private const val DEFAULT_TOML = """# Basalt server configuration, generated once with vanilla-parity defaults.
# Delete a value and restart to have it regenerated.
#
# This is one file of several under config/. Options move out of here as their own domain grows a
# file of its own (worlds, permission groups, server list, commands); see the file each one lands
# in for where an option went.

[server]
# Port the server listens on.
port = 25565
# How many players can be connected at once. Vanilla ships 20; this ships 80,
# because 20 is a number from 2010 and nothing in the server cares. What the list
# *shows* is a separate thing, in server_list.toml, since a profile can advertise
# whatever it likes without changing who actually gets in.
max-players = 80
# Verify players against Mojang's session server. Disabling this accepts any
# claimed username with no verification (vanilla's "cracked" mode).
online-mode = true
# Game mode new players join in: survival, creative, adventure or spectator.
gamemode = "survival"

[network]
# Reserved for future network tuning (compression threshold, rate limits).

[performance]
# Count noise evaluations, biome lookups and per-phase timings while generating
# chunks, then log a report once the startup chunks are ready. Diagnostics only:
# it also forces startup generation onto a single thread so the counts are exact,
# so leave it off unless you are measuring.
generation-metrics = false

[plugins]
# Reserved for BSMod loader settings.

[logging]
# One of: trace, debug, info, warn, error.
level = "info"
# Mirror everything the console shows into .basalt/logs/latest.log, archiving the
# previous run as .basalt/logs/archive/<date>-<n>.log.gz on every start.
file = true
# How many days of archived logs to keep. 0 keeps them forever.
keep-days = 14
# Colour the console output. One of: auto, always, never. "auto" colours only when
# writing to a terminal, so redirected output and log files stay clean.
color = "auto"
"""

/** When the console gets ANSI colour. [AUTO] means "only when stdout is a terminal". */
public enum class ColorMode {
    AUTO,
    ALWAYS,
    NEVER,
}

/**
 * Parsed `config/server.toml`. Only the fields Basalt currently reads are exposed; everything else
 * is a documented placeholder in the file itself.
 *
 * What is left here is what has no larger domain of its own. Worlds left for `worlds.toml`, the
 * message of the day for `server_list.toml`, and operators for `groups.toml` plus a column in
 * `players.db`.
 */
public data class BasaltConfig(
    public val port: Int,
    public val maxPlayers: Int,
    public val onlineMode: Boolean,
    public val gamemode: String,
    public val logLevel: LogLevel,
    public val logToFile: Boolean,
    public val logKeepDays: Int,
    public val logColor: ColorMode,
    public val generationMetrics: Boolean,
) {
    public companion object {
        private val DEFAULT =
            BasaltConfig(
                port = 25565,
                maxPlayers = 80,
                onlineMode = true,
                gamemode = "survival",
                logLevel = LogLevel.INFO,
                logToFile = true,
                logKeepDays = 14,
                logColor = ColorMode.AUTO,
                generationMetrics = false,
            )

        /** The file this configuration lives in, inside [ConfigDirectory]. */
        public const val FILE_NAME: String = "server.toml"

        /** The `motd` of a vanilla `server.properties`, for whoever writes the server list file. */
        public fun legacyMotd(legacyPropertiesPath: String = "server.properties"): String? =
            FileIO.readText(legacyPropertiesPath)?.let { parseJavaProperties(it)["motd"] }

        /**
         * Loads `config/server.toml`, generating a commented default on first run.
         *
         * A vanilla `server.properties` sitting next to it has its overlapping keys imported once,
         * so moving an existing vanilla server over is "copy the folder and start". It is an
         * import, not a compatibility layer: nothing is kept in sync afterwards.
         */
        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            legacyPropertiesPath: String = "server.properties",
            logger: Logger = Logger("Config"),
        ): BasaltConfig {
            val path = directory.pathOf(FILE_NAME)
            if (!FileIO.exists(path)) {
                FileIO.createDirectories(directory.root)
                val contents = initialContents(legacyPropertiesPath, logger)
                FileIO.writeText(path, contents)
            }
            val root = directory.load(FILE_NAME, DEFAULT_TOML)
            logger.info { "Loaded configuration from $path." }
            return fromToml(root)
        }

        /** What to write into a freshly created `server.toml`: an imported `server.properties`, or the default. */
        private fun initialContents(
            legacyPropertiesPath: String,
            logger: Logger,
        ): String {
            val legacy = FileIO.readText(legacyPropertiesPath)
            if (legacy != null) {
                logger.info { "Found $legacyPropertiesPath, importing recognized values once." }
                return mergeLegacyProperties(DEFAULT_TOML, parseJavaProperties(legacy))
            }
            return DEFAULT_TOML
        }

        private fun fromToml(root: TomlValue.Table): BasaltConfig {
            val server = root.tableOrNull("server")
            val logging = root.tableOrNull("logging")
            val performance = root.tableOrNull("performance")
            return BasaltConfig(
                port = server?.intOrNull("port")?.toInt() ?: DEFAULT.port,
                maxPlayers = server?.intOrNull("max-players")?.toInt() ?: DEFAULT.maxPlayers,
                onlineMode = server?.boolOrNull("online-mode") ?: DEFAULT.onlineMode,
                gamemode = server?.stringOrNull("gamemode") ?: DEFAULT.gamemode,
                logLevel = logging?.stringOrNull("level")?.let(::parseLogLevel) ?: DEFAULT.logLevel,
                logToFile = logging?.boolOrNull("file") ?: DEFAULT.logToFile,
                logKeepDays = logging?.intOrNull("keep-days")?.toInt() ?: DEFAULT.logKeepDays,
                logColor = logging?.stringOrNull("color")?.let(::parseColorMode) ?: DEFAULT.logColor,
                generationMetrics =
                    performance?.boolOrNull("generation-metrics") ?: DEFAULT.generationMetrics,
            )
        }

        private fun parseColorMode(name: String): ColorMode =
            ColorMode.entries.find { it.name.equals(name, ignoreCase = true) } ?: DEFAULT.logColor

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
            legacy["max-players"]?.toIntOrNull()?.let {
                result = result.replace("max-players = 80", "max-players = $it")
            }
            legacy["online-mode"]?.toBooleanStrictOrNull()?.let {
                result = result.replace("online-mode = true", "online-mode = $it")
            }
            legacy["view-distance"]?.toIntOrNull()?.let {
                result = result.replace("view-distance = 10", "view-distance = $it")
            }
            return result
        }
    }
}
