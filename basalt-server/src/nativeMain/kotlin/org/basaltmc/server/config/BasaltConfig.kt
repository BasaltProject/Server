package org.basaltmc.server.config

import org.basaltmc.common.logging.LogLevel
import org.basaltmc.common.logging.Logger
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
# Game mode new players join in: survival, creative, adventure or spectator.
gamemode = "survival"
# Player names with operator rights. Vanilla keeps these in ops.json; Basalt keeps
# every setting in this one file. `op <name>` at the console adds a name here,
# `deop <name>` removes it.
operators = []
# Permission level operators get, on vanilla's own 1-4 scale: 4 is full access.
op-permission-level = 4

[network]
# Reserved for future network tuning (compression threshold, rate limits).

[world]
# Name of the default world.
name = "world"
# peaceful, easy, normal or hard. Decides whether hunger can starve you and how
# far down it can take you.
difficulty = "easy"
# Radius, in chunks, sent to a client around its own position (vanilla's own
# server.properties default is 10).
view-distance = 10
# How often changes are written to the region files: a crash costs at most this
# much. 0 disables it, leaving chunks to save when they unload or on stop.
save-interval-seconds = 30

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

/** Parsed `basalt.toml`. Only the fields Basalt currently reads are exposed; everything else is a documented placeholder in the file itself. */
public data class BasaltConfig(
    public val port: Int,
    public val motd: String,
    public val maxPlayers: Int,
    public val onlineMode: Boolean,
    public val gamemode: String,
    public val operators: List<String>,
    public val opPermissionLevel: Int,
    public val worldName: String,
    public val difficulty: String,
    public val viewDistance: Int,
    public val saveIntervalSeconds: Int,
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
                motd = "A Basalt Server",
                maxPlayers = 20,
                onlineMode = true,
                gamemode = "survival",
                operators = emptyList(),
                opPermissionLevel = 4,
                worldName = "world",
                difficulty = "easy",
                viewDistance = 10,
                saveIntervalSeconds = 30,
                logLevel = LogLevel.INFO,
                logToFile = true,
                logKeepDays = 14,
                logColor = ColorMode.AUTO,
                generationMetrics = false,
            )

        /**
         * Loads `basalt.toml` from [path], generating a commented default on first run. If
         * `server.properties` exists and `basalt.toml` does not, imports the overlapping keys once;
         * it is not kept in sync afterward.
         */
        public fun load(
            path: String = "basalt.toml",
            legacyPropertiesPath: String = "server.properties",
            logger: Logger = Logger("Config"),
        ): BasaltConfig {
            if (!FileIO.exists(path)) {
                val legacy = FileIO.readText(legacyPropertiesPath)
                if (legacy != null) {
                    logger.info { "Found $legacyPropertiesPath, importing recognized values into $path once." }
                    FileIO.writeText(path, mergeLegacyProperties(DEFAULT_TOML, parseJavaProperties(legacy)))
                } else {
                    logger.info { "$path not found, generating one with vanilla-parity defaults." }
                    FileIO.writeText(path, DEFAULT_TOML)
                }
            }
            val text = FileIO.readText(path) ?: throw TomlFormatException("Failed to read $path after writing it")
            logger.info { "Loaded configuration from $path." }
            return fromToml(Toml.parse(text))
        }

        private fun fromToml(root: TomlValue.Table): BasaltConfig {
            val server = root.tableOrNull("server")
            val world = root.tableOrNull("world")
            val logging = root.tableOrNull("logging")
            val performance = root.tableOrNull("performance")
            return BasaltConfig(
                port = server?.intOrNull("port")?.toInt() ?: DEFAULT.port,
                motd = server?.stringOrNull("motd") ?: DEFAULT.motd,
                maxPlayers = server?.intOrNull("max-players")?.toInt() ?: DEFAULT.maxPlayers,
                onlineMode = server?.boolOrNull("online-mode") ?: DEFAULT.onlineMode,
                gamemode = server?.stringOrNull("gamemode") ?: DEFAULT.gamemode,
                operators = server?.stringListOrNull("operators") ?: DEFAULT.operators,
                opPermissionLevel =
                    server?.intOrNull("op-permission-level")?.toInt()?.coerceIn(0, 4) ?: DEFAULT.opPermissionLevel,
                worldName = world?.stringOrNull("name") ?: DEFAULT.worldName,
                difficulty = world?.stringOrNull("difficulty") ?: DEFAULT.difficulty,
                viewDistance = world?.intOrNull("view-distance")?.toInt() ?: DEFAULT.viewDistance,
                saveIntervalSeconds =
                    world?.intOrNull("save-interval-seconds")?.toInt()?.coerceAtLeast(0)
                        ?: DEFAULT.saveIntervalSeconds,
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
            legacy["view-distance"]?.toIntOrNull()?.let {
                result = result.replace("view-distance = 10", "view-distance = $it")
            }
            return result
        }
    }
}
