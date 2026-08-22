package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.ConfiguredCommand
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.timings.time
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.protocol.play.CommandNodeData
import org.basaltmc.wire.protocol.play.CommandSuggestion
import org.basaltmc.wire.protocol.play.CommandsPacket
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * Everything one running command can see: who ran it, its parsed arguments, and the server around it.
 *
 * Values are stored already parsed, so a command body reads `context.get<List<PlayerSession>>
 * ("targets")` rather than re-parsing text. Asking for a name the tree never declared is a bug in
 * the command, not in user input, so it throws rather than returning null.
 *
 * This is the whole surface a command is written against, which is why reporting and the two
 * "there has to be a player here" checks live on it rather than being passed around: a [Command]
 * implementation needs nothing else, including the ones plugins will eventually contribute.
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
 * and reads better next to nothing else. It is also the shape plugins need later, since a plugin
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
 * What a redirecting node does to the source before the rest of the line runs.
 *
 * parity: brigadier's `RedirectModifier`, which is how `/execute as @e at @s run ...` works: each
 * link hands the next one its own sources, and a forking link runs the tail once per source.
 */
public fun interface SourceModifier {
    public fun sources(context: CommandContext): List<CommandSource>
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

    /** Set on a redirecting node to change the source the tail runs as. */
    public var modifier: SourceModifier? = null

    /** parity: brigadier's `forks`, which makes the tail run once per source instead of failing on many. */
    public var forks: Boolean = false

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
) : CommandNode() {
    /** Set by `suggests {}` or `suggestsFromServer {}`: a list that belongs to this command. */
    public var suggestions: SuggestionProvider? = null

    /** The provider name the client is given, if any. Set by the two `suggestsFrom*` builders. */
    public var suggestionProviderId: String? = null

    internal val suggestionId: String? get() = suggestionProviderId ?: type.clientSuggestions

    internal val provider: SuggestionProvider? get() = suggestions ?: type.suggestions

    /**
     * Whether the client will ask about this argument at all, which needs the tree to name a
     * provider for it. Leaving that to the parser is not enough: the client only falls back to the
     * server for a registry it knows about and has not got, and a wrong guess is invisible from
     * here (the player just sees the syntax and no list).
     */
    internal val answersOnItsOwn: Boolean get() = suggestionId != null
}

/** Builder for one node's children, executor and permission requirement. */
public class CommandNodeBuilder(
    /** The node being built, for a command that has to redirect back onto its own tree. */
    public val node: CommandNode,
    /** The tree's root, which is where `/execute run` sends the rest of the line. */
    public val root: CommandNode = node,
) {
    /** Overrides the permission this node needs, for a sub-command that should be harder to reach. */
    public fun permission(permission: String) {
        node.permission = permission
    }

    public fun executes(executor: CommandExecutor) {
        node.executor = executor
    }

    /**
     * What this argument completes to when the client asks about it.
     *
     * No provider name travels in the tree, which is what vanilla does for an argument whose own
     * parser names a registry: the client completes from its copy when it has one and asks the
     * server when it has not. Use [suggestsFromServer] for an argument whose parser carries nothing
     * to complete from, since the client never asks about one of those on its own.
     */
    public fun suggests(provider: SuggestionProvider) {
        (node as? ArgumentNode)?.suggestions = provider
    }

    /** Same, but the tree tells the client to ask. parity: an argument vanilla calls `.suggests` on. */
    public fun suggestsFromServer(provider: SuggestionProvider) {
        val argument = node as? ArgumentNode ?: return
        argument.suggestions = provider
        argument.suggestionProviderId = SuggestionProviders.ASK_SERVER
    }

    /** Hands the completions to the client's own provider by name, so it never asks the server. */
    public fun suggestsFromClient(provider: String) {
        (node as? ArgumentNode)?.suggestionProviderId = provider
    }

    public fun redirectTo(
        target: CommandNode,
        forks: Boolean = false,
        modifier: SourceModifier? = null,
    ) {
        node.redirect = target
        node.forks = forks
        node.modifier = modifier
    }

    public fun literal(
        name: String,
        build: CommandNodeBuilder.() -> Unit,
    ): LiteralNode {
        val child = LiteralNode(name)
        child.requiredLevel = node.requiredLevel
        child.permission = node.permission
        CommandNodeBuilder(child, root).build()
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
        CommandNodeBuilder(child, root).build()
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

    /** The tree's root: what `/execute run` and every alias redirect onto. */
    public val rootNode: CommandNode get() = root
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
     * Builds the tree again from the current `commands.toml`, whole rather than patched: permission,
     * aliases and existence are all decided during registration, so diffing would mean writing
     * registration a second way. A few dozen nodes cost nothing and cannot drift from what a fresh
     * start produces.
     *
     * The caller sends the new tree to connected clients; until then they autocomplete the old one.
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
        command.build(CommandNodeBuilder(node, root))
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
        CommandNodeBuilder(node, root).build()
        root.children.add(node)
        return node
    }

    /**
     * Takes a node back out of the tree.
     *
     * For what is registered while the server runs and can go away again, which today means a
     * plugin's commands: unloading one has to leave the tree as it was, aliases included.
     */
    public fun unregister(node: LiteralNode) {
        root.children.remove(node)
        root.children.removeAll { it.redirect === node }
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
        // Named after the command, so a report shows which one is slow rather than that "commands"
        // are. The name comes from the input rather than the parse, since a command that fails to
        // parse still cost whatever it cost.
        val timing = environment.timings.of("command." + input.substringBefore(' ').lowercase())
        return timing.time { run(reader, source, input) }
    }

    private fun run(
        reader: StringReader,
        source: CommandSource,
        input: String,
    ): Int =
        try {
            runChain(parse(reader, root, source, emptyMap()), 0, source, input, reader)
        } catch (e: CommandSyntaxException) {
            reportSyntaxError(source, e)
            0
        } catch (e: Exception) {
            environment.logger.error(e) { "'/$input' threw an exception" }
            feedback.failure(source, Component.translatable("command.failed"))
            0
        }

    /**
     * One link of a parsed line: what it parsed, and what it does to the source for the next link.
     *
     * parity: brigadier's `CommandContext` chain. A redirect ends a link, which is why
     * `/execute as @e run say hi` keeps `@e` instead of losing it to the last redirect.
     */
    private class Link(
        val arguments: Map<String, Any>,
        val modifier: SourceModifier?,
        val forks: Boolean,
        val executor: CommandExecutor?,
    )

    /** Runs a parsed chain, fanning out wherever a link forks. parity: `ContextChain.executeAll`. */
    private fun runChain(
        links: List<Link>,
        index: Int,
        source: CommandSource,
        input: String,
        reader: StringReader,
    ): Int {
        val link = links[index]
        if (index == links.lastIndex) {
            val executor = link.executor ?: throw reader.syntaxError(UNKNOWN_COMMAND)
            // A command that failed still reports, which is what `/execute store success` writes 0 on.
            val result =
                try {
                    executor.run(CommandContext(source, this, input, link.arguments))
                } catch (e: CommandSyntaxException) {
                    (source as? ExecutionSource)?.report(0)
                    throw e
                }
            (source as? ExecutionSource)?.report(result)
            return result
        }
        val modifier =
            link.modifier
                ?: return runChain(links, index + 1, source, input, reader)
        val sources = modifier.sources(CommandContext(source, this, input, link.arguments))
        if (!link.forks) {
            val single = sources.singleOrNull() ?: return 0
            return runChain(links, index + 1, single, input, reader)
        }
        var successes = 0
        for (next in sources) {
            // One branch failing is not the others' problem: vanilla runs the rest and counts.
            val result = runCatching { runChain(links, index + 1, next, input, reader) }.getOrDefault(0)
            if (result > 0) successes++
        }
        return successes
    }

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
    ): List<Link> {
        val redirect = node.redirect
        if (!reader.canRead()) return listOf(Link(arguments, null, false, node.executor))
        // Every node but the first is preceded by the separator that ended the previous one.
        if (node !is RootNode) reader.expect(' ')
        if (!reader.canRead()) throw reader.syntaxError(UNKNOWN_COMMAND)

        val target = redirect ?: node
        val start = reader.cursor
        var deepestError: CommandSyntaxException? = null
        for (child in target.sortedChildren) {
            if (!source.mayUse(child)) continue
            reader.cursor = start
            // What follows a redirect is its own command, so it starts with no arguments.
            val branchArguments = (if (redirect == null) arguments else emptyMap()).toMutableMap()
            try {
                when (child) {
                    // Matched as text, not as a word: `/execute if score ... >= ...` names literals
                    // that are not words at all. parity: `LiteralCommandNode.parse`.
                    is LiteralNode -> if (!reader.readLiteral(child.name)) continue
                    is ArgumentNode -> branchArguments[child.name] = child.type.parse(reader, source, environment)
                    is RootNode -> continue
                }
                if (reader.canRead() && reader.peek() != ' ') throw reader.syntaxError(EXPECTED_SEPARATOR)
                val tail = parse(reader, child, source, branchArguments)
                return if (redirect == null) tail else listOf(Link(arguments, node.modifier, node.forks, null)) + tail
            } catch (e: CommandSyntaxException) {
                if (deepestError == null || e.cursor > deepestError.cursor) deepestError = e
            }
        }
        reader.cursor = start
        throw deepestError ?: reader.syntaxError(UNKNOWN_COMMAND)
    }

    /**
     * What [input] could be completed with, [input] ending where the client's cursor is.
     *
     * The line is walked the way [parse] walks it, but a branch that fails no longer ends the walk:
     * the last token is the one being typed, so a node that consumes text up to the end of the line
     * is the one asked for completions, and every branch that reaches it contributes.
     *
     * Literals are answered too, not only the arguments the client asked about: brigadier collects
     * from every child of the node the cursor sits in, and the client merges the answer with what it
     * worked out itself.
     */
    public fun suggest(
        source: CommandSource,
        input: String,
    ): SuggestionResult {
        val line = input.removePrefix("/")
        val offset = input.length - line.length
        val collected = mutableListOf<Pair<Int, CommandSuggestion>>()
        collect(StringReader(line), root, source, emptyMap(), collected)
        if (collected.isEmpty()) return SuggestionResult.EMPTY
        // The deepest token wins: a branch that got further is completing a more specific word than
        // one that stopped earlier, and a single reply carries a single range.
        val start = collected.maxOf { it.first }
        val suggestions =
            collected
                .filter { it.first == start }
                .map { it.second }
                .distinctBy { it.text }
                .sortedBy { it.text }
        return SuggestionResult(start + offset, line.length - start, suggestions)
    }

    private fun collect(
        reader: StringReader,
        node: CommandNode,
        source: CommandSource,
        arguments: Map<String, Any>,
        out: MutableList<Pair<Int, CommandSuggestion>>,
    ) {
        val target = node.redirect ?: node
        if (node !is RootNode) {
            if (!reader.canRead() || reader.peek() != ' ') return
            reader.skip()
        }
        val start = reader.cursor
        val rest = reader.string.substring(start)
        // A word still being typed has nothing after it: anything with a space is already settled,
        // and the cursor is further along the line.
        val typing = ' ' !in rest

        for (child in target.sortedChildren) {
            if (!source.mayUse(child)) continue
            reader.cursor = start
            when (child) {
                is RootNode -> continue
                is LiteralNode -> {
                    if (typing && child.name.lowercase().startsWith(rest.lowercase())) {
                        out += start to CommandSuggestion(child.name)
                    }
                    if (!reader.readLiteral(child.name)) continue
                    collect(reader, child, source, arguments, out)
                }
                is ArgumentNode -> {
                    if (typing) {
                        child.provider
                            ?.suggest(SuggestionContext(source, this, rest, arguments))
                            ?.forEach { out += start to it }
                    }
                    reader.cursor = start
                    val value = runCatching { child.type.parse(reader, source, environment) }.getOrNull() ?: continue
                    if (reader.canRead() && reader.peek() != ' ') continue
                    collect(reader, child, source, arguments + (child.name to value), out)
                }
            }
        }
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
                            suggestionId = node.suggestionId,
                        )
                }
            }
        return CommandsPacket(nodes, rootIndex = 0)
    }

    /**
     * Arguments that can answer a completion nobody will ever ask them for: no provider name in the
     * tree, and a parser the client cannot complete from either. Empty is the invariant; a test
     * keeps it that way.
     */
    public fun unreachableSuggestions(): List<Pair<String, String>> {
        val found = mutableListOf<Pair<String, String>>()

        fun walk(
            node: CommandNode,
            path: String,
            seen: MutableSet<CommandNode>,
        ) {
            if (!seen.add(node)) return
            for (child in node.children) {
                val name = if (path.isEmpty()) child.name else "$path ${child.name}"
                if (child is ArgumentNode && child.provider != null && !child.answersOnItsOwn) {
                    found += name to child.type.wire.toString()
                }
                walk(child, name, seen)
            }
        }
        walk(root, "", mutableSetOf())
        return found
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

    /**
     * Completions every use of this type gets, for a type whose values are the same wherever it
     * appears. A list that belongs to one command goes on the node instead, with `suggests {}`.
     */
    public val suggestions: SuggestionProvider? get() = null

    /** A vanilla provider the client resolves itself, for types whose values it already has. */
    public val clientSuggestions: String? get() = null
}
