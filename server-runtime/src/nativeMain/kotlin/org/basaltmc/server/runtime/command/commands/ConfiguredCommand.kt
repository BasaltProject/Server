package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.config.CustomCommand
import org.basaltmc.server.runtime.placeholder.PlaceholderSlots
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.common.placeholder.PlaceholderText
import org.basaltmc.wire.text.MiniMessage

/**
 * A command that exists because `commands.toml` said so: a name, a permission, and lines to answer.
 *
 * That is the whole feature, and the limit is the point. It covers the `/discord` and `/rules` that
 * every server writes and that today mean installing a plugin for three lines of text; anything with
 * a condition or an argument in it is a plugin, because a configuration file that grows those
 * becomes a programming language nobody chose to design.
 *
 * [PermissionLevel.ALL] because the named permission is what decides who may run it, and a level
 * above zero would additionally hide it from clients that were granted the permission.
 */
public class ConfiguredCommand(
    private val definition: CustomCommand,
) : Command(
        name = definition.name,
        permissionLevel = PermissionLevel.ALL,
        aliases = definition.aliases,
        permission = definition.permission,
    ) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            val placeholderContext =
                PlaceholderContext.EMPTY.with(PlaceholderSlots.PLAYER, context.source.player)
            for (line in definition.message) {
                val text = PlaceholderText.substitute(line, context.environment.placeholders, placeholderContext)
                context.source.sendSystemMessage(MiniMessage.parse(text))
            }
            1
        }
    }
}
