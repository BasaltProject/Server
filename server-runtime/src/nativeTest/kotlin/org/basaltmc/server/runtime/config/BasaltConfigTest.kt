package org.basaltmc.server.runtime.config

import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.toml.TomlFormatException
import org.basaltmc.wire.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class BasaltConfigTest {
    private fun silentLogger() = Logger("BasaltConfigTest").apply { minLevel = LogLevel.ERROR }

    @Test
    fun generatesACommentedDefaultOnFirstRun() {
        val directory = directoryFor("default")
        val path = directory.pathOf(BasaltConfig.FILE_NAME)
        deleteIfExists(path)

        val config =
            BasaltConfig.load(
                directory = directory,
                legacyPropertiesPath = "build/test-data/no_such_legacy.properties",
            )

        assertTrue(FileIO.exists(path))
        assertEquals(25565, config.port)
        assertEquals(80, config.maxPlayers)
        assertTrue(config.onlineMode)
        assertEquals(LogLevel.INFO, config.logLevel)
        deleteIfExists(path)
    }

    @Test
    fun reloadsAnExistingFileWithoutRegeneratingIt() {
        val directory = directoryFor("existing")
        val path = directory.pathOf(BasaltConfig.FILE_NAME)
        FileIO.createDirectories(directory.root)
        FileIO.writeText(
            path,
            """
            [server]
            port = 30000
            max-players = 5
            online-mode = false

            [logging]
            level = "debug"
            """.trimIndent(),
        )

        val config =
            BasaltConfig.load(
                directory = directory,
                legacyPropertiesPath = "build/test-data/no_such_legacy.properties",
            )

        assertEquals(30000, config.port)
        assertEquals(5, config.maxPlayers)
        assertFalse(config.onlineMode)
        assertEquals(LogLevel.DEBUG, config.logLevel)
        deleteIfExists(path)
    }

    @Test
    fun readsTheMotdOutOfLegacyServerPropertiesForTheServerListFile() {
        val legacyPath = "build/test-data/legacy_motd.properties"
        FileIO.writeText(legacyPath, "motd=Imported MOTD\n")

        // Not part of BasaltConfig any more: server.toml has no MOTD, so the import hands it to
        // whoever generates server_list.toml instead of keeping a field for it.
        assertEquals("Imported MOTD", BasaltConfig.legacyMotd(legacyPath))
        deleteIfExists(legacyPath)
    }

    @Test
    fun importsRecognizedKeysFromLegacyServerPropertiesOnce() {
        val directory = directoryFor("properties_import")
        val configPath = directory.pathOf(BasaltConfig.FILE_NAME)
        val legacyPath = "build/test-data/legacy.properties"
        deleteIfExists(configPath)
        FileIO.writeText(
            legacyPath,
            """
            server-port=30001
            motd=Imported MOTD
            max-players=7
            online-mode=false
            """.trimIndent(),
        )

        val config = BasaltConfig.load(directory = directory, legacyPropertiesPath = legacyPath)

        assertEquals(30001, config.port)
        assertEquals(7, config.maxPlayers)
        assertFalse(config.onlineMode)
        deleteIfExists(configPath)
        deleteIfExists(legacyPath)
    }
}

class ConfigDirectoryTest {
    private val directory = directoryFor("directory")

    @Test
    fun aFileThatDoesNotParseStopsTheServerAndNamesItself() {
        FileIO.createDirectories(directory.root)
        val path = directory.pathOf("broken.toml")
        FileIO.writeText(path, "[server\nport = 1")
        try {
            val failure = assertFailsWith<TomlFormatException> { directory.load("broken.toml", "") }
            // The point: with several files, "the configuration is broken" is not useful on its own.
            assertTrue(failure.message.orEmpty().contains("broken.toml"))
        } finally {
            platform.posix.remove(path)
        }
    }

    /** Writes [contents] as the whole file (the generated default when omitted) and loads it. */
    private fun load(
        name: String,
        contents: String = SERVER_DEFAULT_TOML,
    ): BasaltConfig {
        val directory = directoryFor(name)
        FileIO.createDirectories(directory.root)
        FileIO.writeText(directory.pathOf(BasaltConfig.FILE_NAME), contents)
        return BasaltConfig.load(
            directory = directory,
            legacyPropertiesPath = "build/test-data/no_such_legacy.properties",
        )
    }

    @Test
    fun theGeneratedFileRunsPluginsOnOneLane() {
        val config = load("plugins_defaults")
        assertTrue(config.pluginsEnabled)
        assertEquals(1, config.pluginLanes, "a server runs one copy of each plugin")
        assertEquals(250_000L, config.pluginInstructionsPerTurn)
        assertEquals(32L * 1024 * 1024, config.pluginMemoryBytes)
    }

    @Test
    fun thePluginLimitsAreWhatTheFileSays() {
        val config =
            load(
                "plugins_set",
                "[plugins]\nenabled = false\ninstructions-per-turn = 40000\nmemory-mb = 8\ntest-lanes = 2\n",
            )
        assertFalse(config.pluginsEnabled)
        assertEquals(40_000L, config.pluginInstructionsPerTurn)
        assertEquals(8L * 1024 * 1024, config.pluginMemoryBytes)
        assertEquals(2, config.pluginLanes)
    }

    @Test
    fun nonsensePluginLimitsAreBroughtBackToSomethingRunnable() {
        // A budget of five instructions is a typo, not a policy: it would stop every plugin on the
        // server and read as "plugins are broken".
        val config = load("plugins_silly", "[plugins]\ninstructions-per-turn = 5\nmemory-mb = 0\ntest-lanes = 0\n")
        assertEquals(1_000L, config.pluginInstructionsPerTurn)
        assertEquals(1L * 1024 * 1024, config.pluginMemoryBytes)
        assertEquals(1, config.pluginLanes)
    }
}

private fun deleteIfExists(path: String) {
    platform.posix.remove(path)
}

/** Each test gets its own config directory, under build/ where every other test artefact lives. */
private fun directoryFor(name: String) = ConfigDirectory("build/test-data/config_$name", Logger("ConfigTest"))
