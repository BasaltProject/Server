package org.basaltmc.server.command.commands

import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderText
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.CommandSyntaxException
import org.basaltmc.server.command.GreedyStringArgument
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.command.WordArgument
import org.basaltmc.server.placeholder.PlaceholderSlots
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component
import org.basaltmc.text.MiniMessage
import org.basaltmc.text.TextColor

/**
 * `/placeholder`: what the registry knows and what a piece of text turns into, modelled on
 * PlaceholderAPI's debugging commands, which operators already know.
 *
 * Two deliberate differences: `cmdparse` runs the command as whoever typed it, since running text
 * as the console turns permission to debug placeholders into permission to do everything, and the
 * subject must be online, a stored row not being able to answer where it is standing.
 */
public class PlaceholderCommand :
    Command(
        name = "placeholder",
        permissionLevel = PermissionLevel.ADMINS,
        aliases = listOf("placeholders"),
    ) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("parse") {
            argument("subject", WordArgument) {
                argument("text", GreedyStringArgument) {
                    executes { context ->
                        val parsed = parse(context, subjectOf(context), context.get("text"))
                        context.source.sendSystemMessage(MiniMessage.parse(parsed))
                        1
                    }
                }
            }
        }

        root.literal("bcparse") {
            argument("subject", WordArgument) {
                argument("text", GreedyStringArgument) {
                    executes { context ->
                        val parsed = parse(context, subjectOf(context), context.get("text"))
                        context.environment.broadcast(MiniMessage.parse(parsed))
                        1
                    }
                }
            }
        }

        root.literal("cmdparse") {
            argument("subject", WordArgument) {
                argument("text", GreedyStringArgument) {
                    executes { context ->
                        val parsed = parse(context, subjectOf(context), context.get("text"))
                        context.source.sendSystemMessage(hint("Running: /$parsed"))
                        context.dispatcher.execute(context.source, parsed.removePrefix("/"))
                    }
                }
            }
        }

        root.literal("parserel") {
            argument("first", WordArgument) {
                argument("second", WordArgument) {
                    argument("text", GreedyStringArgument) {
                        executes { context ->
                            val first = onlinePlayer(context, context.get("first"))
                            val second = onlinePlayer(context, context.get("second"))
                            val placeholderContext =
                                PlaceholderContext.EMPTY
                                    .with(PlaceholderSlots.PLAYER, first)
                                    .with(PlaceholderSlots.OTHER_PLAYER, second)
                            val parsed = parse(context, placeholderContext, context.get("text"))
                            context.source.sendSystemMessage(MiniMessage.parse(parsed))
                            // No placeholder Basalt itself registers looks at the second player yet.
                            // The context is here so a plugin's can, which is the only way one ever
                            // could: a relational placeholder needs somewhere to put the other side.
                            context.source.sendSystemMessage(
                                hint("No built-in placeholder is relational yet; this is for plugins to use."),
                            )
                            1
                        }
                    }
                }
            }
        }

        root.literal("info") {
            // Greedy, not a word: an operator types the name the way it appears in a file, and
            // `%basalt_player_x%` is not a bare word to the parser.
            argument("name", GreedyStringArgument) {
                executes { context -> describe(context, context.get<String>("name").trim('%')) }
            }
        }

        root.literal("list") {
            executes { context -> listIdentifiers(context) }
            argument("identifier", WordArgument) {
                executes { context -> listNames(context, context.get("identifier")) }
            }
        }
    }

    /**
     * Who the text is about: a name, `me`, or `--null` for deliberately nobody.
     *
     * `--null` is not the same as leaving the argument out. It is how an operator checks what a
     * server list line or a console message will do, where there really is no player, without
     * having to log out to find out.
     */
    private fun subjectOf(context: CommandContext): PlaceholderContext {
        val subject = context.get<String>("subject")
        if (subject == "--null") return PlaceholderContext.EMPTY
        return PlaceholderContext.EMPTY.with(PlaceholderSlots.PLAYER, onlinePlayer(context, subject))
    }

    private fun onlinePlayer(
        context: CommandContext,
        name: String,
    ): PlayerSession {
        if (name == "me") {
            return context.source.player
                ?: throw CommandSyntaxException(
                    Component.text("The console is not a player. Name one, or use --null for no player at all."),
                )
        }
        return context.environment.players
            .snapshot()
            .firstOrNull { it.name.equals(name, ignoreCase = true) }
            ?: throw CommandSyntaxException(
                Component.text("'$name' is not online. Only online players can be asked about."),
            )
    }

    /** Substitutes, and reports every name that could not be filled in rather than leaving it quiet. */
    private fun parse(
        context: CommandContext,
        placeholderContext: PlaceholderContext,
        text: String,
    ): String =
        PlaceholderText.substitute(text, context.environment.placeholders, placeholderContext) { name, value ->
            context.source.sendSystemMessage(problem(name, value))
            // Kept as written: this command exists to show what a line does, and a blank where a
            // placeholder was is exactly what nobody can debug.
            null
        }

    private fun describe(
        context: CommandContext,
        name: String,
    ): Int {
        val registry = context.environment.placeholders
        val identifier = registry.providerFor(name)?.first
        if (identifier == null) {
            context.source.sendSystemMessage(
                problem(name, PlaceholderValue.Unknown).append(
                    Component.text(" Registered: ${registry.identifiers.sorted().joinToString(", ")}.", TextColor.GRAY),
                ),
            )
            return 0
        }
        context.source.sendSystemMessage(hint("%$name% is served by '$identifier'."))
        // Resolved against whoever asked, since that is the situation they are asking about.
        val here = PlaceholderContext.EMPTY.with(PlaceholderSlots.PLAYER, context.source.player)
        when (val value = registry.resolve(name, here)) {
            is PlaceholderValue.Text ->
                context.source.sendSystemMessage(
                    Component.text("Here it says: ", TextColor.GRAY).append(Component.text(value.value)),
                )
            else -> context.source.sendSystemMessage(problem(name, value))
        }
        return 1
    }

    private fun listIdentifiers(context: CommandContext): Int {
        val registry = context.environment.placeholders
        val identifiers = registry.identifiers.sorted()
        if (identifiers.isEmpty()) {
            context.source.sendSystemMessage(hint("Nothing has registered a placeholder."))
            return 0
        }
        context.source.sendSystemMessage(hint("${identifiers.size} placeholder identifier(s):"))
        for (identifier in identifiers) {
            val count = registry.namesOf(identifier).size
            val tail = if (count == 0) "does not list its names" else "$count name(s)"
            context.source.sendSystemMessage(
                Component
                    .text("  %$identifier", TextColor.WHITE)
                    .append(Component.text("_...%  $tail", TextColor.GRAY)),
            )
        }
        return identifiers.size
    }

    private fun listNames(
        context: CommandContext,
        identifier: String,
    ): Int {
        val registry = context.environment.placeholders
        val names = registry.namesOf(identifier)
        if (names.isEmpty()) {
            context.source.sendSystemMessage(
                hint("'$identifier' has registered no names, or does not list them. Try /placeholder list."),
            )
            return 0
        }
        context.source.sendSystemMessage(hint("${names.size} placeholder(s) under '$identifier':"))
        // One line, because a hundred of them one per line is unreadable in chat and a wall in the
        // console either way.
        context.source.sendSystemMessage(
            Component.text(names.joinToString(", ") { "%${identifier}_$it%" }, TextColor.GRAY),
        )
        return names.size
    }

    private fun problem(
        name: String,
        value: PlaceholderValue,
    ): Component =
        Component.text(
            when {
                value is PlaceholderValue.Unavailable && value.reason != null ->
                    "%$name% cannot be filled in here: ${value.reason}."
                value is PlaceholderValue.Unknown -> "%$name% is not a placeholder this server knows."
                else -> "%$name% cannot be filled in here."
            },
            TextColor.RED,
        )

    private fun hint(text: String): Component = Component.text(text, TextColor.GRAY)
}
