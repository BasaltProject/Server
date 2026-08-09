package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

internal const val COMMANDS_DEFAULT_TOML = """# Commands: which permission each one asks for, whether it exists at all, what it
# answers, and any command of your own.
#
# This file NAMES the permission a command requires. It does not decide who has
# it: that is groups.toml. The two read in one direction and only one:
#
#   commands.toml  ->  "/gamemode requires basalt.command.gamemode"
#   groups.toml    ->  "the operator group has basalt.command.gamemode"
#
# So to open /teleport to your moderators, either give them the permission it
# already asks for, or point it at one they already have. Both work; putting the
# list of who may run it here would mean permissions living in two files.
#
# Every command already asks for `basalt.command.<name>` without being listed
# here. This file is only for changing that.

[commands]
# One table per command you want to change. Every key is optional.
#
# [commands.teleport]
# # The permission it asks for, instead of basalt.command.teleport.
# permission = "basalt.command.moderator"
# # false removes it: it stops existing, and stops being sent to clients, rather
# # than existing and refusing. Aliases go with it.
# enabled = true
# # Extra names for the same command, on top of the ones it ships with.
# aliases = ["tpa"]

[custom]
# Commands of your own: a name, a permission, and what they answer. Enough for
# the /discord and /rules that would otherwise need a plugin, and no more than
# that: anything with logic in it is a BSMod plugin, not a config entry.
#
# `message` is MiniMessage, one entry per line, and takes the same %placeholders%
# as everywhere else. `permission` defaults to basalt.command.<name>, so remember
# to grant it in groups.toml or nobody will be able to run it. To let everyone
# use one, add its permission to the default group.
#
# [custom.discord]
# message = ["<blue>Chat with us: https://discord.gg/example</blue>"]
# aliases = ["dc"]
#
# [custom.online]
# permission = "basalt.command.help"
# message = ["<gray>%basalt_server_online% of %basalt_server_max_players% playing.</gray>"]

[messages]
# Replaces what a command answers, by the translation key it normally sends.
#
# Worth knowing before you use it: by default the server sends the *key* and each
# client translates it into its own language. Override one and it becomes literal
# text, the same for everyone, in whatever language you wrote it. That is the
# trade, and it is why there are no per-language tables here: a server is
# normally aimed at one community rather than being multilingual.
#
# %1%, %2% and so on are the values the message was going to be filled with, in
# order, as plain text. Placeholders work too.
#
# "commands.gamemode.success.self" = "<green>You are now in %1% mode.</green>"
# "commands.seed.success" = "<gray>Seed: %1%</gray>"
"""

/** One command's overrides. Everything is null when the file left it alone. */
public data class CommandOverride(
    public val permission: String? = null,
    public val enabled: Boolean = true,
    public val aliases: List<String> = emptyList(),
)

/** A command the operator invented: a name, a permission, and lines to answer with. */
public data class CustomCommand(
    public val name: String,
    public val permission: String,
    public val message: List<String>,
    public val aliases: List<String> = emptyList(),
)

/**
 * `config/commands.toml`.
 *
 * Deliberately three small things rather than one general mechanism: rename a permission, switch a
 * command off, add a name for it; invent a command that answers with text; replace what a command
 * says. Anything past that is a plugin, and pretending otherwise turns a configuration file into a
 * bad programming language.
 */
public class CommandsConfig(
    private val overrides: Map<String, CommandOverride>,
    public val custom: List<CustomCommand>,
    public val messages: Map<String, String>,
) {
    public operator fun get(command: String): CommandOverride = overrides[command.lowercase()] ?: DEFAULT

    public companion object {
        public const val FILE_NAME: String = "commands.toml"

        private val DEFAULT = CommandOverride()

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): CommandsConfig {
            val root = directory.load(FILE_NAME, COMMANDS_DEFAULT_TOML)
            val path = directory.pathOf(FILE_NAME)

            val declared = root.tableOrNull("commands")
            val overrides =
                declared
                    ?.keys()
                    ?.mapNotNull { name ->
                        val table = declared.tableOrNull(name) ?: return@mapNotNull null
                        name.lowercase() to
                            CommandOverride(
                                permission = table.stringOrNull("permission"),
                                enabled = table.boolOrNull("enabled") ?: true,
                                aliases = table.stringListOrNull("aliases").orEmpty(),
                            )
                    }?.toMap()
                    .orEmpty()

            val declaredCustom = root.tableOrNull("custom")
            val custom =
                declaredCustom
                    ?.keys()
                    ?.mapNotNull { name ->
                        val table = declaredCustom.tableOrNull(name) ?: return@mapNotNull null
                        val message = table.stringListOrNull("message") ?: table.stringOrNull("message")?.split("\n")
                        if (message.isNullOrEmpty()) {
                            // Not fatal: a command with nothing to say is a half-written entry, and
                            // stopping the server over it would be worse than skipping it.
                            logger.warn { "Custom command '$name' in $path has no message; skipping it." }
                            return@mapNotNull null
                        }
                        CustomCommand(
                            name = name.lowercase(),
                            permission = table.stringOrNull("permission") ?: "basalt.command.${name.lowercase()}",
                            message = message,
                            aliases = table.stringListOrNull("aliases").orEmpty(),
                        )
                    }.orEmpty()

            val messagesTable = root.tableOrNull("messages")
            val messages =
                messagesTable
                    ?.keys()
                    ?.mapNotNull { key -> messagesTable.stringOrNull(key)?.let { key to it } }
                    ?.toMap()
                    .orEmpty()

            if (custom.isNotEmpty()) logger.info { "Loaded ${custom.size} custom command(s) from $path." }
            val disabled = overrides.filterValues { !it.enabled }.keys
            if (disabled.isNotEmpty()) logger.info { "Disabled by $path: ${disabled.sorted().joinToString(", ")}." }
            return CommandsConfig(overrides, custom, messages)
        }
    }
}
