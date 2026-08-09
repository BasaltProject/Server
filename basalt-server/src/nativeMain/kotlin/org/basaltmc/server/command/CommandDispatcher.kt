package org.basaltmc.server.command

import org.basaltmc.protocol.play.ArgumentParser
import org.basaltmc.protocol.play.CommandNodeData
import org.basaltmc.protocol.play.CommandsPacket
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor

/**
 * One parsed command's arguments, keyed by the names its tree declared.
 *
 * Values are stored already parsed, so a command body reads `context.player("targets")` rather than
 * re-parsing text. Asking for a name the tree never declared is a bug in the command, not in user
 * input, so it throws rather than returning null.
 */
public class CommandContext(
    public val source: CommandSource,
    public val environment: CommandEnvironment,
    public val input: String,
    private val arguments: Map<String, Any>,
) {
    public fun has(name: String): Boolean = name in arguments

    @Suppress("UNCHECKED_CAST")
    public fun <T> get(name: String): T = (arguments[name] ?: error("no argument named '$name' in /$input")) as T
}

/** A command body. Returns vanilla's "result" integer, which is what `/execute store` reads. */
public fun interface CommandExecutor {
    public fun run(context: CommandContext): Int
}

/**
 * A node of the server's own command tree.
 *
 * Same shape as brigadier's: a root with literal and argument children, each optionally executable
 * and optionally redirecting somewhere else in the tree (which is how `/tp` is an alias of
 * `/teleport` rather than a copy of it). Parsing tries literals before arguments, so a literal
 * child always wins over an argument that would also match its text, matching brigadier's own
 * ordering.
 */
public sealed class CommandNode {
    public val children: MutableList<CommandNode> = mutableListOf()
    public var executor: CommandExecutor? = null
    public var requiredLevel: Int = PermissionLevel.ALL
    public var redirect: CommandNode? = null

    public abstract val name: String

    internal val sortedChildren: List<CommandNode>
        get() = children.sortedBy { if (it is LiteralNode) 0 else 1 }
}

public class RootNode : CommandNode() {
    override val name: String = ""
}

public class LiteralNode(
    override val name: String,
) : CommandNode()

public class ArgumentNode(
    override val name: String,
    public val type: ArgumentType<*>,
) : CommandNode()

/** Builder for one node's children, executor and permission requirement. */
public class CommandNodeBuilder(
    private val node: CommandNode,
) {
    public fun requires(level: Int) {
        node.requiredLevel = level
    }

    public fun executes(executor: CommandExecutor) {
        node.executor = executor
    }

    public fun redirectTo(target: CommandNode) {
        node.redirect = target
    }

    public fun literal(
        name: String,
        build: CommandNodeBuilder.() -> Unit,
    ): LiteralNode {
        val child = LiteralNode(name)
        child.requiredLevel = node.requiredLevel
        CommandNodeBuilder(child).build()
        node.children.add(child)
        return child
    }

    public fun argument(
        name: String,
        type: ArgumentType<*>,
        build: CommandNodeBuilder.() -> Unit,
    ): ArgumentNode {
        val child = ArgumentNode(name, type)
        child.requiredLevel = node.requiredLevel
        CommandNodeBuilder(child).build()
        node.children.add(child)
        return child
    }
}

/**
 * Parses and runs command lines, and hands out the tree the client needs to parse them itself.
 *
 * Failure handling follows vanilla's `Commands.performCommand`: the parse error goes back to the
 * source, followed by the grey "context" line marking where the parser stopped
 * (`command.context.here`, rendered by the client as `<--[HERE]`). A command that throws while
 * running reports `command.failed` instead, and never takes the connection down with it.
 */
public class CommandDispatcher(
    public val environment: CommandEnvironment,
) {
    private val root = RootNode()
    private val feedback = CommandFeedback(environment)

    public fun register(
        name: String,
        build: CommandNodeBuilder.() -> Unit,
    ): LiteralNode {
        val node = LiteralNode(name)
        CommandNodeBuilder(node).build()
        root.children.add(node)
        return node
    }

    /** Registers [name] as an alias that redirects onto [target], the way vanilla declares `/tp` and `/xp`. */
    public fun registerAlias(
        name: String,
        target: LiteralNode,
    ): LiteralNode {
        val node = LiteralNode(name)
        node.requiredLevel = target.requiredLevel
        node.redirect = target
        node.executor = target.executor
        root.children.add(node)
        return node
    }

    /**
     * Runs [input] (without its leading slash) as [source]. Returns the command's own result, or 0
     * if it failed; the failure has already been reported to [source] by then.
     */
    public fun execute(
        source: CommandSource,
        input: String,
    ): Int {
        val reader = StringReader(input)
        return try {
            val parsed = parse(reader, root, source, emptyMap())
            val executor =
                parsed.executor
                    ?: throw reader.syntaxError(UNKNOWN_COMMAND)
            executor.run(CommandContext(source, environment, input, parsed.arguments))
        } catch (e: CommandSyntaxException) {
            reportSyntaxError(source, e)
            0
        } catch (e: Exception) {
            environment.logger.error(e) { "'/$input' threw an exception" }
            feedback.failure(source, Component.translatable("command.failed"))
            0
        }
    }

    private class ParseResult(
        val executor: CommandExecutor?,
        val arguments: Map<String, Any>,
    )

    /**
     * Walks one node's children, trying each in turn and backtracking on failure, and reports
     * whichever failure got furthest along the line.
     *
     * Backtracking is not optional: `/teleport <destination>` and `/teleport <targets> <location>`
     * both start by parsing a player selector, so `/tp Steve 10 64 10` only resolves because the
     * first branch is allowed to fail on the trailing coordinates and the second is then tried.
     * That is brigadier's own behaviour, and vanilla's command tree is built assuming it. Each
     * branch parses into its own copy of the arguments so a failed attempt leaves nothing behind.
     */
    private fun parse(
        reader: StringReader,
        node: CommandNode,
        source: CommandSource,
        arguments: Map<String, Any>,
    ): ParseResult {
        val target = node.redirect ?: node
        if (!reader.canRead()) return ParseResult(node.executor, arguments)
        // Every node but the first is preceded by the separator that ended the previous one.
        if (node !is RootNode) reader.expect(' ')
        if (!reader.canRead()) throw reader.syntaxError(UNKNOWN_COMMAND)

        val start = reader.cursor
        var deepestError: CommandSyntaxException? = null
        for (child in target.sortedChildren) {
            if (!source.hasPermission(child.requiredLevel)) continue
            reader.cursor = start
            val branchArguments = arguments.toMutableMap()
            try {
                when (child) {
                    is LiteralNode -> {
                        val word = reader.readUnquotedString()
                        if (word != child.name) continue
                    }
                    is ArgumentNode -> branchArguments[child.name] = child.type.parse(reader, source, environment)
                    is RootNode -> continue
                }
                if (reader.canRead() && reader.peek() != ' ') throw reader.syntaxError(EXPECTED_SEPARATOR)
                return parse(reader, child, source, branchArguments)
            } catch (e: CommandSyntaxException) {
                if (deepestError == null || e.cursor > deepestError.cursor) deepestError = e
            }
        }
        reader.cursor = start
        throw deepestError ?: reader.syntaxError(UNKNOWN_COMMAND)
    }

    private fun reportSyntaxError(
        source: CommandSource,
        e: CommandSyntaxException,
    ) {
        feedback.failure(source, e.component)
        val rawInput = e.input ?: return
        if (e.cursor < 0) return
        val cursor = minOf(rawInput.length, e.cursor)
        val prefix = rawInput.substring(maxOf(0, cursor - CONTEXT_CHARACTERS), cursor)
        val context = mutableListOf<Component>()
        if (cursor > CONTEXT_CHARACTERS) context += Component.text("...")
        context += Component.text(prefix)
        if (cursor < rawInput.length) {
            context += Component.text(rawInput.substring(cursor)).color(TextColor.RED).underlined()
        }
        context += Component.translatable("command.context.here").color(TextColor.RED).italic()
        // Vanilla also hangs a "suggest this command again" click event off this line: skipped,
        // documented gap, nothing else about the message changes.
        source.sendSystemMessage(Component.Text("", children = context).color(TextColor.GRAY))
    }

    /**
     * The tree [source] may use, flattened for [CommandsPacket]. Filtered by permission exactly as
     * vanilla's `Commands.fillUsableCommands` filters by `canUse`: a player who cannot run
     * `/gamemode` is never told it exists, so their client greys it out rather than letting them
     * type it and be refused.
     */
    public fun treeFor(source: CommandSource): CommandsPacket {
        val ids = mutableMapOf<CommandNode, Int>()
        val ordered = mutableListOf<CommandNode>()

        fun enumerate(node: CommandNode) {
            if (node in ids) return
            ids[node] = ordered.size
            ordered.add(node)
            for (child in node.sortedChildren) {
                if (source.hasPermission(child.requiredLevel)) enumerate(child)
            }
            node.redirect?.let { enumerate(it) }
        }
        enumerate(root)

        val nodes =
            ordered.map { node ->
                val children =
                    node.sortedChildren
                        .filter { source.hasPermission(it.requiredLevel) }
                        .mapNotNull { ids[it] }
                        .toIntArray()
                when (node) {
                    is RootNode ->
                        CommandNodeData(CommandNodeData.Kind.ROOT, executable = false, children = children)
                    is LiteralNode ->
                        CommandNodeData(
                            CommandNodeData.Kind.LITERAL,
                            executable = node.executor != null,
                            children = children,
                            redirect = node.redirect?.let { ids[it] },
                            name = node.name,
                        )
                    is ArgumentNode ->
                        CommandNodeData(
                            CommandNodeData.Kind.ARGUMENT,
                            executable = node.executor != null,
                            children = children,
                            redirect = node.redirect?.let { ids[it] },
                            name = node.name,
                            parser = node.type.wire,
                            suggestionId = node.type.suggestionId,
                        )
                }
            }
        return CommandsPacket(nodes, rootIndex = 0)
    }

    /** Every registered top-level command name, in registration order. Used by `/help`. */
    public fun usableCommands(source: CommandSource): List<LiteralNode> =
        root.children.filterIsInstance<LiteralNode>().filter { source.hasPermission(it.requiredLevel) }

    private companion object {
        // Brigadier's own literals, which the client shows verbatim: they are not translation keys.
        const val UNKNOWN_COMMAND = "Unknown command"
        const val EXPECTED_SEPARATOR = "Expected whitespace to end one argument, but found trailing data"

        // How much of the line vanilla echoes back before the `<--[HERE]` marker.
        const val CONTEXT_CHARACTERS = 10
    }
}

/** Wire-level argument parsing: text in, a value out, plus how the client should be told to parse the same thing. */
public interface ArgumentType<T> {
    public fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): T & Any

    public val wire: ArgumentParser

    /** A `minecraft:ask_server`-style suggestion provider, or null for the parser's built-in suggestions. */
    public val suggestionId: String? get() = null
}
