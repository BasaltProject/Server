package org.basaltmc.server.plugins

/**
 * Whoever is inside a plugin right now, and since when.
 *
 * Written by the thread that runs plugins and read by the watchdog on another, which is why it
 * is two atomics and not a data class: nobody has to see them agree.
 */
internal class PluginWatchdog(
    private val clock: kotlin.time.TimeMark,
) {
    private val runningPlugin = kotlin.concurrent.AtomicReference<String?>(null)
    private val runningSince = kotlin.concurrent.AtomicLong(0)

    internal fun nowMillis(): Long = clock.elapsedNow().inWholeMilliseconds

    /** Marks [pluginId] as the one running, now. */
    internal fun mark(pluginId: String) {
        runningPlugin.value = pluginId
        runningSince.value = nowMillis()
    }

    /** Clears whoever was marked as running. */
    internal fun clear() {
        runningPlugin.value = null
    }

    /** Marks a plugin as the one running, for as long as [body] takes. */
    internal fun <T> watched(
        plugin: LoadedPlugin,
        body: () -> T,
    ): T {
        runningPlugin.value = plugin.manifest.id
        runningSince.value = nowMillis()
        val startedAt = clock.elapsedNow().inWholeNanoseconds
        return try {
            body()
        } finally {
            runningPlugin.value = null
        }
    }

    /**
     * Cuts off a plugin that has been inside the server for longer than [limitMillis].
     *
     * The instruction ceiling catches one spinning in Lua; this catches one stuck in something the
     * host is doing for it, which the counter never sees. Answers who was cut off, so the caller
     * can say it out loud: a plugin cannot lag the server without the server naming it.
     */
    internal fun interruptIfStuck(
        limitMillis: Long,
        plugins: Map<String, LoadedPlugin>,
    ): String? {
        val who = runningPlugin.value ?: return null
        if (nowMillis() - runningSince.value < limitMillis) return null
        runningPlugin.value = null
        plugins[who]?.log?.error { "was still inside the server after ${limitMillis}ms and was cut off" }
        interruptAll(plugins.values)
        return who
    }

    /**
     * Stops whatever plugin code is running right now.
     *
     * Meant for a watchdog on another thread: the instruction ceiling catches a plugin spinning in
     * Lua, and this catches one stuck in something the host is doing for it.
     */
    internal fun interruptAll(plugins: Collection<LoadedPlugin>) {
        for (plugin in plugins) for (instance in plugin.instances) instance.sandbox.interrupt()
    }
}
