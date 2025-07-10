package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.Lua.field
import org.basaltmc.server.lua.WaitingFunction
import org.basaltmc.server.lua.pauseUntil
import org.basaltmc.wire.common.toml.TomlValue
import org.basaltmc.wire.io.FileIO

/**
 * What a plugin sees when it starts: the whole of the API of this stage, put into its globals.
 *
 * Everything registered here goes through the plugin's [PluginRegistrations], so unloading it takes
 * all of it back out. Nothing is a singleton a plugin could reach around the host with.
 */
internal object PluginApi {
    fun install(
        instance: PluginInstance,
        plugin: LoadedPlugin,
        host: PluginHost,
        lanes: Int,
    ) {
        val globals = instance.sandbox.globals
        globals.set("plugin", describe(plugin, instance, lanes))
        globals.set("log", logTable(plugin))
        globals.set("config", tableOf(plugin.config))
        globals.set("files", fileTable(plugin))
        globals.set("wait", Wait(host, plugin, instance))
        globals.set("later", later(host, plugin, instance))
        globals.set("every", every(host, plugin, instance))
        globals.set("cmd", command(host, plugin, instance))
        globals.set("players", playersTable(host, plugin))
        globals.set("worlds", worldsTable(host, plugin))
        globals.set("at", at(host, plugin, instance))
        globals.set("pos", position())
        globals.set("placeholder", placeholder(host, plugin))
        globals.set("fill", fill(host))
        globals.set("broadcast", broadcast(host, plugin))
        globals.set("t", translate(plugin))
        globals.set("service", service(host, plugin))
        globals.set("store", storeTable(host, plugin))
        globals.set("data", dataTable(plugin))
        globals.set("emit", emit(host))
        globals.set("menu", menu(host, plugin))
        globals.set("channel", channel(host, plugin))
        // Only where the manifest asked for it: watching packets is watching everyone's chat.
        if (plugin.manifest.capabilities.packets) globals.set("packet", packets(host, plugin))
        for (surface in PluginSurface.entries) {
            globals.set(surface.surfaceName, claim(host, plugin, surface))
        }
        globals.set("on", listener(host, plugin, instance, observer = false))
        globals.set("watch", listener(host, plugin, instance, observer = true))
    }

    private fun describe(
        plugin: LoadedPlugin,
        instance: PluginInstance,
        lanes: Int,
    ): LuaTable {
        val table = Lua.table()
        table.set("id", plugin.manifest.id)
        table.set("name", plugin.manifest.name)
        table.set("version", plugin.manifest.version)
        // A plugin that keeps state in a module-level variable has one of those per lane, so it is
        // told which one it is rather than having to find out the hard way.
        table.set("lane", instance.lane)
        table.set("lanes", lanes)
        return table
    }

    private fun logTable(plugin: LoadedPlugin): LuaTable {
        val table = Lua.table()
        table.set(
            "info",
            HostFunction("log.info") { args ->
                plugin.log.info { args.tojstring(1) }
                Lua.NONE
            },
        )
        table.set(
            "warn",
            HostFunction("log.warn") { args ->
                plugin.log.warn { args.tojstring(1) }
                Lua.NONE
            },
        )
        table.set(
            "error",
            HostFunction("log.error") { args ->
                plugin.log.error { args.tojstring(1) }
                Lua.NONE
            },
        )
        return table
    }

    /**
     * `files.read`, `files.list` and `files.exists`, over the plugin's own folder.
     *
     * Paths are relative to `plugins/<id>/`, which is where the package's `resources/` were put, so
     * what a plugin asks for is what the operator sees. Anything the package kept to itself
     * (`[resources] private`) is still readable: the folder is looked at first and the package
     * after it.
     */
    private fun fileTable(plugin: LoadedPlugin): LuaTable {
        val table = Lua.table()
        table.set(
            "read",
            HostFunction("files.read") { args ->
                val path = args.checkjstring(1)
                val text =
                    FileIO.readText("${plugin.folder}/$path")
                        ?: plugin.pkg.readText(PackageLayout.RESOURCES + path)
                text?.let { Lua.of(it) } ?: Lua.NIL
            },
        )
        table.set(
            "exists",
            HostFunction("files.exists") { args ->
                val path = args.checkjstring(1)
                Lua.of(FileIO.exists("${plugin.folder}/$path") || plugin.pkg.exists(PackageLayout.RESOURCES + path))
            },
        )
        table.set(
            "list",
            HostFunction("files.list") { args ->
                val prefix = if (args.narg() >= 1) args.checkjstring(1) else ""
                val found =
                    plugin.files.keys
                        .filter { it.startsWith(prefix) }
                        .sorted()
                val out = Lua.table()
                for ((index, name) in found.withIndex()) out.set(index + 1, Lua.of(name))
                out
            },
        )
        table.set(
            "write",
            HostFunction("files.write") { args ->
                val path = args.checkjstring(1)
                val text = args.checkjstring(2)
                val live = "${plugin.folder}/$path"
                FileIO.createDirectories(live.substringBeforeLast('/'))
                FileIO.writeText(live, text)
                Lua.NIL
            },
        )
        return table
    }

    /** The TOML tree as Lua tables. Numbers stay integers where they were written as integers. */
    private fun tableOf(table: TomlValue.Table): LuaTable {
        val out = Lua.table()
        for (key in table.keys()) {
            val value = table[key] ?: continue
            val converted = valueOf(value)
            out.set(key, converted)
            // TOML is written kebab-case here and in the server's own files, and Lua cannot spell a
            // dash in `config.starting-balance`, so the same key answers to an underscore too.
            if ('-' in key) out.set(key.replace('-', '_'), converted)
        }
        return out
    }

    private fun valueOf(value: TomlValue): LuaValue =
        when (value) {
            is TomlValue.Str -> Lua.of(value.value)
            is TomlValue.Int64 -> Lua.of(value.value)
            is TomlValue.Real -> Lua.of(value.value)
            is TomlValue.Bool -> Lua.of(value.value)
            is TomlValue.Arr -> {
                val list = Lua.table()
                for ((index, element) in value.values.withIndex()) list.set(index + 1, valueOf(element))
                list
            }

            is TomlValue.Table -> tableOf(value)
        }

    /** `wait(ticks)`: ends the turn here and picks it up again later. */
    private class Wait(
        private val host: PluginHost,
        private val plugin: LoadedPlugin,
        private val instance: PluginInstance,
    ) : WaitingFunction("wait") {
        override suspend fun waiting(args: Varargs): Varargs {
            if (instance.deciderDepth > 0) {
                throw net.blueva.luak.LuaError(
                    "wait cannot be used inside on(): the server is holding the question open for " +
                        "the answer. Use watch(), which runs after the fact and may take its time",
                )
            }
            val ticks = args.optint(1, 1).coerceAtLeast(0)
            pauseUntil { resume ->
                val handle =
                    host.services.tasks.after(ticks) {
                        instance.sandbox.refill()
                        host.resuming(plugin) { resume() }
                    }
                plugin.registrations.add("wait($ticks)") { handle.cancel() }
            }
            return Lua.NONE
        }
    }

    private fun later(
        host: PluginHost,
        plugin: LoadedPlugin,
        instance: PluginInstance,
    ): LuaValue =
        HostFunction("later") { args ->
            val ticks = args.checkint(1)
            val fn = args.arg(2)
            if (!fn.isfunction()) throw luaError("later(ticks, fn) needs a function")
            val handle =
                host.services.tasks.after(ticks) {
                    host.enter(plugin, instance, fn, "later")
                }
            handleTable(plugin.registrations.add("later($ticks)") { handle.cancel() })
        }

    private fun every(
        host: PluginHost,
        plugin: LoadedPlugin,
        instance: PluginInstance,
    ): LuaValue =
        HostFunction("every") { args ->
            // every(period, fn) is the common shape; every(delay, period, fn) when the first one
            // should not be immediate.
            val threeArguments = args.narg() >= 3
            val delay = if (threeArguments) args.checkint(1) else args.checkint(1)
            val period = if (threeArguments) args.checkint(2) else args.checkint(1)
            val fn = if (threeArguments) args.arg(3) else args.arg(2)
            if (!fn.isfunction()) throw luaError("every(period, fn) needs a function")
            val handle =
                host.services.tasks.every(delay, period) {
                    host.enter(plugin, instance, fn, "every")
                }
            handleTable(plugin.registrations.add("every($period)") { handle.cancel() })
        }

    private fun command(
        host: PluginHost,
        plugin: LoadedPlugin,
        instance: PluginInstance,
    ): LuaValue =
        HostFunction("cmd") { args ->
            val spec = args.checkjstring(1)
            val fn = args.arg(2)
            if (!fn.isfunction()) throw luaError("cmd(spec, fn) needs a function")
            val options = args.arg(3)
            val permission =
                if (options.istable()) {
                    (options.get("permission") ?: Lua.NIL).let { if (it.isstring()) it.tojstring() else null }
                } else {
                    null
                }
            val parsed =
                try {
                    PluginCommandSpec.parse(spec, permission)
                } catch (e: ManifestException) {
                    throw luaError(e.message ?: "bad command")
                }
            host.registerCommand(plugin, instance, parsed, fn)
            Lua.NIL
        }

    /**
     * `on(event, [opts], fn)` and `watch(event, [opts], fn)`.
     *
     * The two lifecycle names stay on `on`, because they are about this plugin rather than about
     * something that happened in the world, and there is nothing to cancel or to watch.
     *
     * `opts` is a table and everything in it is optional: `priority` (first, early, normal, late,
     * last), `cancelled = true` to be told about events another handler already stopped, and any
     * other key is a filter on a field of the event. That last one is what saves every handler in
     * the world from opening with the same `if`.
     */
    private fun listener(
        host: PluginHost,
        plugin: LoadedPlugin,
        instance: PluginInstance,
        observer: Boolean,
    ): LuaValue =
        HostFunction(if (observer) "watch" else "on") { args ->
            val name = args.checkjstring(1)
            val hasOptions = args.narg() >= 3
            val options = if (hasOptions) args.arg(2) else Lua.NIL
            val fn = if (hasOptions) args.arg(3) else args.arg(2)
            if (!fn.isfunction()) throw luaError("${if (observer) "watch" else "on"}(event, fn) needs a function")

            if (name == PluginEventTypes.PLUGIN_ENABLE || name == PluginEventTypes.PLUGIN_DISABLE) {
                if (observer) throw luaError("'$name' is about this plugin: listen to it with on(), not watch()")
                if (name == PluginEventTypes.PLUGIN_ENABLE) instance.onEnable += fn else instance.onDisable += fn
                return@HostFunction Lua.NIL
            }

            // A name with a colon in it is a plugin's own, not one of the server's.
            if (':' in name) {
                if (observer) throw luaError("'$name' belongs to a plugin: listen to it with on()")
                host.announce(plugin, name, fn)
                return@HostFunction Lua.NIL
            }

            val type =
                host.events.typeOf(name)
                    ?: throw luaError(
                        "there is no event called '$name'; this server has " +
                            host.events.declared.joinToString(", ") { it.name },
                    )
            if (!observer && !type.cancellable && options.istable() && !(options.get("cancelled") ?: Lua.NIL).isnil()) {
                throw luaError("'$name' cannot be cancelled, so 'cancelled' means nothing on it")
            }

            var priority = PluginEventPriority.NORMAL
            var wantsCancelled = false
            val filters = mutableMapOf<String, Any?>()
            if (options.istable()) {
                for (field in type.fields + listOf("priority", "cancelled")) {
                    val value = options.get(field) ?: Lua.NIL
                    if (value.isnil()) continue
                    when (field) {
                        "priority" ->
                            priority =
                                PluginEventPriority.of(value.tojstring())
                                    ?: throw luaError(
                                        "'${value.tojstring()}' is not a priority; they are " +
                                            PluginEventPriority.entries.joinToString(", ") { it.name.lowercase() },
                                    )

                        "cancelled" -> wantsCancelled = value.toboolean()
                        else -> filters[field] = fromLua(value)
                    }
                }
            }
            host.registerEventHandler(plugin, instance, type, priority, observer, wantsCancelled, filters, fn)
            Lua.NIL
        }

    private fun fromLua(value: LuaValue): Any? =
        when {
            value.isnil() -> null
            value.isboolean() -> value.toboolean()
            value.isint() -> value.tolong()
            value.isnumber() -> value.todouble()
            else -> value.tojstring()
        }

    /** `players.online()`, `players.by_name(name)`, `players.by_uuid(uuid)`. */
    private fun playersTable(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaTable {
        val table = Lua.table()
        table.set(
            "online",
            HostFunction("players.online") {
                val out = Lua.table()
                for ((index, player) in host.services.players
                    .online()
                    .withIndex()) {
                    out.set(index + 1, playerObject(player, host.services.storage, plugin.manifest.id, plugin.text))
                }
                out
            },
        )
        table.set(
            "by_name",
            HostFunction("players.by_name") { args ->
                host.services.players
                    .byName(args.checkjstring(1))
                    ?.let { playerObject(it, host.services.storage, plugin.manifest.id, plugin.text) } ?: Lua.NIL
            },
        )
        table.set(
            "by_uuid",
            HostFunction("players.by_uuid") { args ->
                host.services.players
                    .byUuid(args.checkjstring(1))
                    ?.let { playerObject(it, host.services.storage, plugin.manifest.id, plugin.text) } ?: Lua.NIL
            },
        )
        table.set(
            "count",
            HostFunction("players.count") {
                Lua.of(
                    host.services.players
                        .online()
                        .size,
                )
            },
        )
        return table
    }

    /** `worlds.all()`, `worlds.by_name(name)`, `worlds.default()`. */
    private fun worldsTable(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaTable {
        val clicked = { fn: LuaValue, uuid: String -> host.npcClicked(plugin, fn, uuid) }
        // `worlds.create{ name = "arena", dimension = "overworld", type = "flat", seed = 1 }`.
        val make =
            HostFunction("worlds.create") { args ->
                val options = args.arg(1)
                if (!options.istable()) throw luaError("worlds.create{ name = ..., dimension = ... } takes a table")
                val name = options.field("name")
                if (!name.isstring()) throw luaError("a world needs a name")
                val seed = options.field("seed")
                host.services.worlds
                    .create(
                        name.tojstring(),
                        options.field("dimension").takeIf { it.isstring() }?.tojstring() ?: "overworld",
                        options.field("type").takeIf { it.isstring() }?.tojstring() ?: "normal",
                        if (seed.isnumber()) seed.tolong() else null,
                    )?.let { worldObject(it, clicked) } ?: Lua.NIL
            }
        val table = Lua.table()
        table.set("create", make)
        table.set(
            "all",
            HostFunction("worlds.all") {
                val out = Lua.table()
                for ((index, world) in host.services.worlds
                    .all()
                    .withIndex()) {
                    out.set(index + 1, worldObject(world, clicked))
                }
                out
            },
        )
        table.set(
            "by_name",
            HostFunction("worlds.by_name") { args ->
                host.services.worlds
                    .byName(args.checkjstring(1))
                    ?.let { worldObject(it, clicked) } ?: Lua.NIL
            },
        )
        table.set(
            "default",
            HostFunction("worlds.default") {
                host.services.worlds.default
                    ?.let { worldObject(it, clicked) } ?: Lua.NIL
            },
        )
        return table
    }

    /**
     * `at(world, x, z)`: from here on, this turn runs on the owner of that position.
     *
     * It is a pause, not a lock: the rest of the function is picked up by that owner when it gets
     * its turn, which is why it can only be used where pausing is allowed (not inside `on`).
     */
    private fun at(
        host: PluginHost,
        plugin: LoadedPlugin,
        instance: PluginInstance,
    ): LuaValue =
        object : WaitingFunction("at") {
            override suspend fun waiting(args: Varargs): Varargs {
                if (instance.deciderDepth > 0) {
                    throw net.blueva.luak.LuaError(
                        "at() cannot be used inside on(): the server is holding the question open. " +
                            "Use watch(), which runs after the fact",
                    )
                }
                // at(pos) and at(world, x, z) are the same question asked two ways.
                val point = pointFrom(args.arg(1))
                val world = point?.world ?: args.checkjstring(1)
                val x = point?.x ?: args.todouble(2)
                val z = point?.z ?: args.todouble(3)
                pauseUntil { resume ->
                    val arranged =
                        host.services.at(world, x, z) {
                            instance.sandbox.refill()
                            host.resuming(plugin) { resume() }
                        }
                    // Nobody owns it (an unloaded world): carry on where we are rather than
                    // dropping the rest of the turn on the floor.
                    if (!arranged) resume()
                }
                return Lua.NONE
            }
        }

    /**
     * `sidebar{ priority = 50, when = ..., title = ..., lines = ... }`, and the same for the rest.
     *
     * A plugin is one more profile beside the operator's, competing by the rule the config files
     * already use. The priority is asked for and never defaulted, because a hidden default is how
     * two plugins end up tied and the winner becomes whoever the file system happened to list first.
     */
    private fun claim(
        host: PluginHost,
        plugin: LoadedPlugin,
        surface: PluginSurface,
    ): LuaValue =
        HostFunction(surface.surfaceName) { args ->
            val options = args.arg(1)
            if (!options.istable()) {
                throw luaError("${surface.surfaceName}{ priority = ..., lines = ... } takes a table")
            }
            val priority = options.field("priority")
            if (!priority.isnumber()) {
                throw luaError(
                    "${surface.surfaceName}{} needs a priority: it is what decides who draws it when " +
                        "two plugins want to, and there is no default so a tie cannot happen by accident",
                )
            }
            val applies = options.field("when").takeIf { it.isfunction() }
            val title = options.field("title")
            val lines = options.field("lines")
            val claim =
                PluginSurfaceClaim(
                    pluginId = plugin.manifest.id,
                    surface = surface,
                    priority = priority.toint(),
                    appliesTo =
                        applies?.let { fn ->
                            { player: PluginPlayerRef ->
                                host.ask(plugin, fn, player, "${surface.surfaceName} when")?.toboolean() ?: false
                            }
                        },
                    render = { player ->
                        val drawnTitle = textOf(host, plugin, title, player, surface)
                        val drawnLines = linesOf(host, plugin, lines, player, surface)
                        if (drawnTitle == null && drawnLines == null) {
                            null
                        } else {
                            PluginSurfaceContent(drawnTitle.orEmpty(), drawnLines.orEmpty())
                        }
                    },
                )
            val handle = host.surfaces.claim(claim)
            plugin.registrations.add("${surface.surfaceName}(priority ${claim.priority})") { handle.cancel() }
            Lua.NIL
        }

    /** A string, or a function that gives one back per player. */
    private fun textOf(
        host: PluginHost,
        plugin: LoadedPlugin,
        value: LuaValue,
        player: PluginPlayerRef,
        surface: PluginSurface,
    ): String? =
        when {
            value.isfunction() ->
                host.ask(plugin, value, player, "${surface.surfaceName} title")?.takeIf { it.isstring() }?.tojstring()

            value.isstring() -> value.tojstring()
            else -> null
        }

    private fun linesOf(
        host: PluginHost,
        plugin: LoadedPlugin,
        value: LuaValue,
        player: PluginPlayerRef,
        surface: PluginSurface,
    ): List<String>? {
        val table =
            when {
                value.isfunction() -> host.ask(plugin, value, player, "${surface.surfaceName} lines")
                value.istable() -> value
                else -> null
            } ?: return null
        if (!table.istable()) return null
        val out = mutableListOf<String>()
        for (index in 1..table.length()) out += (table.get(index) ?: Lua.NIL).tojstring()
        return out
    }

    /**
     * `packet.on(name, fn)`, `packet.watch(name, fn)` and `packet.send(player, name, bytes)`.
     *
     * The names are the protocol's own with the direction in front, so `clientbound.set_title_text`
     * is what the jar calls `set_title_text` going out. A misspelling is refused at registration
     * with the list, rather than becoming a listener that never fires.
     */
    private fun packets(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaValue {
        val table = Lua.table()
        for (observer in listOf(false, true)) {
            table.set(
                if (observer) "watch" else "on",
                HostFunction(if (observer) "packet.watch" else "packet.on") { args ->
                    val name = args.checkjstring(1)
                    val hasOptions = args.narg() >= 3
                    val options = if (hasOptions) args.arg(2) else Lua.NIL
                    val fn = if (hasOptions) args.arg(3) else args.arg(2)
                    if (!fn.isfunction()) throw luaError("packet.on(name, fn) needs a function")
                    val sender = host.services.packets
                    if (!sender.isKnown(name)) {
                        throw luaError(
                            "this server has no packet called '$name'. " +
                                "Names look like clientbound.set_title_text; /plugins packets lists them",
                        )
                    }
                    val priority =
                        options
                            .takeIf { it.istable() }
                            ?.field("priority")
                            ?.takeIf { it.isstring() }
                            ?.let { spelled ->
                                PluginEventPriority.of(spelled.tojstring())
                                    ?: throw luaError("'${spelled.tojstring()}' is not a priority")
                            } ?: PluginEventPriority.NORMAL
                    val handle = host.packets.add(plugin, name, fn, priority, observer)
                    plugin.registrations.add("packet.${if (observer) "watch" else "on"}($name)") { handle.cancel() }
                    Lua.NIL
                },
            )
        }
        table.set(
            "send",
            HostFunction("packet.send") { args ->
                val player =
                    playerFrom(args.arg(1), host.services.players)
                        ?: throw luaError("packet.send(player, name, bytes) needs a player")
                val name = args.checkjstring(2)
                val body = args.arg(3)
                if (!body.istable()) throw luaError("packet.send needs the body as a list of bytes")
                val bytes =
                    ByteArray(body.length()) { index ->
                        ((body.get(index + 1) ?: Lua.NIL).toint() and 0xff).toByte()
                    }
                Lua.of(host.services.packets.send(player, name, bytes))
            },
        )
        table.set("names", HostFunction("packet.names") { listOf(host.services.packets.names()).toLuaList() })
        return table
    }

    private fun List<List<String>>.toLuaList(): LuaValue {
        val out = Lua.table()
        for ((index, name) in first().withIndex()) out.set(index + 1, Lua.of(name))
        return out
    }

    /**
     * `channel.on("arena:main", fn)` and `channel.send(player, "arena:main", bytes)`.
     *
     * Only the channels the manifest listed under `[capabilities] network`: a plugin listening on
     * `bungeecord:main` is talking to the proxy, and that is the operator's decision, not the
     * plugin's. Asking for one that was not declared is refused with the list that was.
     */
    private fun channel(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaValue {
        val allowed = plugin.manifest.capabilities.network

        fun check(name: String) {
            if (name in allowed) return
            throw luaError(
                if (allowed.isEmpty()) {
                    "${plugin.manifest.id} declared no channels; add network = [\"$name\"] " +
                        "to [capabilities] in plugin.toml"
                } else {
                    "'$name' is not one of ${plugin.manifest.id}'s channels, which are " +
                        allowed.joinToString(", ")
                },
            )
        }

        val table = Lua.table()
        table.set(
            "on",
            HostFunction("channel.on") { args ->
                val name = args.checkjstring(1)
                check(name)
                val fn = args.arg(2)
                if (!fn.isfunction()) throw luaError("channel.on(name, fn) needs a function")
                host.listenOn(plugin, name, fn)
                Lua.NIL
            },
        )
        table.set(
            "send",
            HostFunction("channel.send") { args ->
                val player =
                    playerFrom(args.arg(1), host.services.players)
                        ?: throw luaError("channel.send(player, name, bytes) needs a player")
                val name = args.checkjstring(2)
                check(name)
                val body = args.arg(3)
                if (!body.istable()) throw luaError("channel.send needs the body as a list of bytes")
                val bytes =
                    ByteArray(body.length()) { index ->
                        ((body.get(index + 1) ?: Lua.NIL).toint() and 0xff).toByte()
                    }
                Lua.of(host.services.channels.send(player, name, bytes))
            },
        )
        return table
    }

    /** `menu{ title = ..., rows = 3 }`: a window with buttons in it, and no slot arithmetic. */
    private fun menu(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaValue =
        HostFunction("menu") { args ->
            val options = args.arg(1)
            if (!options.istable()) throw luaError("menu{ title = ..., rows = 3 } takes a table")
            val rows = options.field("rows").takeIf { it.isnumber() }?.toint() ?: 3
            if (rows !in 1..PluginMenus.MAX_ROWS) {
                throw luaError("a menu has between 1 and ${PluginMenus.MAX_ROWS} rows, not $rows")
            }
            val title =
                options
                    .field("title")
                    .takeIf { it.isstring() }
                    ?.tojstring()
                    .orEmpty()
            val built =
                LuaMenu(
                    host.services.menus,
                    host.services.players,
                    plugin.text.resolve(title, Lua.NIL),
                    rows,
                ) { handler, player, slot ->
                    host.clicked(plugin, handler, player, slot)
                }
            plugin.registrations.add("menu($title)") { built.closeAll() }
            built.table
        }

    /** `emit("arena:round_start", { ... })`: tell whoever is listening, and carry on either way. */
    private fun emit(host: PluginHost): LuaValue =
        HostFunction("emit") { args ->
            val name = args.checkjstring(1)
            if (':' !in name) {
                throw luaError(
                    "'$name' has no colon in it, so it would collide with the server's own events; " +
                        "call it something like 'yourplugin:$name'",
                )
            }
            Lua.of(host.emit(name, args.arg(2)))
        }

    /** `pos(world, x, y, z)`: somewhere, written down once and passed around after that. */
    private fun position(): LuaValue =
        HostFunction("pos") { args ->
            positionObject(
                args.checkjstring(1),
                args.todouble(2),
                args.todouble(3),
                args.todouble(4),
            )
        }

    /** `placeholder("wins", fn)` registers `%<id>_wins%` in the server's own registry. */
    private fun placeholder(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaValue =
        HostFunction("placeholder") { args ->
            val name = args.checkjstring(1)
            val fn = args.arg(2)
            if (!fn.isfunction()) throw luaError("placeholder(name, fn) needs a function")
            val handle =
                host.services.placeholders.register(plugin.manifest.id, name) { player ->
                    host.answer(plugin, fn, player)
                }
            plugin.registrations.add("placeholder(%${plugin.manifest.id}_$name%)") { handle.cancel() }
            Lua.NIL
        }

    /** `fill(text, player)`: renders every placeholder the server knows, not only this plugin's. */
    private fun fill(host: PluginHost): LuaValue =
        HostFunction("fill") { args ->
            val text = args.checkjstring(1)
            val player = args.arg(2)
            Lua.of(host.services.placeholders.fill(text, host.playerOf(player)))
        }

    /** `broadcast(text)`: to everyone online, MiniMessage like everything else. */
    private fun broadcast(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaValue =
        HostFunction("broadcast") { args ->
            val message = plugin.text.resolve(args.checkjstring(1), args.arg(2))
            for (player in host.services.players.online()) player.send(message)
            Lua.NONE
        }

    /**
     * `service("economy", { ... })` to offer, `service("economy")` to use.
     *
     * What comes back from the second is not the table the first passed: it is a set of calls the
     * host mediates, so the provider's code runs in the provider's state and on its budget. Null
     * when nobody offers it, which is the whole of a soft dependency.
     */
    private fun service(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaValue =
        HostFunction("service") { args ->
            val name = args.checkjstring(1)
            val offered = args.arg(2)
            if (offered.isnil()) {
                host.serviceRegistry.consume(host, name) ?: Lua.NIL
            } else {
                // Declared on both sides: what a plugin offers is in its manifest, so the
                // tooling knows it and the operator can read it without running anything.
                if (!plugin.manifest.exports.services
                        .contains(name)
                ) {
                    throw luaError(
                        "$name is not in [exports] services of ${plugin.manifest.id}; " +
                            "add services = [\"$name\"] to plugin.toml",
                    )
                }
                val handle = host.serviceRegistry.provide(plugin.manifest.id, name, offered)
                plugin.registrations.add("service($name)") { handle.cancel() }
                Lua.NIL
            }
        }

    /** `t("welcome", { name = ... })`: the translation on its own, for text that goes somewhere else. */
    private fun translate(plugin: LoadedPlugin): LuaValue =
        HostFunction("t") { args ->
            Lua.of(plugin.text.resolve("@" + args.checkjstring(1), args.arg(2)))
        }

    /**
     * `store`: what the plugin keeps that is not about one player, and the queries over what is.
     *
     * `store.record = ...` is the plugin's own key-value, kept under the empty uuid so it lives in
     * the same table with the same isolation; `store.top("kills", 10)` is the query that made this
     * worth being a database rather than a file.
     */
    private fun storeTable(
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaTable {
        val table = Lua.table()
        val metatable = Lua.table()
        val id = plugin.manifest.id
        metatable.set(
            "__index",
            HostFunction("store.index") { args ->
                val key = args.arg(2).tojstring()
                if (key == "top") return@HostFunction topFunction(host, id)
                val stored = host.services.storage.playerValue(id, SERVER_SCOPE, key) ?: return@HostFunction Lua.NIL
                stored.toLongOrNull()?.let { return@HostFunction Lua.of(it) }
                stored.toDoubleOrNull()?.let { return@HostFunction Lua.of(it) }
                when (stored) {
                    "true" -> Lua.TRUE
                    "false" -> Lua.FALSE
                    else -> Lua.of(stored)
                }
            },
        )
        metatable.set(
            "__newindex",
            HostFunction("store.newindex") { args ->
                val key = args.arg(2).tojstring()
                val value = args.arg(3)
                host.services.storage.setPlayerValue(
                    id,
                    SERVER_SCOPE,
                    key,
                    if (value.isnil()) null else value.tojstring(),
                )
                Lua.NONE
            },
        )
        table.setmetatable(metatable)
        return table
    }

    /** `store.top("kills", 10)`: highest first, as `{ { name = ..., value = ... }, ... }`. */
    private fun topFunction(
        host: PluginHost,
        pluginId: String,
    ): LuaValue =
        HostFunction("store.top") { args ->
            val first = firstArgument(args)
            val key = args.checkjstring(first)
            val limit = args.optint(first + 1, 10)
            val out = Lua.table()
            for ((index, row) in host.services.storage
                .top(pluginId, key, limit)
                .withIndex()) {
                val entry = Lua.table()
                val player = host.services.players.byUuid(row.first)
                entry.set("uuid", row.first)
                entry.set("name", player?.name ?: row.first)
                entry.set("value", row.second.toLongOrNull()?.let { Lua.of(it) } ?: Lua.of(row.second))
                out.set(index + 1, entry)
            }
            out
        }

    /**
     * `data.json("arenas")`: a table kept in a file of the plugin's own folder.
     *
     * For what is not about a player. It is read when asked for and written by `data.save`, atomically,
     * so a crash leaves the old file rather than half of a new one.
     */
    private fun dataTable(plugin: LoadedPlugin): LuaTable {
        val table = Lua.table()
        val loaded = mutableMapOf<String, LuaTable>()
        table.set(
            "json",
            HostFunction("data.json") { args ->
                val name = args.checkjstring(1)
                loaded.getOrPut(name) {
                    val path = jsonPathOf(plugin, name)
                    val text = FileIO.readText(path)
                    if (text == null) Lua.table() else PluginJson.parse(text)
                }
            },
        )
        table.set(
            "save",
            HostFunction("data.save") { args ->
                val name = args.checkjstring(1)
                val value = loaded[name] ?: return@HostFunction Lua.of(false)
                val path = jsonPathOf(plugin, name)
                FileIO.createDirectories(path.substringBeforeLast('/'))
                // Serialised before the file is touched: a plugin whose table cannot be written
                // keeps the one it had rather than losing both.
                val text = PluginJson.write(value)
                FileIO.writeText(path, text)
                Lua.of(true)
            },
        )
        return table
    }

    /** `arenas` is `data/arenas.json` unless the plugin gave a path of its own. */
    private fun jsonPathOf(
        plugin: LoadedPlugin,
        name: String,
    ): String =
        if (name.contains('/') || name.endsWith(".json")) {
            "${plugin.folder}/$name"
        } else {
            "${plugin.folder}/data/$name.json"
        }

    /** The uuid a plugin's own data is kept under: not a player, and no player can have it. */
    private const val SERVER_SCOPE = "server"

    private fun handleTable(handle: PluginHandle): LuaTable {
        val table = Lua.table()
        table.set(
            "cancel",
            HostFunction("cancel") {
                handle.cancel()
                Lua.NONE
            },
        )
        return table
    }

    private fun luaError(message: String): net.blueva.luak.LuaError = net.blueva.luak.LuaError(message)
}
