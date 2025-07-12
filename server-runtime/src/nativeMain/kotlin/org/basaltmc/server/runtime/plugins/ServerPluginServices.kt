package org.basaltmc.server.runtime.plugins

import org.basaltmc.server.plugins.PluginArgumentType
import org.basaltmc.server.plugins.PluginCommandCall
import org.basaltmc.server.plugins.PluginCommandSpec
import org.basaltmc.server.plugins.PluginCommands
import org.basaltmc.server.plugins.PluginHandle
import org.basaltmc.server.plugins.PluginPaths
import org.basaltmc.server.plugins.PluginServices
import org.basaltmc.server.plugins.PluginTasks
import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSource
import org.basaltmc.server.runtime.command.DoubleArgument
import org.basaltmc.server.runtime.command.GreedyStringArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.LiteralNode
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.sched.Owner
import org.basaltmc.wire.protocol.play.CustomPayloadPacket
import org.basaltmc.wire.text.MiniMessage

/**
 * The server, as much of it as the plugin host is allowed to borrow.
 *
 * Everything a plugin runs is put on one owner: the global one, which is the only owner that exists
 * in both scheduler modes and is a single thread in both. That is what makes this stage's single
 * lane true rather than hopeful. When lanes follow regions, this is the seam that changes, and
 * nothing above it does.
 */
public class ServerPluginServices(
    private val owner: Owner,
    private val dispatcher: CommandDispatcher,
    private val currentTick: () -> Long,
    root: String = ".",
    override val players: org.basaltmc.server.plugins.PluginPlayers = NoPlayers,
    override val worlds: org.basaltmc.server.plugins.PluginWorlds = NoWorlds,
    override val placeholders: org.basaltmc.server.plugins.PluginPlaceholders = NoPlaceholders,
    override val storage: org.basaltmc.server.plugins.PluginStorage = NoStorage,
    override val menus: org.basaltmc.server.plugins.PluginMenus = NoMenus,
    override val packets: org.basaltmc.server.plugins.PluginPacketSender = NoPackets,
    override val channels: org.basaltmc.server.plugins.PluginChannelSender = NoChannels,
    /** Who owns a position: the seam `at()` is built on. Null answers "nobody", and it runs here. */
    private val ownerAt: (String, Double, Double) -> Owner? = { _, _, _ -> null },
) : PluginServices {
    override val tasks: PluginTasks = OwnerTasks(owner, currentTick)
    override val commands: PluginCommands = DispatcherCommands(owner, dispatcher)

    override fun at(
        world: String,
        x: Double,
        z: Double,
        task: () -> Unit,
    ): Boolean {
        val target = ownerAt(world, x, z) ?: return false
        if (target.isCurrent) task() else target.submit(task)
        return true
    }

    /**
     * Puts the plugin commands back after the tree was rebuilt.
     *
     * Rebuilding is how `commands.toml` is applied, and it starts from the commands registered in
     * code, which plugin ones are not. Without this they would quietly disappear the first time an
     * operator reloaded the configuration.
     */
    public fun refreshCommands() {
        (commands as DispatcherCommands).refresh()
    }

    override val paths: PluginPaths =
        object : PluginPaths {
            override val packages: String = "$root/plugins".removePrefix("./")

            override fun folder(pluginId: String): String = "$packages/$pluginId"
        }
}

/** Plugin work, on the owner's own queue: the same one the server's own tasks go through. */
private class OwnerTasks(
    private val owner: Owner,
    private val currentTick: () -> Long,
) : PluginTasks {
    override val tick: Long get() = currentTick()

    override fun after(
        delayTicks: Int,
        task: () -> Unit,
    ): PluginHandle {
        val handle = owner.schedule(delayTicks, task)
        return PluginHandle { handle.cancel() }
    }

    /**
     * Repeats by scheduling the next one as it runs.
     *
     * The owner has no repeating task of its own, and giving it one for this would mean a second
     * kind of queue entry to cancel and to drain at shutdown. Rescheduling keeps one kind.
     *
     * The re-arm asks for one turn less than the period: a task scheduled while a turn is running
     * is due that many turns *after* this one, so `every(1)` armed with 1 would run every other
     * tick rather than every tick.
     */
    override fun every(
        delayTicks: Int,
        periodTicks: Int,
        task: () -> Unit,
    ): PluginHandle {
        val period = maxOf(periodTicks, 1)
        var cancelled = false
        var handle = owner.schedule(delayTicks) {}

        fun arm(delay: Int) {
            handle =
                owner.schedule(delay) {
                    if (cancelled) return@schedule
                    task()
                    if (!cancelled) arm(period - 1)
                }
        }
        arm(maxOf(delayTicks, 0))
        return PluginHandle {
            cancelled = true
            handle.cancel()
        }
    }
}

/** Plugin commands, in the server's own tree, so they complete on the client like any other. */
private class DispatcherCommands(
    private val owner: Owner,
    private val dispatcher: CommandDispatcher,
) : PluginCommands {
    private class Entry(
        val pluginId: String,
        val spec: PluginCommandSpec,
        val run: (PluginCommandCall) -> Unit,
    ) {
        var node: LiteralNode? = null
    }

    private val entries = mutableListOf<Entry>()

    override fun register(
        pluginId: String,
        spec: PluginCommandSpec,
        run: (PluginCommandCall) -> Unit,
    ): PluginHandle {
        val entry = Entry(pluginId, spec, run)
        entries += entry
        entry.node = add(entry)
        return PluginHandle {
            entries.remove(entry)
            entry.node?.let { dispatcher.unregister(it) }
        }
    }

    /** Registers everything again, after a rebuild threw the whole tree away. */
    fun refresh() {
        for (entry in entries) entry.node = add(entry)
    }

    private fun add(entry: Entry): LiteralNode =
        dispatcher.register(entry.spec.name) {
            permission(entry.spec.permission ?: "basalt.plugin.${entry.pluginId}.${entry.spec.name}")
            build(this, entry.spec, entry.spec.literals.drop(1), entry.run)
        }

    /** Literals first, then one argument node per argument, the executor hanging off the last one. */
    private fun build(
        builder: CommandNodeBuilder,
        spec: PluginCommandSpec,
        literals: List<String>,
        run: (PluginCommandCall) -> Unit,
    ) {
        if (literals.isNotEmpty()) {
            builder.literal(literals.first()) {
                build(this, spec, literals.drop(1), run)
            }
            return
        }
        arguments(builder, spec, 0, run)
    }

    private fun arguments(
        builder: CommandNodeBuilder,
        spec: PluginCommandSpec,
        index: Int,
        run: (PluginCommandCall) -> Unit,
    ) {
        // An optional argument means the command is runnable without it, so every node from the
        // first optional one onwards executes.
        val remaining = spec.arguments.drop(index)
        if (remaining.isEmpty() || remaining.first().optional) {
            builder.executes { context ->
                execute(context.source, spec, index, context, run)
                1
            }
        }
        val argument = remaining.firstOrNull() ?: return
        builder.argument(argument.name, typeOf(argument.type, argument.min, argument.max)) {
            arguments(this, spec, index + 1, run)
        }
    }

    private fun typeOf(
        type: PluginArgumentType,
        min: Long?,
        max: Long?,
    ) = when (type) {
        PluginArgumentType.WORD -> WordArgument
        PluginArgumentType.TEXT -> GreedyStringArgument
        PluginArgumentType.INT ->
            IntegerArgument(
                (min ?: Int.MIN_VALUE.toLong()).toInt(),
                (
                    max
                        ?: Int.MAX_VALUE.toLong()
                ).toInt(),
            )
        PluginArgumentType.FLOAT ->
            DoubleArgument(
                min?.toDouble() ?: -Double.MAX_VALUE,
                max?.toDouble() ?: Double.MAX_VALUE,
            )
        PluginArgumentType.BOOL -> BoolArgument
    }

    /**
     * Hands the call to the owner rather than running it here.
     *
     * A command arrives on whichever owner the source belongs to, and plugin code only ever runs on
     * one. Going through the queue is also what gives the plugin a fresh turn of its own.
     */
    private fun execute(
        source: CommandSource,
        spec: PluginCommandSpec,
        provided: Int,
        context: org.basaltmc.server.runtime.command.CommandContext,
        run: (PluginCommandCall) -> Unit,
    ) {
        val values = mutableMapOf<String, Any?>()
        for (argument in spec.arguments.take(provided)) {
            values[argument.name] = if (context.has(argument.name)) context.get<Any>(argument.name) else null
        }
        val name = source.name
        val console = source.player == null
        val call =
            object : PluginCommandCall {
                override val source: String get() = name

                override val isConsole: Boolean get() = console

                override fun reply(message: String) {
                    source.sendSystemMessage(MiniMessage.parse(message))
                }

                override fun argument(name: String): Any? = values[name]
            }
        if (owner.isCurrent) run(call) else owner.submit { run(call) }
    }
}

/** Before the object model is given to a test server: nobody is online and there are no worlds. */
private object NoPlayers : org.basaltmc.server.plugins.PluginPlayers {
    override fun online(): List<org.basaltmc.server.plugins.PluginPlayerRef> = emptyList()

    override fun byName(name: String): org.basaltmc.server.plugins.PluginPlayerRef? = null

    override fun byUuid(uuid: String): org.basaltmc.server.plugins.PluginPlayerRef? = null
}

private object NoWorlds : org.basaltmc.server.plugins.PluginWorlds {
    override fun all(): List<org.basaltmc.server.plugins.PluginWorldRef> = emptyList()

    override fun byName(name: String): org.basaltmc.server.plugins.PluginWorldRef? = null

    override val default: org.basaltmc.server.plugins.PluginWorldRef? = null

    override fun create(
        name: String,
        dimension: String,
        type: String,
        seed: Long?,
    ): org.basaltmc.server.plugins.PluginWorldRef? = null
}

private object NoPlaceholders : org.basaltmc.server.plugins.PluginPlaceholders {
    override fun register(
        pluginId: String,
        name: String,
        value: (org.basaltmc.server.plugins.PluginPlayerRef?) -> String,
    ): org.basaltmc.server.plugins.PluginHandle =
        org.basaltmc.server.plugins
            .PluginHandle { }

    override fun fill(
        text: String,
        player: org.basaltmc.server.plugins.PluginPlayerRef?,
    ): String = text
}

private object NoStorage : org.basaltmc.server.plugins.PluginStorage {
    override fun playerValue(
        pluginId: String,
        uuid: String,
        key: String,
    ): String? = null

    override fun setPlayerValue(
        pluginId: String,
        uuid: String,
        key: String,
        value: String?,
    ) = Unit

    override fun playerValues(
        pluginId: String,
        uuid: String,
    ): Map<String, String> = emptyMap()

    override fun top(
        pluginId: String,
        key: String,
        limit: Int,
    ): List<Pair<String, String>> = emptyList()
}

/** Before there is a world to open a window in. */
internal object NoMenus : org.basaltmc.server.plugins.PluginMenus {
    override fun open(
        player: org.basaltmc.server.plugins.PluginPlayerRef,
        title: String,
        rows: Int,
        items: Map<Int, org.basaltmc.server.plugins.PluginItem>,
        onClick: (Int) -> Unit,
    ): org.basaltmc.server.plugins.PluginHandle =
        org.basaltmc.server.plugins
            .PluginHandle { }

    override fun close(player: org.basaltmc.server.plugins.PluginPlayerRef) {
    }

    override fun refresh(
        player: org.basaltmc.server.plugins.PluginPlayerRef,
        items: Map<Int, org.basaltmc.server.plugins.PluginItem>,
    ) {
    }
}

/**
 * Sending a packet by name, and the names this build has.
 *
 * The names come from [PacketNames], which is derived from what this server actually dispatches and
 * sends, so a plugin cannot register on something that would never arrive.
 */
internal object NoPackets : org.basaltmc.server.plugins.PluginPacketSender {
    override fun send(
        player: org.basaltmc.server.plugins.PluginPlayerRef,
        name: String,
        bytes: ByteArray,
    ): Boolean = false

    override fun isKnown(name: String): Boolean = PacketNames.isKnown(name)

    override fun names(): List<String> = PacketNames.all()
}

/** The real one: finds the connection and writes the bytes with the right id in front. */
internal class SessionPackets(
    private val registry: org.basaltmc.server.runtime.player.PlayerRegistry,
) : org.basaltmc.server.plugins.PluginPacketSender {
    override fun send(
        player: org.basaltmc.server.plugins.PluginPlayerRef,
        name: String,
        bytes: ByteArray,
    ): Boolean {
        val (_, id) = PacketNames.idOf(name) ?: return false
        val session = registry.snapshot().firstOrNull { it.uuid.toString() == player.uuid } ?: return false
        session.connection.sendPlayPacket(id) { buffer -> buffer.writeBytes(bytes) }
        return true
    }

    override fun isKnown(name: String): Boolean = PacketNames.isKnown(name)

    override fun names(): List<String> = PacketNames.all()
}

/** Before there is a connection to talk over. */
internal object NoChannels : org.basaltmc.server.plugins.PluginChannelSender {
    override fun send(
        player: org.basaltmc.server.plugins.PluginPlayerRef,
        channel: String,
        bytes: ByteArray,
    ): Boolean = false
}

/** The real one: a custom payload down that player's own connection. */
internal class SessionChannels(
    private val registry: org.basaltmc.server.runtime.player.PlayerRegistry,
) : org.basaltmc.server.plugins.PluginChannelSender {
    override fun send(
        player: org.basaltmc.server.plugins.PluginPlayerRef,
        channel: String,
        bytes: ByteArray,
    ): Boolean {
        val session = registry.snapshot().firstOrNull { it.uuid.toString() == player.uuid } ?: return false
        session.connection.sendPlayPacket(CustomPayloadPacket.CLIENTBOUND_ID) { buffer ->
            CustomPayloadPacket(channel, bytes).write(buffer)
        }
        return true
    }
}
