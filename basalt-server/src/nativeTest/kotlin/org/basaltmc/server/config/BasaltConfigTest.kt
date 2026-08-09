package org.basaltmc.server.config

import org.basaltmc.common.logging.LogLevel
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
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
        assertEquals("A Basalt Server", config.motd)
        assertEquals(20, config.maxPlayers)
        assertTrue(config.onlineMode)
        assertEquals("world", config.worldName)
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
            motd = "custom"
            max-players = 5
            online-mode = false

            [world]
            name = "custom-world"

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
        assertEquals("custom", config.motd)
        assertEquals(5, config.maxPlayers)
        assertFalse(config.onlineMode)
        assertEquals("custom-world", config.worldName)
        assertEquals(LogLevel.DEBUG, config.logLevel)
        deleteIfExists(path)
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
        assertEquals("Imported MOTD", config.motd)
        assertEquals(7, config.maxPlayers)
        assertFalse(config.onlineMode)
        deleteIfExists(configPath)
        deleteIfExists(legacyPath)
    }

    @Test
    fun readsTheSaveInterval() {
        val directory = directoryFor("save_interval")
        FileIO.createDirectories(directory.root)
        FileIO.writeText(directory.pathOf(BasaltConfig.FILE_NAME), "[world]\nsave-interval-seconds = 120\n")
        try {
            assertEquals(120, BasaltConfig.load(directory, logger = silentLogger()).saveIntervalSeconds)
        } finally {
            platform.posix.remove(directory.pathOf(BasaltConfig.FILE_NAME))
        }
    }

    @Test
    fun rejectsANegativeSaveInterval() {
        val directory = directoryFor("negative_save_interval")
        FileIO.createDirectories(directory.root)
        FileIO.writeText(directory.pathOf(BasaltConfig.FILE_NAME), "[world]\nsave-interval-seconds = -5\n")
        try {
            // Clamped rather than obeyed: a negative interval has no meaning, and 0 already says
            // "never save periodically".
            assertEquals(0, BasaltConfig.load(directory, logger = silentLogger()).saveIntervalSeconds)
        } finally {
            platform.posix.remove(directory.pathOf(BasaltConfig.FILE_NAME))
        }
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
}

private fun deleteIfExists(path: String) {
    platform.posix.remove(path)
}

/** Each test gets its own config directory, under build/ where every other test artefact lives. */
private fun directoryFor(name: String) = ConfigDirectory("build/test-data/config_$name", Logger("ConfigTest"))
