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
    internal val log = Logger("plugins")
    private val plugins = LinkedHashMap<String, LoadedPlugin>()

    /** Events a plugin made up, by name. Nothing declares them: whoever emits one decides it exists. */
    private val pluginAnnouncements = PluginAnnouncements()

    private val commandDispatch = PluginCommandDispatch(this)
    private val eventDispatch = PluginEventDispatch(this)

    /** What one plugin offers another. See [PluginServiceRegistry] for why it is not a shared table. */
    internal val serviceRegistry: PluginServiceRegistry = PluginServiceRegistry()

    /** Packet listeners, and the promise that a packet nobody watches is never opened. */
    public val packets: PluginPacketRegistry =
        PluginPacketRegistry { plugin, fn, packet -> deliverPacket(plugin, fn, packet) }

    /**
     * What the operator has allowed each plugin beyond its own sandbox.
     *
     * Asked for in the manifest and granted here, never the other way around: a plugin cannot talk
     * its way into one at runtime, and one it asked for and did not get is said once at load rather
     * than failing later in a way that reads like a bug.
     */
    public var allowed: (String) -> Set<PluginPermission> = { emptySet() }

    /** Who is listening on which channel, by channel name. */
    private val channelHost = PluginChannelHost()

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

    /** Marked once, so the two threads are reading the same rising number. */
    internal val clock =
        kotlin.time.TimeSource.Monotonic
            .markNow()

    /** Who is inside a plugin right now, and the means to cut it off if it stays too long. */
    internal val watchdog = PluginWatchdog(clock)

    /**
     * Where a plugin's time is recorded, so `/timings` names it beside the server's own work.
     *
     * A plugin that costs a millisecond a tick should be as visible as the chunk loader, and by the
     * plugin's own id: that is the whole difference between "the server is laggy" and "arena is".
     */
    public var timings: org.basaltmc.wire.common.timings.Timings? = null

    /**
     * Cuts off a plugin that has been inside the server for longer than [limitMillis].
     *
     * The instruction ceiling catches one spinning in Lua; this catches one stuck in something the
     * host is doing for it, which the counter never sees. Answers who was cut off, so the caller
     * can say it out loud: a plugin cannot lag the server without the server naming it.
     */
    public fun interruptIfStuck(limitMillis: Long): String? = watchdog.interruptIfStuck(limitMillis, plugins)

    /**
     * Stops whatever plugin code is running right now.
     *
     * Meant for a watchdog on another thread: the instruction ceiling catches a plugin spinning in
     * Lua, and this catches one stuck in something the host is doing for it.
     */
    public fun interruptAll() {
        watchdog.interruptAll(plugins.values)
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

        // Said once, at load, and not at the first call: a plugin that asked for something it did
        // not get should be visible to the operator before it misbehaves, and the plugin itself
        // simply does not get the global.
        val granted = allowed(manifest.id)
        val missing = manifest.capabilities.asks() - granted
        if (missing.isNotEmpty()) {
            log.warn {
                "${manifest.id} asks for ${missing.joinToString(", ") { it.permissionName }} and the " +
                    "operator has not granted it. Add ${manifest.id} = " +
                    "[${missing.joinToString(", ") { "\"${it.permissionName}\"" }}] under [plugins.allow] " +
                    "in config/server.toml to allow it."
            }
        }

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
     * The data of plugins that are no longer installed.
     *
     * Deleting it would be losing somebody's work because a folder was moved for an afternoon, so
     * this only reports. Reinstalling the plugin tomorrow has to give it all back intact.
     */
    public fun orphanedData(): Map<String, Long> {
        val installed = plugins.keys
        return services.storage.namespaces().filterKeys { it !in installed }
    }

    /**
     * Deletes what one uninstalled plugin kept, and answers how many rows went.
     *
     * Refuses while the plugin is loaded, because purging under a running plugin is deleting what it
     * is still using. Null is that refusal.
     */
    public fun purge(pluginId: String): Long? {
        if (pluginId in plugins) return null
        val rows = services.storage.purge(pluginId)
        log.warn { "purged $rows row(s) of '$pluginId'" }
        return rows
    }

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
        eventDispatch.registerEventHandler(
            plugin,
            instance,
            type,
            priority,
            observer,
            wantsCancelled,
            filters,
            handler,
        )
    }

    /** Registers one command once, and remembers which function each lane answers it with. */
    internal fun registerCommand(
        plugin: LoadedPlugin,
        instance: PluginInstance,
        spec: PluginCommandSpec,
        handler: LuaValue,
    ) {
        commandDispatch.registerCommand(plugin, instance, spec, handler)
    }

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
        pluginAnnouncements.announce(plugin, name, fn)
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
    ): Int = pluginAnnouncements.emit(this, name, payload)

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
        watchdog.mark(plugin.manifest.id)
        val startedAt = clock.elapsedNow().inWholeNanoseconds
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
            watchdog.clear()
            timings?.of("plugin.${plugin.manifest.id}")?.record(clock.elapsedNow().inWholeNanoseconds - startedAt)
        }
    }

    /** `channel("arena:main", fn)`: what arrives on a channel from this player's client. */
    internal fun listenOn(
        plugin: LoadedPlugin,
        channel: String,
        fn: LuaValue,
    ) {
        channelHost.listenOn(plugin, channel, fn)
    }

    public fun listeningOnChannel(channel: String): Boolean = channelHost.listeningOnChannel(channel)

    /** A payload arrived: hands the bytes to whoever asked for that channel. */
    public fun channelMessage(
        channel: String,
        playerUuid: String,
        bytes: ByteArray,
    ) {
        channelHost.channelMessage(this, channel, playerUuid, bytes)
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
        watchdog.mark(plugin.manifest.id)
        val startedAt = clock.elapsedNow().inWholeNanoseconds
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
            watchdog.clear()
            timings?.of("plugin.${plugin.manifest.id}")?.record(clock.elapsedNow().inWholeNanoseconds - startedAt)
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
        watchdog.mark(plugin.manifest.id)
        val startedAt = clock.elapsedNow().inWholeNanoseconds
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
            watchdog.clear()
            timings?.of("plugin.${plugin.manifest.id}")?.record(clock.elapsedNow().inWholeNanoseconds - startedAt)
        }
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
        // The work is the provider's, so it is the provider the watchdog would name.
        watchdog.mark(provider.manifest.id)
        val startedAt = clock.elapsedNow().inWholeNanoseconds
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
            watchdog.clear()
            timings
                ?.of("plugin.${provider.manifest.id}")
                ?.record(clock.elapsedNow().inWholeNanoseconds - startedAt)
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

    internal fun report(
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
