package org.basaltmc.server.plugins

/**
 * When a handler runs relative to the others.
 *
 * Five, not six: Bukkit's `MONITOR` is not a priority, it is "I want to look without touching", and
 * that is [PluginEventBus.watch] instead.
 */
public enum class PluginEventPriority {
    FIRST,
    EARLY,
    NORMAL,
    LATE,
    LAST,
    ;

    public companion object {
        public fun of(name: String): PluginEventPriority? =
            entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
    }
}

/**
 * One kind of event: what it is called, what it carries, and whether it can be stopped.
 *
 * The fields are declared rather than discovered so that the documentation, the editor's completion
 * and the audit against Bukkit all come from one place, and so that a plugin reading a field that
 * does not exist finds out at once instead of getting a silent nil.
 */
public class PluginEventType(
    public val name: String,
    public val cancellable: Boolean,
    /** In the order the server fills them, which is the order they are read back. */
    public val fields: List<String>,
    /** The ones a handler may write, which the server reads back after dispatch. */
    public val mutable: Set<String> = emptySet(),
) {
    public fun indexOf(field: String): Int = fields.indexOf(field)

    override fun toString(): String = name
}

/**
 * One event being fired.
 *
 * There is one of these per type and lane, reused: firing an event does not make one, so an event
 * nobody listens to costs a comparison and an event with listeners costs what its values cost.
 * The other half of that bargain is that it stops being valid the moment the handler returns, which
 * is checked rather than left to chance: keeping one and reading it a tick later is a mistake with
 * a message, not a value that quietly means nothing.
 */
public class PluginEvent(
    public val type: PluginEventType,
) {
    private val values = arrayOfNulls<Any?>(type.fields.size)

    /** Bumped on every fire, so a handler that kept the event can be told it is stale. */
    internal var generation: Long = 0
        private set

    public var cancelled: Boolean = false
        private set

    /** What to tell whoever caused it, when the handler bothered to say. */
    public var cancelMessage: String? = null
        private set

    internal var live: Boolean = false

    public fun set(
        index: Int,
        value: Any?,
    ) {
        values[index] = value
    }

    public fun set(
        field: String,
        value: Any?,
    ) {
        val index = type.indexOf(field)
        if (index >= 0) values[index] = value
    }

    public fun get(index: Int): Any? = values[index]

    public fun get(field: String): Any? {
        val index = type.indexOf(field)
        return if (index >= 0) values[index] else null
    }

    public fun cancel(message: String? = null) {
        cancelled = true
        if (message != null) cancelMessage = message
    }

    /** Readies it for another fire. Values are overwritten by the caller straight after. */
    internal fun begin() {
        cancelled = false
        cancelMessage = null
        generation++
        live = true
    }

    internal fun end() {
        live = false
    }

    /** What the fast path does: no handlers, nothing to answer, no stale answer left behind. */
    internal fun clear() {
        cancelled = false
        cancelMessage = null
    }
}

/** One registered handler. */
internal class PluginEventHandler(
    val pluginId: String,
    val lane: Int,
    val priority: PluginEventPriority,
    val observer: Boolean,
    /** Whether it wants events another handler already cancelled. Off by default, unlike Bukkit. */
    val wantsCancelled: Boolean,
    /** Field equality that has to hold, so a handler is not woken to run one `if`. */
    val filters: Map<String, Any?>,
    val call: (PluginEvent) -> Unit,
) {
    fun accepts(event: PluginEvent): Boolean {
        if (event.cancelled && !wantsCancelled) return false
        for ((field, expected) in filters) {
            if (event.get(field) != expected) return false
        }
        return true
    }
}

/**
 * Who is listening to what.
 *
 * Two kinds of handler, and the difference is not a convention: a **decider** (`on`) runs where the
 * server asked the question and answers it, so it may cancel and may not pause; an **observer**
 * (`watch`) runs after the fact, so it may take as long as it likes and may not change anything.
 * That is the whole of the split, and it is what lets a protection plugin answer inside the tick
 * while a statistics one goes to disk without holding anyone up.
 */
public class PluginEventBus {
    private val handlers = HashMap<String, MutableList<PluginEventHandler>>()
    private val types = HashMap<String, PluginEventType>()

    /** What may be listened to. A name outside this is an error at registration, not a silent no-op. */
    public fun declare(type: PluginEventType) {
        types[type.name] = type
    }

    public fun typeOf(name: String): PluginEventType? = types[name]

    public val declared: List<PluginEventType> get() = types.values.sortedBy { it.name }

    /** The one question the hot path asks. */
    public fun listening(type: PluginEventType): Boolean = handlers[type.name]?.isNotEmpty() == true

    private val events = HashMap<String, PluginEvent>()

    /**
     * The reusable event of this type, to fill in and fire.
     *
     * One per type rather than one per fire: that is what makes firing free of allocation. It is
     * handed out rather than made by the caller so that nothing else has to know it is shared.
     */
    public fun event(type: PluginEventType): PluginEvent = events.getOrPut(type.name) { PluginEvent(type) }

    internal fun add(
        handler: PluginEventHandler,
        type: PluginEventType,
    ): PluginHandle {
        val list = handlers.getOrPut(type.name) { mutableListOf() }
        list += handler
        // Deciders first, in priority order, then the observers: the answer is settled before
        // anyone is told about it.
        list.sortWith(compareBy({ if (it.observer) 1 else 0 }, { it.priority.ordinal }))
        return PluginHandle {
            list.remove(handler)
            if (list.isEmpty()) handlers.remove(type.name)
        }
    }

    /** Drops everything one plugin registered, for an unload that has to leave nothing behind. */
    public fun removeAll(pluginId: String) {
        for ((name, list) in handlers.entries.toList()) {
            list.removeAll { it.pluginId == pluginId }
            if (list.isEmpty()) handlers.remove(name)
        }
    }

    /**
     * Runs the handlers of [event] and answers whether it may go ahead.
     *
     * A handler that throws does not stop the others: its plugin is told, and the rest of the
     * server's turn carries on, which is the difference between a plugin bug and an outage.
     */
    public fun fire(event: PluginEvent): Boolean {
        val list = handlers[event.type.name]
        if (list == null || list.isEmpty()) {
            // Cleared even with nobody listening: the event is reused, so leaving last time's
            // answer in it would cancel something at random the next time one is fired.
            event.clear()
            return true
        }
        event.begin()
        try {
            // Indexed rather than iterated: a handler that unregisters another during dispatch
            // would invalidate an iterator, and that is a plugin's right.
            var index = 0
            while (index < list.size) {
                val handler = list.getOrNull(index) ?: break
                index++
                if (!handler.accepts(event)) continue
                handler.call(event)
            }
        } finally {
            event.end()
        }
        return !event.cancelled
    }
}
