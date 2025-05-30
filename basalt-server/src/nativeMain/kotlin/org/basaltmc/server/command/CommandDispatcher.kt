package org.basaltmc.server.command

import org.basaltmc.protocol.play.ArgumentParser
import org.basaltmc.protocol.play.CommandNodeData
import org.basaltmc.protocol.play.CommandsPacket
import org.basaltmc.server.command.commands.ConfiguredCommand
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor

/**
 * Everything one running command can see: who ran it, its parsed arguments, and the server around it.
 *
 * Values are stored already parsed, so a command body reads `context.get<List<PlayerSession>>
 * ("targets")` rather than re-parsing text. Asking for a name the tree never declared is a bug in
 * the command, not in user input, so it throws rather than returning null.
 *
 * This is the whole surface a command is written against, which is why reporting and the two
 * "there has to be a player here" checks live on it rather than being passed around: a [Command]
 * implementation needs nothing else, including the ones BSMod will eventually contribute.
 */
public class CommandContext(
    public val source: CommandSource,
    public val dispatcher: CommandDispatcher,
    public val input: String,
    private val arguments: Map<String, Any>,
) {
    public val environment: CommandEnvironment get() = dispatcher.environment

    public fun has(name: String): Boolean = name in arguments

    @Suppress("UNCHECKED_CAST")
    public fun <T> get(name: String): T = (arguments[name] ?: error("no argument named '$name' in /$input")) as T

    /**
     * Reports success, and with [broadcastToAdmins] set also tells every other operator and the
     * log. Mirrors `CommandSourceStack.sendSuccess`, whose boolean means the same thing.
     */
    public fun success(
        message: Component,
        broadcastToAdmins: Boolean = false,
    ) {
        dispatcher.feedback.success(source, message, broadcastToAdmins)
    }

    /** parity: `CommandSourceStack.getPlayerOrException`, for forms that act on "you" as a player. */
    public fun requirePlayer(): PlayerSession =
        source.player ?: throw CommandSyntaxException(Component.translatable("permissions.requires.player"))

    /** parity: `CommandSourceStack.getEntityOrException`, for forms that act on "you" as an entity. */
    public fun requireEntity(): PlayerSession =
        source.player ?: throw CommandSyntaxException(Component.translatable("permissions.requires.entity"))
}

/**
 * One command, as a class: its name, the permission it needs, its aliases, and the tree it builds.
 *
 * Vanilla gives every command its own class under `net.minecraft.server.commands` and this follows
 * that split, one file each, for the same reason: a command is a self-contained unit of behaviour
 * and reads better next to nothing else. It is also the shape BSMod needs later, since a plugin
 * contributing a command should have exactly this to fill in and nothing more.
 *
 * [aliases] are registered as brigadier redirects onto this command's own node, not as copies, so
 * `/tp` really is `/teleport` rather than a second tree that can drift from it.
 */
public abstract class Command(
    public val name: String,
    public val permissionLevel: Int = PermissionLevel.ALL,
    public val aliases: List<String> = emptyList(),
    /** Defaults to `basalt.command.<name>`, which is what `groups.toml` grants by wildcard. */
    public val permission: String = "basalt.command.$name",
) {
    /** Builds this command's arguments and sub-commands onto its own (already named) root node. */
    public abstract fun build(root: CommandNodeBuilder)
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

    /**
     * The permission this node needs, e.g. `basalt.command.gamemode`.
     *
     * Named, not numeric, because `commands.toml` has to be able to say which permission a command
     * asks for and `groups.toml` has to be able to grant it, and a number cannot be referred to.
     * Null means the node is open to anyone who reached it.
     */
    public var permission: String? = null

    /** parity: what vanilla demands for this command. Recorded, not enforced; see [PermissionLevel]. */
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
    /** Overrides the permission this node needs, for a sub-command that should be harder to reach. */
    public fun permission(permission: String) {
        node.permission = permission
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
        child.permission = node.permission
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
        child.permission = node.permission
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
    internal val feedback: CommandFeedback = CommandFeedback(environment)

    /** What was registered from code, so a reload can build the tree again from the same list. */
    private var registered: List<Command> = emptyList()

    init {
        registerCustom()
    }

    private fun registerCustom() {
        for (definition in environment.commands.custom) register(ConfiguredCommand(definition))
    }

    /**
     * Builds the tree again from the current `commands.toml`.
     *
     * The whole tree, not a patch of it: a command's permission, its aliases and whether it exists
     * at all are decided while it is being registered, so working out the difference would mean
     * reimplementing registration a second way. Rebuilding is cheap (it is a few dozen nodes) and
     * cannot drift from what a fresh start would have produced, which for a reload is the property
     * that matters.
     *
     * The caller is responsible for sending the new tree to every connected client: until it does,
     * their client still autocompletes the old one.
     */
    public fun rebuild() {
        root.children.clear()
        registerCustom()
        for (command in registered) registerBuilt(command)
    }

    /**
     * Registers [command] and every alias it declares, as `commands.toml` says to.
     *
     * Returns null for a command that file switched off. Off means gone: it is not registered, so it
     * is not in the tree sent to clients either, and it fails as an unknown command rather than as a
     * refusal. A command that answers "you may not do that" tells everyone it exists.
     */
    public fun register(command: Command): LiteralNode? {
        // Remembered before the override is consulted, so a command switched off in the file can be
        // switched back on by a later reload.
        if (registered.none { it.name == command.name }) registered = registered + command
        return registerBuilt(command)
    }

    private fun registerBuilt(command: Command): LiteralNode? {
        val override = environment.commands[command.name]
        if (!override.enabled) return null

        val node = LiteralNode(command.name)
        // Before build(), so children inherit them: a sub-command is never easier to reach than the
        // command it belongs to.
        node.requiredLevel = command.permissionLevel
        node.permission = override.permission ?: command.permission
        command.build(CommandNodeBuilder(node))
        root.children.add(node)
        for (alias in command.aliases + override.aliases) registerAlias(alias, node)
        return node
    }

    /** The lower-level form, for a tree built inline rather than by a [Command]. */
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
        node.permission = target.permission
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
            executor.run(CommandContext(source, this, input, parsed.arguments))
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
            if (!source.mayUse(child)) continue
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
                if (source.mayUse(child)) enumerate(child)
            }
            node.redirect?.let { enumerate(it) }
        }
        enumerate(root)

        val nodes =
            ordered.map { node ->
                val children =
                    node.sortedChildren
                        .filter { source.mayUse(it) }
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
        root.children.filterIsInstance<LiteralNode>().filter { source.mayUse(it) }

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
