package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class CommandsConfigTest {
    private fun load(
        name: String,
        contents: String = COMMANDS_DEFAULT_TOML,
    ): CommandsConfig {
        val directory = ConfigDirectory("build/test-data/config_commands_$name", Logger("Test"))
        FileIO.createDirectories(directory.root)
        FileIO.writeText(directory.pathOf(CommandsConfig.FILE_NAME), contents)
        return CommandsConfig.load(directory, Logger("Test"))
    }

    @Test
    fun theGeneratedFileChangesNothing() {
        val config = load("generated")

        // Every example in it is commented out on purpose: generating a file that quietly renames a
        // permission or switches a command off would be a trap.
        assertTrue(config.custom.isEmpty())
        assertTrue(config.messages.isEmpty())
        assertTrue(config["gamemode"].enabled)
        assertEquals(null, config["gamemode"].permission)
    }

    @Test
    fun aCommandCanBePointedAtAPermissionSomebodyAlreadyHas() {
        val config =
            load(
                "permission",
                """
                [commands.teleport]
                permission = "basalt.command.moderator"
                aliases = ["tpa"]
                """.trimIndent(),
            )

        assertEquals("basalt.command.moderator", config["teleport"].permission)
        assertEquals(listOf("tpa"), config["teleport"].aliases)
        // Naming a permission is all this file does; who holds it is groups.toml's business.
        assertTrue(config["teleport"].enabled)
    }

    @Test
    fun aCommandCanBeSwitchedOff() {
        val config = load("disabled", "[commands.seed]\nenabled = false\n")

        assertFalse(config["seed"].enabled)
        assertTrue(config["kill"].enabled)
    }

    @Test
    fun aCustomCommandNeedsOnlyAMessage() {
        val config =
            load(
                "custom",
                """
                [custom.discord]
                message = ["<blue>Chat with us</blue>", "<gray>discord.gg/example</gray>"]
                aliases = ["dc"]
                """.trimIndent(),
            )

        val discord = config.custom.single()
        assertEquals("discord", discord.name)
        assertEquals(2, discord.message.size)
        assertEquals(listOf("dc"), discord.aliases)
        // Defaulted, so that granting it reads the same as granting any other command.
        assertEquals("basalt.command.discord", discord.permission)
    }

    @Test
    fun aCustomCommandWithNothingToSayIsSkippedRatherThanRegistered() {
        // Half-written, not broken: stopping the server over it would be worse than leaving it out.
        val config = load("empty_custom", "[custom.web]\naliases = [\"w\"]\n")

        assertTrue(config.custom.isEmpty())
    }

    @Test
    fun messagesAreKeyedByTheTranslationKeyTheyReplace() {
        val config =
            load(
                "messages",
                """
                [messages]
                "commands.seed.success" = "<gray>Seed: %1%</gray>"
                """.trimIndent(),
            )

        assertEquals("<gray>Seed: %1%</gray>", config.messages["commands.seed.success"])
    }
}
