package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSource
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.GreedyStringArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.TimeArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * Runs a function's lines, and holds what `/return` did while they run.
 *
 * parity: `ServerFunctionManager`, minus its command-chain limits: `maxCommandChainLength` caps the
 * whole cascade in vanilla, and here [MAX_DEPTH] caps recursion, which is the failure that actually
 * matters (a function that calls itself).
 */
public class FunctionRunner(
    private val library: FunctionLibrary,
) {
    private var depth = 0

    /** Set by `/return` while a function is running, and cleared by the runner that reads it. */
    private var pendingReturn: Int? = null

    public fun requestReturn(value: Int) {
        pendingReturn = value
    }

    public val insideFunction: Boolean get() = depth > 0

    public fun run(
        dispatcher: CommandDispatcher,
        source: CommandSource,
        id: Identifier,
    ): Int {
        val function =
            library[id]
                ?: throw CommandSyntaxException(
                    Component.translatable("arguments.function.unknown", Component.text(id.toString())),
                )
        if (depth >= MAX_DEPTH) {
            throw CommandSyntaxException(
                Component.text("Function '$id' recursed more than $MAX_DEPTH deep and was stopped."),
            )
        }
        depth++
        val outerReturn = pendingReturn
        pendingReturn = null
        try {
            var result = 0
            for (line in function.lines) {
                result = dispatcher.execute(source, line)
                pendingReturn?.let { return it }
            }
            return result
        } finally {
            pendingReturn = outerReturn
            depth--
        }
    }

    private companion object {
        const val MAX_DEPTH = 64
    }
}

/** parity: `FunctionCommand`, minus its `with <source>` macro arguments, which need macro lines. */
public class FunctionCommand(
    private val library: FunctionLibrary,
    private val runner: FunctionRunner,
) : Command(name = "function", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("name", RegistryArgument.FUNCTION) {
            suggestsFromServer { context -> context.matchingIds(library.all().map { it.id }) }
            executes { context ->
                val id = context.get<Identifier>("name")
                val result = runner.run(context.dispatcher, context.source, id)
                context.success(
                    Component.translatable(
                        "commands.function.result",
                        Component.text(id.toString()),
                        Component.text(result.toString()),
                    ),
                    broadcastToAdmins = true,
                )
                result
            }
        }
    }
}

/**
 * parity: `ReturnCommand`. Only meaningful inside a function, which is why it refuses outside one
 * rather than pretending to have returned from the console.
 */
public class ReturnCommand(
    private val runner: FunctionRunner,
) : Command(name = "return", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("value", IntegerArgument()) {
            executes { context -> returnWith(context, context.get("value")) }
        }
        root.literal("fail") { executes { context -> returnWith(context, 0) } }
        root.literal("run") {
            argument("command", GreedyStringArgument) {
                executes { context ->
                    val result = context.dispatcher.execute(context.source, context.get("command"))
                    returnWith(context, result)
                }
            }
        }
    }

    private fun returnWith(
        context: CommandContext,
        value: Int,
    ): Int {
        if (!runner.insideFunction) {
            throw CommandSyntaxException(Component.text("/return only works inside a function."))
        }
        runner.requestReturn(value)
        return value
    }
}

/**
 * parity: `ScheduleCommand`. The queue is ticked by the server, so a scheduled function runs on the
 * tick it was asked for rather than after a wall-clock delay.
 */
public class ScheduleCommand(
    private val library: FunctionLibrary,
    private val schedule: FunctionSchedule,
) : Command(name = "schedule", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("function") {
            argument("function", RegistryArgument.FUNCTION) {
                suggestsFromServer { context -> context.matchingIds(library.all().map { it.id }) }
                argument("time", TimeArgument(min = 0)) {
                    executes { context -> schedule(context, replace = true) }
                    literal("append") { executes { context -> schedule(context, replace = false) } }
                    literal("replace") { executes { context -> schedule(context, replace = true) } }
                }
            }
        }
        root.literal("clear") {
            // parity: a greedy string, not a function name: a scheduled entry can be a `#tag` too.
            argument("function", GreedyStringArgument) {
                suggestsFromServer { context -> context.matchingIds(schedule.scheduledIds()) }
                executes { context ->
                    val id = Identifier.parse(context.get<String>("function"))
                    val cleared = schedule.clear(id)
                    if (cleared == 0) {
                        throw CommandSyntaxException(
                            Component.translatable(
                                "commands.schedule.cleared.failure",
                                Component.text(id.toString()),
                            ),
                        )
                    }
                    context.success(
                        Component.translatable(
                            "commands.schedule.cleared.success",
                            Component.text(cleared.toString()),
                            Component.text(id.toString()),
                        ),
                        broadcastToAdmins = true,
                    )
                    cleared
                }
            }
        }
    }

    private fun schedule(
        context: CommandContext,
        replace: Boolean,
    ): Int {
        val id = context.get<Identifier>("function")
        val delay = context.get<Int>("time")
        val runAt = schedule.schedule(id, delay, replace)
        context.success(
            Component.translatable(
                "commands.schedule.created.function",
                Component.text(id.toString()),
                Component.text(delay.toString()),
                Component.text(runAt.toString()),
            ),
            broadcastToAdmins = true,
        )
        return delay
    }
}

/** Functions waiting for a tick to come round. parity: `TimerQueue`, keyed by function id. */
public class FunctionSchedule {
    private class Entry(
        val id: Identifier,
        val runAtTick: Long,
    )

    private val entries = mutableListOf<Entry>()
    private var tick = 0L

    public fun schedule(
        id: Identifier,
        delayTicks: Int,
        replace: Boolean,
    ): Long {
        if (replace) entries.removeAll { it.id == id }
        val runAt = tick + delayTicks
        entries += Entry(id, runAt)
        return runAt
    }

    /** What is waiting right now, which is what `/schedule clear` completes with. */
    public fun scheduledIds(): List<Identifier> = entries.map { it.id }.distinct()

    public fun clear(id: Identifier): Int {
        val before = entries.size
        entries.removeAll { it.id == id }
        return before - entries.size
    }

    /** Advances one tick and returns whatever is due now, in the order it was scheduled. */
    public fun tick(): List<Identifier> {
        tick++
        val due = entries.filter { it.runAtTick <= tick }
        if (due.isEmpty()) return emptyList()
        entries.removeAll { it.runAtTick <= tick }
        return due.map { it.id }
    }
}

/** parity: `DataPackCommand`, over the packs [FunctionLibrary] found. */
public class DataPackCommand(
    private val library: FunctionLibrary,
) : Command(name = "datapack", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("list") {
            executes { context -> list(context, null) }
            literal("available") { executes { context -> list(context, false) } }
            literal("enabled") { executes { context -> list(context, true) } }
        }
        root.literal("enable") {
            argument("name", WordArgument) {
                suggestsFromServer { context -> context.matching(library.availablePacks.filterNot(library::isEnabled)) }
                executes { context -> setEnabled(context, true) }
            }
        }
        root.literal("disable") {
            argument("name", WordArgument) {
                suggestsFromServer { context -> context.matching(library.enabledPacks) }
                executes { context -> setEnabled(context, false) }
            }
        }
    }

    private fun list(
        context: CommandContext,
        enabledOnly: Boolean?,
    ): Int {
        val packs =
            when (enabledOnly) {
                null -> library.availablePacks
                true -> library.enabledPacks
                false -> library.availablePacks.filter { !library.isEnabled(it) }
            }
        val key = if (enabledOnly == true) "enabled" else "available"
        context.success(
            if (packs.isEmpty()) {
                Component.translatable("commands.datapack.list.$key.none")
            } else {
                Component.translatable(
                    "commands.datapack.list.$key.success",
                    Component.text(packs.size.toString()),
                    Component.text(packs.joinToString(", ")),
                )
            },
        )
        return packs.size
    }

    private fun setEnabled(
        context: CommandContext,
        enabled: Boolean,
    ): Int {
        val name = context.get<String>("name")
        if (!library.setEnabled(name, enabled)) {
            throw CommandSyntaxException(
                Component.translatable(
                    if (enabled) "commands.datapack.enable.failed" else "commands.datapack.disable.failed",
                    Component.text(name),
                ),
            )
        }
        context.success(
            Component.translatable(
                if (enabled) "commands.datapack.modify.enable" else "commands.datapack.modify.disable",
                Component.text(name),
            ),
            broadcastToAdmins = true,
        )
        return library.enabledPacks.size
    }
}
