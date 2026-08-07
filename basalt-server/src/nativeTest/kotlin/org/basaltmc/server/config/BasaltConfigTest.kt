package org.basaltmc.server.config

import org.basaltmc.common.logging.LogLevel
import org.basaltmc.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class BasaltConfigTest {
    @Test
    fun generatesACommentedDefaultOnFirstRun() {
        val path = "basalt_config_test_default.toml"
        deleteIfExists(path)

        val config = BasaltConfig.load(path = path, legacyPropertiesPath = "basalt_config_test_no_legacy.properties")

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
        val path = "basalt_config_test_existing.toml"
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

        val config = BasaltConfig.load(path = path, legacyPropertiesPath = "basalt_config_test_no_legacy.properties")

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
        val configPath = "basalt_config_test_migrated.toml"
        val legacyPath = "basalt_config_test_legacy.properties"
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

        val config = BasaltConfig.load(path = configPath, legacyPropertiesPath = legacyPath)

        assertEquals(30001, config.port)
        assertEquals("Imported MOTD", config.motd)
        assertEquals(7, config.maxPlayers)
        assertFalse(config.onlineMode)
        deleteIfExists(configPath)
        deleteIfExists(legacyPath)
    }
}

private fun deleteIfExists(path: String) {
    platform.posix.remove(path)
}
