package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.Lua

/**
 * Registers a plugin's event handlers and fires them, and answers what an event's `player`,
 * `entity` and `pos` are behind the fields it carries by name and id.
 */
internal class PluginEventDispatch(
    private val host: PluginHost,
) {
    private var warnedAboutThread = false

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
        val handle = host.events.add(entry, type)
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
            host.services.tasks.after(0) {
                val laneView = PluginEventView(snapshot, { }, { key -> objectFor(plugin, snapshot, key) })
                snapshot.begin()
                laneView.bind()
                // Never ended: the copy is nobody else's, so it stays readable across a `wait`
                // rather than going stale under a handler that was allowed to pause.
                host.enter(plugin, instance, handler, "watch ${event.type.name}", Lua.varargs(laneView.table))
            }
            return
        }
        if (!host.onPluginThread()) {
            if (!warnedAboutThread) {
                warnedAboutThread = true
                host.log.warn {
                    "a ${event.type.name} was fired from a thread plugins do not run on, so no " +
                        "decider saw it. Either the server is running regions on more than one " +
                        "thread (the known limit until lanes follow them), or that event is being " +
                        "fired from the wrong place and it is a bug here"
                }
            }
            return
        }
        instance.deciderDepth++
        val startedAt = host.clock.elapsedNow().inWholeNanoseconds
        try {
            host.enter(plugin, instance, handler, "on ${event.type.name}", Lua.varargs(view))
        } finally {
            instance.deciderDepth--
            host.watchdog.clear()
            host.timings
                ?.of("plugin.${plugin.manifest.id}")
                ?.record(host.clock.elapsedNow().inWholeNanoseconds - startedAt)
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
                )?.let { host.services.players.byName(it) }
                    ?.let { playerObject(it, host.services.storage, plugin.manifest.id, plugin.text) }
            "entity" ->
                (event.get("entity_id") as? Int)
                    ?.let { id ->
                        val world = event.get("world") as? String
                        val worlds =
                            if (world !=
                                null
                            ) {
                                listOfNotNull(host.services.worlds.byName(world))
                            } else {
                                host.services.worlds.all()
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
}
