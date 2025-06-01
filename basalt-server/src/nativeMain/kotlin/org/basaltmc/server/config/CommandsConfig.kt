package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

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
