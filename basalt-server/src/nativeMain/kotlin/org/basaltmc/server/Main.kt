package org.basaltmc.server

import io.ktor.client.HttpClient
import org.basaltmc.assets.AssetCache
import org.basaltmc.assets.EulaFile
import org.basaltmc.assets.LangAssets
import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.assets.VersionManifest
import org.basaltmc.auth.MojangSessionService
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionKeyPair
import org.basaltmc.common.logging.LogOutput
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.LogFile
import org.basaltmc.io.ShutdownSignal
import org.basaltmc.io.SocketException
import org.basaltmc.io.restoreTerminalMode
import org.basaltmc.server.command.ConsoleReader
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.config.ColorMode
import org.basaltmc.server.config.LiveConfigs
import org.basaltmc.server.config.ServerListConfig
import org.basaltmc.server.config.TomlFormatException
import org.basaltmc.server.config.WorldsConfig
import org.basaltmc.server.world.VanillaWorldGeneratorFactory
import org.basaltmc.storage.PlayerDatabaseSchemaException
import platform.posix.STDOUT_FILENO
import platform.posix.isatty
import kotlin.system.exitProcess
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * Exit codes an orchestrator can tell apart, the same way [EulaFile] already has one.
 *
 * A configuration mistake is the failure an operator actually hits (a typo in a world type, a
 * malformed table), and it deserves one readable line, not a Kotlin stack trace: the trace says
 * nothing they can act on and buries the message that does.
 */
private const val CONFIG_EXIT_CODE = 2
private const val BIND_EXIT_CODE = 3
private const val DATABASE_EXIT_CODE = 4

@OptIn(ExperimentalTime::class)
public fun main(args: Array<String>) {
    try {
        run(args)
    } catch (e: TomlFormatException) {
        Logger("Config").error { e.message ?: "Invalid configuration." }
        exitProcess(CONFIG_EXIT_CODE)
    } catch (e: PlayerDatabaseSchemaException) {
        Logger("Storage").error { e.message ?: "players.db does not match this build." }
        exitProcess(DATABASE_EXIT_CODE)
    } catch (e: SocketException) {
        Logger("Server").error {
            "${e.message}. Another server is probably already running on that port; " +
                "stop it or change `port` in config/server.toml."
        }
        exitProcess(BIND_EXIT_CODE)
    }
}

@OptIn(ExperimentalTime::class)
private fun run(args: Array<String>) {
    val startedAt = Clock.System.now()
    println(BANNER)
    println(versionLine())

    // No readLine()/prompts, no TTY detection: identical behavior interactive, headless, or
    // containerized. --accept-eula is a deliberate operator act, not a fallback. (This is about
    // boot never blocking on input; the operator console `BasaltServer` starts once already
    // running is a different concern, see `ConsoleReader`'s own KDoc.)
    val logger = Logger("Eula")
    val eulaResult = EulaFile.check(acceptFlag = "--accept-eula" in args, logger = logger)
    if (eulaResult is EulaFile.Result.Rejected) {
        exitProcess(EulaFile.REJECTED_EXIT_CODE)
    }

    // Escape codes are a terminal affordance; piped or redirected output gets none. Applied before
    // the config is read so that loading it is itself coloured, then re-applied once the operator's
    // own choice is known.
    val attachedToTerminal = isatty(STDOUT_FILENO) == 1
    LogOutput.useColor = attachedToTerminal

    // Read once for the things that cannot be redone per instance: the log file would archive
    // itself on every restart, and colour is a property of the terminal, not of the server.
    val bootConfig = BasaltConfig.load(logger = Logger("Config"), announce = false)
    Logger.defaultMinLevel = bootConfig.logLevel
    LogOutput.useColor =
        when (bootConfig.logColor) {
            ColorMode.ALWAYS -> true
            ColorMode.NEVER -> false
            ColorMode.AUTO -> attachedToTerminal
        }
    if (bootConfig.logToFile) {
        LogFile.install(LOG_ROOT, keepDays = bootConfig.logKeepDays, header = versionLine())
    }

    // Installed once for the process, not once per server: a second handler would leave the first
    // one pointing at an instance that has already stopped.
    ShutdownSignal.install()

    // The console outlives the server it talks to, because `/restart` replaces that server while
    // this thread is parked inside a blocking read of stdin. See ConsoleReader's own KDoc.
    var server: BasaltServer? = null
    ConsoleReader.start { server?.commands }

    // One instance per turn around this loop. A restart is not a re-exec and not a new process: the
    // old instance winds itself down exactly as `/stop` does, and then a new one is built from the
    // files as they are now. That is what makes server.toml and worlds.toml applicable, which
    // `/reload` deliberately cannot do.
    var first = true
    while (true) {
        val instanceStartedAt = if (first) startedAt else Clock.System.now()
        first = false
        val instance = buildServer(attachedToTerminal)
        server = instance
        instance.start(instanceStartedAt)
        if (!instance.shouldRestart) break
        server = null
    }
    // The console thread is parked in a blocking read and never gets to run its own cleanup, so
    // the terminal is put back from here. Skipping it leaves the operator's shell with no echo
    // after the server stops, which looks like their terminal broke.
    restoreTerminalMode()
    exitProcess(0)
}

/** Builds one server from the configuration as it stands right now. Called again on every restart. */
@OptIn(ExperimentalTime::class)
private fun buildServer(attachedToTerminal: Boolean): BasaltServer {
    // Read again on every start, so a restart really does pick up an edited server.toml: the port,
    // the game mode and the log level are all decided here.
    val config = BasaltConfig.load(logger = Logger("Config"))
    Logger.defaultMinLevel = config.logLevel

    // Downloading is the fallback, not the path: once a version's data is unpacked the lambda below
    // is never invoked, so a warm install resolves no metadata, contacts no network and reads no
    // archive. Only the server data is fetched; the client download holds assets nothing reads yet
    // (see `LangAssets`, still unused), and fetching it bought a log line.
    val assetsLogger = Logger("Assets")
    val dataRoot = "./.basalt/assets/${BuildInfo.MINECRAFT_VERSION}"
    val serverJar =
        ServerJarAssets(BuildInfo.MINECRAFT_VERSION, dataRoot, assetsLogger) {
            HttpClient().use { httpClient ->
                val versionMetadata = VersionManifest(httpClient).resolveCached(BuildInfo.MINECRAFT_VERSION)
                AssetCache(httpClient, logger = assetsLogger).ensure(versionMetadata, AssetCache.Artifact.SERVER)
            }
        }
    assetsLogger.info { "${BuildInfo.MINECRAFT_VERSION} data ready at $dataRoot." }

    val worldsConfig = WorldsConfig.load(logger = Logger("Config"))
    // The MOTD is the one thing the server list takes from a vanilla server.properties, and only
    // while generating the file for the first time.
    // Generated on first run before anything else reads them, so the MOTD import from a vanilla
    // server.properties happens exactly once, here.
    ServerListConfig.load(legacyMotd = BasaltConfig.legacyMotd(), logger = Logger("Config"))
    val live = LiveConfigs(LiveConfigs.read(logger = Logger("Config")))
    val onlineMode = if (config.onlineMode) OnlineMode(SessionKeyPair.generate(), MojangSessionService()) else null
    // Console-only: clients translate for themselves, a log file cannot. Unpacked from the same
    // server.jar as everything else, so this costs no extra download.
    val lang = LangAssets.loadCachedOrEmpty(dataRoot)
    return BasaltServer(
        config = config,
        onlineMode = onlineMode,
        worldGeneratorFactory = VanillaWorldGeneratorFactory(serverJar),
        serverJar = serverJar,
        worldsConfig = worldsConfig,
        live = live,
        lang = lang,
    )
}

private fun versionLine(): String = "Basalt Server ${BuildInfo.VERSION} (protocol ${BuildInfo.PROTOCOL_VERSION})"

private const val LOG_ROOT = "./.basalt/logs"

// Raw string, and deliberately not indented: the art is full of backslashes, and every leading
// space in it is significant. One blank line above and one below come from the newlines this
// string opens and closes with, plus println's own.
private const val BANNER =
    """
  ____                  _ _      _____
 |  _ \                | | |    / ____|
 | |_) | __ _ ___  __ _| | |_  | (___   ___ _ ____   _____ _ __
 |  _ < / _` / __|/ _` | | __|  \___ \ / _ \ '__\ \ / / _ \ '__|
 | |_) | (_| \__ \ (_| | | |_   ____) |  __/ |   \ V /  __/ |
 |____/ \__,_|___/\__,_|_|\__| |_____/ \___|_|    \_/ \___|_|
"""
