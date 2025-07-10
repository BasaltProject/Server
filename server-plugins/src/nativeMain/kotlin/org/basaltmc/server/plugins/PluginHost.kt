package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.LuaFailure
import org.basaltmc.server.lua.LuaLimits
import org.basaltmc.server.lua.LuaSandbox
import org.basaltmc.server.lua.calls
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.toml.TomlValue

/** One plugin, loaded: its package, its states, and everything it has registered. */
internal class LoadedPlugin(
    val manifest: PluginManifest,
    val pkg: PluginPackage,
    val log: Logger,
    val config: TomlValue.Table,
    /** `plugins/<id>/`: what the plugin reads and writes, in whatever shape it decided on. */
    val folder: String,
    /** What was in that folder when it loaded, by path relative to it. */
    val files: Map<String, String>,
) {
    val registrations: PluginRegistrations = PluginRegistrations()

    /** This plugin's own `lang/`, and the `{name}` substitution that goes with it. */
    val text: PluginText = PluginText(folder, "en_us").also { it.load() }
    val instances: MutableList<PluginInstance> = mutableListOf()
    val commands: LinkedHashMap<String, CommandEntry> = LinkedHashMap()

    /** Consecutive failures. Reset by a turn that works, so an old mistake does not add up forever. */
    var strikes: Int = 0

    /** Every failure since it loaded, which is what an operator wants to see even after it recovers. */
    var failures: Int = 0

    /** Stopped after too many failures in a row: still loaded, still listed, no longer run. */
    var disabled: Boolean = false
}

/** One Lua state of one plugin: its lane. */
internal class PluginInstance(
    val lane: Int,
    val sandbox: LuaSandbox,
) {
    val onEnable: MutableList<LuaValue> = mutableListOf()
    val onDisable: MutableList<LuaValue> = mutableListOf()

    /**
     * How deep this lane is inside a decider right now.
     *
     * A decider answers a question the server is holding open, so it cannot pause: `wait` looks at
     * this and refuses rather than ending the turn and leaving the server with no answer.
     */
    var deciderDepth: Int = 0
}

/** A command of a plugin, with the function each lane answers it with. */
internal class CommandEntry(
    val spec: PluginCommandSpec,
    val handlers: MutableMap<Int, LuaValue> = mutableMapOf(),
) {
    var nextLane: Int = 0
}

/** What is loaded, for `/plugins` and for anything that has to report. */
public data class PluginStatus(
    val id: String,
    val name: String,
    val version: String,
    val origin: String,
    val editable: Boolean,
    val lanes: Int,
    val registrations: Int,
    /** Failures in a row right now: this is what turns a plugin off when it reaches the limit. */
    val errorStreak: Int,
    /** Failures since it loaded, which do not go away when it recovers. */
    val errors: Int,
    val disabled: Boolean,
    val commands: List<String>,
)

/** The outcome of a load pass: what came up, and what did not and why. */
public data class PluginLoadReport(
    val loaded: List<String>,
    val refused: List<Pair<String, String>>,
)

/**
 * Loads plugins, runs them, and takes them back out again.
 *
 * Three things it will not do, and they are the design rather than omissions. It never runs plugin
 * code anywhere but on an owner the server gave it, so a plugin never has to think about threads.
 * It never lets a plugin register anything it cannot retire, which is what makes unloading whole.
 * And it never lets a plugin's mistake past its own turn: an error is logged against the plugin and
 * the tick carries on.
 */
public class PluginHost(
    internal val services: PluginServices,
    /**
     * How many states each plugin gets. One in the default configuration, which is what makes the
     * lane invisible there; more only when the regional scheduler runs more than one at a time, or
     * when a developer asks for more to shake out a plugin that assumes there is only one.
     */
    private val lanes: Int = 1,
    private val limits: LuaLimits = LuaLimits(),
    private val strikeLimit: Int = 3,
) {
    private val log = Logger("plugins")
    private val plugins = LinkedHashMap<String, LoadedPlugin>()

    /** Events a plugin made up, by name. Nothing declares them: whoever emits one decides it exists. */
    private val announcements = LinkedHashMap<String, MutableList<Announcement>>()

    /** What one plugin offers another. See [PluginServiceRegistry] for why it is not a shared table. */
    internal val serviceRegistry: PluginServiceRegistry = PluginServiceRegistry()

    /** Packet listeners, and the promise that a packet nobody watches is never opened. */
    public val packets: PluginPacketRegistry =
        PluginPacketRegistry { plugin, fn, packet -> deliverPacket(plugin, fn, packet) }

    /** Who is listening on which channel, by channel name. */
    private val channels = LinkedHashMap<String, MutableList<Announcement>>()

    /** Who is drawing the sidebar, and who is next if they let go. */
    public val surfaces: PluginSurfaceRegistry = PluginSurfaceRegistry { message -> log.warn { message } }

    /** Who is listening to what. The server fires; nothing else touches it. */
    public val events: PluginEventBus =
        PluginEventBus().also { bus ->
            for (type in PluginEventTypes.ALL) bus.declare(type)
        }

    /**
     * Whether plugin code may run on the calling thread right now.
     *
     * Set by the server to point at the owner plugins run on. A decider has to answer where the
     * question was asked, so if that is not this owner it cannot run at all; an observer can be
     * handed to the owner and run later, which is exactly what it is for.
     */
    public var onPluginThread: () -> Boolean = { true }
    private var warnedAboutThread = false

    public val statuses: List<PluginStatus>
        get() =
            plugins.values.map { plugin ->
                PluginStatus(
                    id = plugin.manifest.id,
                    name = plugin.manifest.name,
                    version = plugin.manifest.version,
                    origin = plugin.pkg.origin,
                    editable = plugin.pkg.editable,
                    lanes = plugin.instances.size,
                    registrations = plugin.registrations.size,
                    errorStreak = plugin.strikes,
                    errors = plugin.failures,
                    disabled = plugin.disabled,
                    commands = plugin.commands.values.map { it.spec.usage },
                )
            }

    public fun isLoaded(id: String): Boolean = id in plugins

    /**
     * Loads everything in `plugins/` that is not loaded yet, in dependency order.
     *
     * Called at startup, and again for a hot-plug: a package dropped into the folder while the
     * server is full comes up the same way, which is why there is no separate path for it.
     */
    public fun loadAll(): PluginLoadReport {
        val candidates = mutableListOf<Pair<PluginPackage, PluginManifest>>()
        val refused = mutableListOf<Pair<String, String>>()
        for (pkg in discoverPackages(services.paths.packages)) {
            val text = pkg.readText(PackageLayout.MANIFEST)
            if (text == null) {
                refused += pkg.origin to "it has no ${PackageLayout.MANIFEST}"
                continue
            }
            val manifest =
                try {
                    PluginManifest.parse(text)
                } catch (e: ManifestException) {
                    refused += pkg.origin to (e.message ?: "its manifest could not be read")
                    continue
                }
            val clash = candidates.firstOrNull { it.second.id == manifest.id }
            if (clash != null) {
                refused += pkg.origin to "'${manifest.id}' is also at ${clash.first.origin}"
                continue
            }
            if (manifest.id in plugins) continue
            candidates += pkg to manifest
        }

        val loaded = mutableListOf<String>()
        for (decision in PluginOrder.resolve(candidates.map { it.second })) {
            when (decision) {
                is PluginOrdering.Refused -> refused += decision.manifest.id to decision.reason
                is PluginOrdering.Load -> {
                    val pkg = candidates.first { it.second.id == decision.manifest.id }.first
                    if (start(decision.manifest, pkg)) {
                        loaded += decision.manifest.id
                    } else {
                        refused +=
                            decision.manifest.id to "it failed while starting"
                    }
                }
            }
        }
        for ((what, why) in refused) log.warn { "$what did not load: $why" }
        return PluginLoadReport(loaded, refused)
    }

    /** Unloads [id], and whatever depends on it first. Returns what came out, in the order it did. */
    public fun unload(id: String): List<String> {
        val plugin = plugins[id] ?: return emptyList()
        val order = PluginOrder.dependents(id, plugins.values.map { it.manifest }) + id
        if (order.size > 1) {
            log.info { "unloading $id also unloads ${order.dropLast(1).joinToString(", ")}, which depend on it" }
        }
        val gone = mutableListOf<String>()
        for (each in order) {
            val target = plugins[each] ?: continue
            stop(target)
            plugins.remove(each)
            gone += each
        }
        return gone
    }

    /**
     * Reloads [id] from where it came from.
     *
     * A folder is read again from disk, so saving a file and reloading is the whole loop. Anything
     * that depended on it comes back with it, in the order it went out.
     */
    public fun reload(id: String): PluginLoadReport {
        val gone = unload(id)
        if (gone.isEmpty()) return PluginLoadReport(emptyList(), listOf(id to "it is not loaded"))
        return loadAll()
    }

    /** Everything out, dependents first, for a shutdown that leaves nothing running. */
    public fun unloadAll() {
        for (id in plugins.keys.toList().asReversed()) unload(id)
    }

    /**
     * Stops whatever plugin code is running right now.
     *
     * Meant for a watchdog on another thread: the instruction ceiling catches a plugin spinning in
     * Lua, and this catches one stuck in something the host is doing for it.
     */
    public fun interruptAll() {
        for (plugin in plugins.values) for (instance in plugin.instances) instance.sandbox.interrupt()
    }

    private fun start(
        manifest: PluginManifest,
        pkg: PluginPackage,
    ): Boolean {
        val log = Logger("plugin/${manifest.id}")
        val source = pkg.readText(manifest.main)
        // A package that only exports modules is a library, and a library has nothing to run.
        val library = source == null && manifest.exports.modules.isNotEmpty()
        if (source == null && !library) {
            log.error { "${manifest.id} says its code is at ${manifest.main} and there is nothing there" }
            return false
        }
        val folderPath = pkg.workingFolder ?: services.paths.folder(manifest.id)
        val folder = PluginFolder(pkg, manifest, folderPath)
        val files: Map<String, String>
        val config =
            try {
                folder.materialize { log.info { it } }
                files = folder.index()
                PluginConfig().load(files)
            } catch (e: PluginLoadException) {
                log.error { "${manifest.id} did not load: ${e.message}" }
                return false
            }
        val plugin = LoadedPlugin(manifest, pkg, log, config, folderPath, files)

        // Compiled once and bound into each lane: a prototype is finished code and shared safely,
        // and it is the compile that costs, not the libraries.
        val chunkName = "@${manifest.id}/${manifest.main}"
        val first = sandboxFor(plugin, 0)
        val prototype =
            if (source == null) {
                null
            } else {
                try {
                    first.compile(source, chunkName)
                } catch (e: net.blueva.luak.LuaError) {
                    log.error { "${manifest.id} does not compile: ${e.message}" }
                    return false
                }
            }

        plugins[manifest.id] = plugin
        for (lane in 0 until lanes) {
            val sandbox = if (lane == 0) first else sandboxFor(plugin, lane)
            val instance = PluginInstance(lane, sandbox)
            plugin.instances += instance
            PluginApi.install(instance, plugin, this, lanes)
            if (prototype != null) enter(plugin, instance, sandbox.bind(prototype), "load")
        }
        // A plugin whose own file failed while running has not loaded, whatever it managed to
        // register before it fell over: half a plugin is worse than none, and the log already
        // says what went wrong.
        if (plugin.failures > 0 || plugin.disabled) {
            log.error { "${manifest.id} failed while loading and is off" }
            stop(plugin)
            plugins.remove(manifest.id)
            return false
        }
        for (instance in plugin.instances) {
            for (handler in instance.onEnable) enter(plugin, instance, handler, "plugin.enable")
        }
        log.info { "${manifest.name} ${manifest.version} loaded from ${pkg.origin}" }
        return true
    }

    private fun sandboxFor(
        plugin: LoadedPlugin,
        lane: Int,
    ): LuaSandbox =
        LuaSandbox(
            name = plugin.manifest.id,
            limits = limits,
            // Two lanes of the same plugin must not walk the same random sequence.
            seed =
                plugin.manifest.id
                    .hashCode()
                    .toLong() * 31 + lane,
            sources = { name -> moduleSource(plugin, name) },
            printLine = { line -> plugin.log.info { line } },
        )

    /**
     * Where `require` looks: this plugin's own `src/`, then another plugin's exported modules.
     *
     * `require("economy.api")` arrives here as `economy/api.lua`. The code is read from its owner's
     * package but loaded into the caller's state, and that is deliberate: the call
     * costs no hop, runs on the caller's budget, and a failure inside it is the caller's failure.
     * The other side of that bargain is that an exported module shares no state, so anything with
     * live data belongs behind a service instead.
     */
    private fun moduleSource(
        plugin: LoadedPlugin,
        requested: String,
    ): ByteArray? {
        plugin.pkg.read(PackageLayout.SOURCE + requested)?.let { return it }
        val id = requested.substringBefore('/')
        if (id == requested) return null
        // Its own code under its own id: `require("me.util")` from inside `me` is the same file as
        // `require("util")`, and a library that names its own modules the way everyone else does
        // should not have to write them differently.
        if (id == plugin.manifest.id) {
            return plugin.pkg.read(PackageLayout.SOURCE + requested.removePrefix("$id/"))
        }
        val owner = plugins[id] ?: return null
        val module = requested.removePrefix("$id/").removeSuffix(".lua")
        if (!owner.manifest.exports.exports(module)) {
            // Not "no such module": it is there and it is private, and saying which is the
            // difference between a one-line fix and an afternoon.
            throw net.blueva.luak.LuaError(
                "'$id.$module' is not exported by $id" +
                    if (owner.manifest.exports.modules
                            .isEmpty()
                    ) {
                        ", which exports nothing"
                    } else {
                        "; it exports ${owner.manifest.exports.modules.joinToString(", ")}"
                    },
            )
        }
        return owner.pkg.read(PackageLayout.SOURCE + requested.removePrefix("$id/"))
    }

    private fun stop(plugin: LoadedPlugin) {
        for (instance in plugin.instances) {
            for (handler in instance.onDisable) enter(plugin, instance, handler, "plugin.disable")
        }
        plugin.registrations.retireAll { what, error ->
            plugin.log.warn { "could not retire $what: ${error.message}" }
        }
        // The token above already took them out one by one; this is the belt to its braces, so a
        // handler cannot outlive its plugin even if a registration was written wrong.
        events.removeAll(plugin.manifest.id)
        plugin.instances.clear()
        plugin.commands.clear()
        plugin.log.info { "${plugin.manifest.name} unloaded" }
    }

    /**
     * Registers one event handler of one plugin, for one lane.
     *
     * The handler is the lane's, not the plugin's: with more than one lane a plugin has one state
     * per lane and each has its own function, and the fire picks a lane the way a command does.
     */
    internal fun registerEventHandler(
        plugin: LoadedPlugin,
        instance: PluginInstance,
        type: PluginEventType,
        priority: PluginEventPriority,
        observer: Boolean,
        wantsCancelled: Boolean,
        filters: Map<String, Any?>,
        handler: LuaValue,
    ) {
        var view: PluginEventView? = null
        var boundTo: PluginEvent? = null
        val entry =
            PluginEventHandler(
                pluginId = plugin.manifest.id,
                lane = instance.lane,
                priority = priority,
                observer = observer,
                wantsCancelled = wantsCancelled,
                filters = filters,
            ) { event ->
                if (view == null || boundTo !== event) {
                    boundTo = event
                    view =
                        PluginEventView(
                            event,
                            { message -> event.cancel(message) },
                            { key -> objectFor(plugin, event, key) },
                            { message, arguments -> plugin.text.resolve(message, arguments) },
                        )
                }
                val bound = view
                if (bound != null) {
                    bound.bind()
                    dispatch(plugin, instance, handler, event, bound.table, observer)
                }
            }
        val handle = events.add(entry, type)
        plugin.registrations.add("${if (observer) "watch" else "on"}(${type.name})") { handle.cancel() }
    }

    /**
     * Runs one handler for one fire.
     *
     * A decider runs here and now, because the server is waiting for the answer: if this is not the
     * thread plugins run on, it does not run at all, and the reason is said once rather than every
     * tick. An observer is handed to the owner instead, which is the whole point of it being one.
     */
    private fun dispatch(
        plugin: LoadedPlugin,
        instance: PluginInstance,
        handler: LuaValue,
        event: PluginEvent,
        view: LuaValue,
        observer: Boolean,
    ) {
        if (plugin.disabled) return
        if (observer) {
            // Copied out, and this is the one place dispatch allocates: an observer runs after the
            // fact, and by then the shared event has been filled in again by whatever happened
            // next. A copy per fire is what buys it the right to take its time.
            val snapshot = PluginEvent(event.type)
            for (index in event.type.fields.indices) snapshot.set(index, event.get(index))
            services.tasks.after(0) {
                val laneView = PluginEventView(snapshot, { }, { key -> objectFor(plugin, snapshot, key) })
                snapshot.begin()
                laneView.bind()
                // Never ended: the copy is nobody else's, so it stays readable across a `wait`
                // rather than going stale under a handler that was allowed to pause.
                enter(plugin, instance, handler, "watch ${event.type.name}", Lua.varargs(laneView.table))
            }
            return
        }
        if (!onPluginThread()) {
            if (!warnedAboutThread) {
                warnedAboutThread = true
                log.warn {
                    "a ${event.type.name} was fired from a thread plugins do not run on, so no " +
                        "decider saw it. Either the server is running regions on more than one " +
                        "thread (the known limit until lanes follow them), or that event is being " +
                        "fired from the wrong place and it is a bug here"
                }
            }
            return
        }
        instance.deciderDepth++
        try {
            enter(plugin, instance, handler, "on ${event.type.name}", Lua.varargs(view))
        } finally {
            instance.deciderDepth--
        }
    }

    /** Registers one command once, and remembers which function each lane answers it with. */
    internal fun registerCommand(
        plugin: LoadedPlugin,
        instance: PluginInstance,
        spec: PluginCommandSpec,
        handler: LuaValue,
    ) {
        val key = spec.literals.joinToString(" ")
        val existing = plugin.commands[key]
        if (existing != null) {
            existing.handlers[instance.lane] = handler
            return
        }
        val entry = CommandEntry(spec)
        entry.handlers[instance.lane] = handler
        plugin.commands[key] = entry
        val handle =
            services.commands.register(plugin.manifest.id, spec) { call ->
                runCommand(plugin, entry, call)
            }
        plugin.registrations.add("command /${spec.usage}") {
            handle.cancel()
            plugin.commands.remove(key)
        }
    }

    private fun runCommand(
        plugin: LoadedPlugin,
        entry: CommandEntry,
        call: PluginCommandCall,
    ) {
        val instance = laneFor(plugin, entry) ?: return
        val handler = entry.handlers[instance.lane] ?: return
        val arguments = mutableListOf<LuaValue>(sourceTable(plugin, call))
        for (argument in entry.spec.arguments) arguments += luaOf(call.argument(argument.name))
        enter(plugin, instance, handler, "/${entry.spec.name}", Lua.varargs(*arguments.toTypedArray()))
    }

    /** The lane that answers the next fire of this event type, the same way a command picks one. */
    internal fun laneForEvent(plugin: LoadedPlugin): PluginInstance? = plugin.instances.firstOrNull()

    /**
     * Which lane answers this one.
     *
     * With one lane there is nothing to decide. With more, they take turns, which is on purpose: a
     * plugin that assumed one state sees it here, in a test, rather than in production the first
     * time an operator turns regions on.
     */
    private fun laneFor(
        plugin: LoadedPlugin,
        entry: CommandEntry,
    ): PluginInstance? {
        if (plugin.instances.isEmpty()) return null
        val lane = entry.nextLane % plugin.instances.size
        entry.nextLane = lane + 1
        return plugin.instances.firstOrNull { it.lane == lane } ?: plugin.instances.first()
    }

    private fun sourceTable(
        plugin: LoadedPlugin,
        call: PluginCommandCall,
    ): LuaValue {
        val table = Lua.table()
        table.set("name", call.source)
        table.set("is_console", Lua.of(call.isConsole))
        table.set(
            "send",
            org.basaltmc.server.lua.HostFunction("send") { args ->
                // Both `src:send(text)` and `src.send(text)` end up here; the first passes the
                // table as well, and a plugin should not have to care which it wrote.
                val first = firstArgument(args)
                call.reply(plugin.text.resolve(args.tojstring(first), args.arg(first + 1)))
                Lua.NONE
            },
        )
        return table
    }

    private fun luaOf(value: Any?): LuaValue =
        when (value) {
            null -> Lua.NIL
            is String -> Lua.of(value)
            is Long -> Lua.of(value)
            is Int -> Lua.of(value)
            is Double -> Lua.of(value)
            is Boolean -> Lua.of(value)
            else -> Lua.of(value.toString())
        }

    /** One plugin waiting on an event another plugin made up. */
    private class Announcement(
        val plugin: LoadedPlugin,
        val fn: LuaValue,
    )

    /**
     * `on("arena:round_start", fn)` for a name no event type has.
     *
     * The loosest of the ways to talk: the one emitting does not know whether anybody
     * is there, and the one listening does not need the other to be loaded, or ever to exist.
     */
    internal fun announce(
        plugin: LoadedPlugin,
        name: String,
        fn: LuaValue,
    ) {
        val list = announcements.getOrPut(name) { mutableListOf() }
        val entry = Announcement(plugin, fn)
        list += entry
        plugin.registrations.add("on($name)") {
            list.remove(entry)
            if (list.isEmpty()) announcements.remove(name)
        }
    }

    /**
     * `emit("arena:round_start", { ... })`.
     *
     * The table crosses by copy, the same as a service: what reaches each listener is its
     * own, so two plugins cannot end up holding the same mutable table, and each one's work is paid
     * by its own budget.
     */
    internal fun emit(
        name: String,
        payload: LuaValue,
    ): Int {
        val listeners = announcements[name] ?: return 0
        val copied = PluginValues.out(payload)
        var reached = 0
        for (entry in listeners.toList()) {
            val plugin = entry.plugin
            if (plugin.disabled) continue
            val instance = plugin.instances.firstOrNull() ?: continue
            instance.sandbox.refill()
            instance.deciderDepth++
            try {
                entry.fn.call(PluginValues.into(copied, this, plugin))
                plugin.strikes = 0
                reached++
            } catch (e: net.blueva.luak.LuaError) {
                report(
                    plugin,
                    org.basaltmc.server.lua
                        .LuaFailure(plugin.manifest.id, e),
                    name,
                )
            } finally {
                instance.deciderDepth--
            }
        }
        return reached
    }

    /**
     * Somebody clicked a menu: runs the plugin's handler with who and where.
     *
     * The handler gets one table rather than two arguments because a click grows fields over time
     * (the button, whether it was a shift-click) and a table can take them without moving anybody's
     * parameters around.
     */
    internal fun clicked(
        plugin: LoadedPlugin,
        handler: LuaValue,
        player: PluginPlayerRef,
        slot: Int,
    ) {
        val instance = plugin.instances.firstOrNull() ?: return
        if (plugin.disabled) return
        instance.sandbox.refill()
        instance.deciderDepth++
        try {
            val event = Lua.table()
            event.set("player", playerObject(player, services.storage, plugin.manifest.id, plugin.text))
            event.set("slot", slot)
            handler.call(event)
            plugin.strikes = 0
        } catch (e: net.blueva.luak.LuaError) {
            report(
                plugin,
                org.basaltmc.server.lua
                    .LuaFailure(plugin.manifest.id, e),
                "menu click",
            )
        } finally {
            instance.deciderDepth--
        }
    }

    /** `channel("arena:main", fn)`: what arrives on a channel from this player's client. */
    internal fun listenOn(
        plugin: LoadedPlugin,
        channel: String,
        fn: LuaValue,
    ) {
        val list = channels.getOrPut(channel) { mutableListOf() }
        val entry = Announcement(plugin, fn)
        list += entry
        plugin.registrations.add("channel($channel)") {
            list.remove(entry)
            if (list.isEmpty()) channels.remove(channel)
        }
    }

    public fun listeningOnChannel(channel: String): Boolean = channels.containsKey(channel)

    /** A payload arrived: hands the bytes to whoever asked for that channel. */
    public fun channelMessage(
        channel: String,
        playerUuid: String,
        bytes: ByteArray,
    ) {
        val listeners = channels[channel] ?: return
        val player = services.players.byUuid(playerUuid)
        for (entry in listeners.toList()) {
            val plugin = entry.plugin
            if (plugin.disabled) continue
            val instance = plugin.instances.firstOrNull() ?: continue
            instance.sandbox.refill()
            instance.deciderDepth++
            try {
                val list = Lua.table()
                for ((index, byte) in bytes.withIndex()) list.set(index + 1, Lua.of(byte.toInt() and 0xff))
                entry.fn.call(
                    player?.let { playerObject(it, services.storage, plugin.manifest.id, plugin.text) } ?: Lua.NIL,
                    list,
                )
                plugin.strikes = 0
            } catch (e: net.blueva.luak.LuaError) {
                report(
                    plugin,
                    org.basaltmc.server.lua
                        .LuaFailure(plugin.manifest.id, e),
                    "channel $channel",
                )
            } finally {
                instance.deciderDepth--
            }
        }
    }

    /** Somebody right-clicked an NPC: runs the plugin's handler with the player who did. */
    internal fun npcClicked(
        plugin: LoadedPlugin,
        handler: LuaValue,
        playerUuid: String,
    ) {
        val player = services.players.byUuid(playerUuid) ?: return
        ask(plugin, handler, player, "npc click")
    }

    /** Hands one packet to one listener, on its own state and its own budget. */
    private fun deliverPacket(
        plugin: LoadedPlugin,
        fn: LuaValue,
        packet: PluginPacket,
    ) {
        val instance = plugin.instances.firstOrNull() ?: return
        if (plugin.disabled) return
        instance.sandbox.refill()
        instance.deciderDepth++
        try {
            fn.call(PluginPacketView(packet, services.players, this, plugin).table)
            plugin.strikes = 0
        } catch (e: net.blueva.luak.LuaError) {
            report(
                plugin,
                org.basaltmc.server.lua
                    .LuaFailure(plugin.manifest.id, e),
                "packet ${packet.name}",
            )
        } finally {
            instance.deciderDepth--
        }
    }

    internal fun answer(
        plugin: LoadedPlugin,
        fn: LuaValue,
        player: PluginPlayerRef?,
    ): String = ask(plugin, fn, player, "placeholder")?.tojstring().orEmpty()

    /**
     * Calls a plugin function that has to answer *now* and gives back what it said.
     *
     * For a placeholder or a surface, both of which the server renders in the middle of drawing
     * something: there is nowhere to pause to, so this is the one entry that runs synchronously. A
     * plugin that fails here gets a strike and the thing renders without it, rather than the whole
     * screen failing.
     */
    internal fun ask(
        plugin: LoadedPlugin,
        fn: LuaValue,
        player: PluginPlayerRef?,
        why: String,
    ): LuaValue? {
        val instance = plugin.instances.firstOrNull() ?: return null
        if (plugin.disabled) return null
        instance.sandbox.refill()
        instance.deciderDepth++
        return try {
            val argument =
                player?.let { playerObject(it, services.storage, plugin.manifest.id, plugin.text) } ?: Lua.NIL
            val answered = fn.call(argument)
            plugin.strikes = 0
            answered
        } catch (e: net.blueva.luak.LuaError) {
            report(
                plugin,
                org.basaltmc.server.lua
                    .LuaFailure(plugin.manifest.id, e),
                why,
            )
            null
        } finally {
            instance.deciderDepth--
        }
    }

    /**
     * `e.player` and `e.entity`: the object behind what the event carries by name and id.
     *
     * Null for anything else, which is what tells the view to look at the fields instead. A player
     * who has left between the fire and the read answers nil rather than a handle to nobody.
     */
    private fun objectFor(
        plugin: LoadedPlugin,
        event: PluginEvent,
        key: String,
    ): LuaValue? =
        when (key) {
            "player" ->
                (
                    event.get(
                        "player_name",
                    ) as? String
                )?.let { services.players.byName(it) }
                    ?.let { playerObject(it, services.storage, plugin.manifest.id, plugin.text) }
            "entity" ->
                (event.get("entity_id") as? Int)
                    ?.let { id ->
                        val world = event.get("world") as? String
                        val worlds =
                            if (world !=
                                null
                            ) {
                                listOfNotNull(services.worlds.byName(world))
                            } else {
                                services.worlds.all()
                            }
                        worlds.firstNotNullOfOrNull { it.entities().firstOrNull { entity -> entity.id == id } }
                    }?.let { entityObject(it) }

            // Only for the events that carry a place: a chat message has no `pos` and saying so
            // is better than handing back one made of zeroes.
            "pos" -> {
                val world = event.get("world") as? String
                val x = event.get("x")
                if (world == null || x == null) {
                    null
                } else {
                    positionObject(world, numberOf(x), numberOf(event.get("y")), numberOf(event.get("z")))
                }
            }

            else -> null
        }

    private fun numberOf(value: Any?): Double =
        when (value) {
            is Double -> value
            is Int -> value.toDouble()
            is Long -> value.toDouble()
            is Float -> value.toDouble()
            else -> 0.0
        }

    internal fun pluginById(id: String): LoadedPlugin? = plugins[id]

    /**
     * One call across the boundary between two plugins.
     *
     * It runs in the provider's state, on the provider's budget, and what goes in and comes out are
     * copies. A failure there is the provider's strike and an ordinary error for whoever called: a
     * plugin should not be taken down by a service it merely used.
     */
    internal fun callService(
        provider: LoadedPlugin,
        function: LuaValue,
        args: Varargs,
    ): Varargs {
        if (provider.disabled) throw net.blueva.luak.LuaError("the plugin behind this service is off")
        val instance = provider.instances.firstOrNull() ?: return Lua.NONE
        val copied = PluginValues.arguments(args)
        instance.sandbox.refill()
        instance.deciderDepth++
        return try {
            val inside = copied.map { PluginValues.into(it, this, provider) }
            val answered =
                when (inside.size) {
                    0 -> function.call()
                    1 -> function.call(inside[0])
                    2 -> function.call(inside[0], inside[1])
                    else -> function.call(inside[0], inside[1], inside[2])
                }
            provider.strikes = 0
            answered ?: Lua.NIL
        } catch (e: net.blueva.luak.LuaError) {
            report(
                provider,
                org.basaltmc.server.lua
                    .LuaFailure(provider.manifest.id, e),
                "service",
            )
            throw net.blueva.luak.LuaError("the service failed inside ${provider.manifest.id}: ${e.message}")
        } finally {
            instance.deciderDepth--
        }
    }

    /** The player behind a Lua player object, for a call that takes one. */
    internal fun playerOf(value: LuaValue): PluginPlayerRef? {
        if (!value.istable()) return null
        val name = (value.get("name") ?: Lua.NIL).takeIf { it.isstring() }?.tojstring() ?: return null
        return services.players.byName(name)
    }

    /** Runs one thing a plugin asked for, and keeps its failure inside its own turn. */
    internal fun enter(
        plugin: LoadedPlugin,
        instance: PluginInstance,
        fn: LuaValue,
        why: String,
        args: Varargs = Lua.NONE,
    ) {
        if (plugin.disabled) return
        instance.sandbox.calls().start(fn, args) { failure ->
            if (failure == null) {
                plugin.strikes = 0
            } else {
                report(plugin, failure, why)
            }
        }
    }

    /** The other half of [enter]: picking a paused turn back up, with the same care. */
    internal fun resuming(
        plugin: LoadedPlugin,
        resume: () -> Unit,
    ) {
        if (plugin.disabled) return
        try {
            resume()
            plugin.strikes = 0
        } catch (e: LuaFailure) {
            report(plugin, e, "wait")
        }
    }

    private fun report(
        plugin: LoadedPlugin,
        failure: LuaFailure,
        why: String,
    ) {
        plugin.strikes++
        plugin.failures++
        val reason =
            when {
                failure.outOfBudget -> "ran out of its turn"
                failure.outOfMemory -> "asked for more memory than it may have"
                else -> "failed"
            }
        plugin.log.error { "$why $reason: ${failure.error.message}" }
        if (plugin.strikes >= strikeLimit) {
            plugin.disabled = true
            // A plugin that is off must not leave a frozen sidebar on everyone's screen.
            surfaces.yield(plugin.manifest.id, "it is off after $strikeLimit failures in a row")
            plugin.log.error {
                "${plugin.manifest.id} is off after $strikeLimit failures in a row; " +
                    "fix it and run /plugins reload ${plugin.manifest.id}"
            }
        }
    }
}
