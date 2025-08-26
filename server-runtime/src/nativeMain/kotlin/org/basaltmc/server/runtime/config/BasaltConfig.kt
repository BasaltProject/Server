package org.basaltmc.server.runtime.config

import org.basaltmc.wire.auth.forwarding.ProxyForwarding
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.toml.TomlValue
import org.basaltmc.wire.io.FileIO

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
    /** Whether only whitelisted players may join. Who is whitelisted lives in `players.db`. */
    public val whitelist: Boolean,
    /** Which proxy, if any, is speaking for the players that reach this server. See `[network.forwarding]`. */
    public val forwarding: ProxyForwarding,
    public val logLevel: LogLevel,
    public val logToFile: Boolean,
    public val logKeepDays: Int,
    public val logColor: ColorMode,
    public val generationMetrics: Boolean,
    /**
     * Whether a world may be split into regions that tick independently.
     *
     * NOT VANILLA above one region: two of them stop having a fixed order of tick between them and
     * what crosses a border is deferred a tick, so it is off unless the operator asks for it.
     */
    public val regional: Boolean = false,
    /** How many regions tick at once with [regional] on. Zero means "one per pair of cores". */
    public val regionThreads: Int = 0,
    /** Whether the plugins folder is looked at. Off means it is not even read. */
    public val pluginsEnabled: Boolean = true,
    /** What one plugin may run in a single turn, refilled on every entry. See `[plugins]`. */
    public val pluginInstructionsPerTurn: Long = 250_000,
    /** The most memory one plugin's own objects may take. */
    public val pluginMemoryBytes: Long = 32L * 1024 * 1024,
    /**
     * What each plugin is allowed beyond its own sandbox, by id.
     *
     * Empty for almost every server: only `packets`, `network` and `commands` are asked for here,
     * and a plugin that wants one and is not listed loads without it and says so.
     */
    public val pluginAllowances: Map<String, Set<String>> = emptyMap(),
    /** Whether orphaned plugin data is reported at startup: `warn` or `quiet`. */
    public val orphanData: String = "warn",
    /** How many copies of each plugin run. One on a server; more is a development switch. */
    public val pluginLanes: Int = 1,
) {
    public companion object {
        private val DEFAULT =
            BasaltConfig(
                port = 25565,
                maxPlayers = 80,
                onlineMode = true,
                gamemode = "survival",
                whitelist = false,
                forwarding = ProxyForwarding.DISABLED,
                logLevel = LogLevel.INFO,
                logToFile = true,
                logKeepDays = 14,
                logColor = ColorMode.AUTO,
                generationMetrics = false,
                regional = false,
                regionThreads = 0,
                pluginsEnabled = true,
                pluginInstructionsPerTurn = 250_000,
                pluginMemoryBytes = 32L * 1024 * 1024,
                pluginLanes = 1,
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
            /** The boot read that only exists to set up logging says nothing, so startup shows one line, not two. */
            announce: Boolean = true,
        ): BasaltConfig {
            val path = directory.pathOf(FILE_NAME)
            if (!FileIO.exists(path)) {
                FileIO.createDirectories(directory.root)
                val contents = initialContents(legacyPropertiesPath, logger)
                FileIO.writeText(path, contents)
            }
            val root = directory.load(FILE_NAME, SERVER_DEFAULT_TOML)
            if (announce) logger.info { "Loaded configuration from $path." }
            return fromToml(root, path)
        }

        /** What to write into a freshly created `server.toml`: an imported `server.properties`, or the default. */
        private fun initialContents(
            legacyPropertiesPath: String,
            logger: Logger,
        ): String {
            val legacy = FileIO.readText(legacyPropertiesPath)
            if (legacy != null) {
                logger.info { "Found $legacyPropertiesPath, importing recognized values once." }
                return mergeLegacyProperties(SERVER_DEFAULT_TOML, parseJavaProperties(legacy))
            }
            return SERVER_DEFAULT_TOML
        }

        private fun fromToml(
            root: TomlValue.Table,
            path: String,
        ): BasaltConfig {
            val server = root.tableOrNull("server")
            val logging = root.tableOrNull("logging")
            val performance = root.tableOrNull("performance")
            val plugins = root.tableOrNull("plugins")
            return BasaltConfig(
                port = server?.intOrNull("port")?.toInt() ?: DEFAULT.port,
                maxPlayers = server?.intOrNull("max-players")?.toInt() ?: DEFAULT.maxPlayers,
                onlineMode = server?.boolOrNull("online-mode") ?: DEFAULT.onlineMode,
                gamemode = server?.stringOrNull("gamemode") ?: DEFAULT.gamemode,
                whitelist = server?.boolOrNull("whitelist") ?: DEFAULT.whitelist,
                forwarding = ForwardingConfig.fromToml(root, path),
                logLevel = logging?.stringOrNull("level")?.let(::parseLogLevel) ?: DEFAULT.logLevel,
                logToFile = logging?.boolOrNull("file") ?: DEFAULT.logToFile,
                logKeepDays = logging?.intOrNull("keep-days")?.toInt() ?: DEFAULT.logKeepDays,
                logColor = logging?.stringOrNull("color")?.let(::parseColorMode) ?: DEFAULT.logColor,
                generationMetrics =
                    performance?.boolOrNull("generation-metrics") ?: DEFAULT.generationMetrics,
                regional = performance?.boolOrNull("regional") ?: DEFAULT.regional,
                regionThreads = regionThreadsOf(performance?.stringOrNull("region-threads")),
                pluginsEnabled = plugins?.boolOrNull("enabled") ?: DEFAULT.pluginsEnabled,
                // Floored rather than taken as written: a budget of five instructions or a
                // megabyte of nothing stops every plugin on the server, and reads as "plugins are
                // broken" rather than as the typo it is.
                pluginInstructionsPerTurn =
                    (plugins?.intOrNull("instructions-per-turn") ?: DEFAULT.pluginInstructionsPerTurn)
                        .coerceAtLeast(1_000L),
                pluginMemoryBytes =
                    (plugins?.intOrNull("memory-mb") ?: (DEFAULT.pluginMemoryBytes / 1024 / 1024))
                        .coerceAtLeast(1L) * 1024 * 1024,
                pluginLanes = (plugins?.intOrNull("test-lanes") ?: 1L).toInt().coerceAtLeast(1),
                pluginAllowances =
                    plugins
                        ?.tableOrNull("allow")
                        ?.let { allow ->
                            allow.keys().associateWith { id -> allow.stringListOrNull(id).orEmpty().toSet() }
                        }.orEmpty(),
                orphanData = plugins?.stringOrNull("orphan-data") ?: DEFAULT.orphanData,
            )
        }

        /**
         * parity of shape with Folia's own `threads = -1`: a number is taken as it is, and anything
         * else ("auto", missing) means "work it out", which is zero here and is resolved by the
         * scheduler rather than by the file.
         */
        private fun regionThreadsOf(value: String?): Int = value?.toIntOrNull() ?: DEFAULT.regionThreads

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
