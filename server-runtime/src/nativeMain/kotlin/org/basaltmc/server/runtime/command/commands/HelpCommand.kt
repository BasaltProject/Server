package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.GreedyStringArgument
import org.basaltmc.server.runtime.command.smartUsage
import org.basaltmc.wire.text.Component

/**
 * parity: `HelpCommand`. Prints brigadier's own smart usage, one line per command, slash included.
 *
 * Vanilla's form takes a whole command line and describes wherever that line lands in the tree;
 * this one takes a command name and describes the command. The narrower form is a declared gap,
 * not an accident: describing a partially typed line needs the parser to report where it stopped
 * even on success, which the dispatcher does not expose yet.
 */
public class HelpCommand : Command(name = "help") {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            val usable = context.dispatcher.usableCommands(context.source)
            for (node in usable) {
                context.source.sendSystemMessage(
                    Component.text("/" + smartUsage(node, context.source)),
                )
            }
            usable.size
        }
        root.argument("command", GreedyStringArgument) {
            suggestsFromServer { context ->
                context.matching(context.dispatcher.usableCommands(context.source).map { it.name })
            }
            executes { context ->
                // Greedy, as vanilla declares it, but only the command named by the first word is
                // described: see the class docs for why the rest of the line is not.
                val name = context.get<String>("command").substringBefore(' ')
                val node =
                    context.dispatcher.usableCommands(context.source).firstOrNull { it.name == name }
                        ?: throw CommandSyntaxException(Component.translatable("commands.help.failed"))
                context.source.sendSystemMessage(Component.text("/" + smartUsage(node, context.source)))
                1
            }
        }
    }
}
