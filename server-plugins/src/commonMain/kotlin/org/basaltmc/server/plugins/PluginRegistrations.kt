package org.basaltmc.server.plugins

/**
 * Everything one plugin has registered, and how to take it back out.
 *
 * This is the whole of why unloading works: nothing a plugin registers is allowed to exist outside
 * this list, so putting the server back the way it was is walking it backwards. The rule that keeps
 * it true is worth stating plainly: **what cannot be retired cannot be registered.**
 */
public class PluginRegistrations {
    private val entries = mutableListOf<Entry>()

    private class Entry(
        val what: String,
        val undo: () -> Unit,
    )

    public val size: Int get() = entries.size

    /** What is registered, for `/plugins` and for a test that wants to see nothing is left. */
    public val summary: List<String> get() = entries.map { it.what }

    public fun add(
        what: String,
        undo: () -> Unit,
    ): PluginHandle {
        val entry = Entry(what, undo)
        entries += entry
        return PluginHandle {
            if (entries.remove(entry)) entry.undo()
        }
    }

    /**
     * Retires everything, newest first, and keeps going when one of them throws.
     *
     * Newest first because a later registration may lean on an earlier one; and it keeps going
     * because a plugin left half-unloaded is worse than one whose cleanup complained.
     */
    public fun retireAll(onFailure: (String, Throwable) -> Unit) {
        for (entry in entries.asReversed()) {
            try {
                entry.undo()
            } catch (e: Throwable) {
                onFailure(entry.what, e)
            }
        }
        entries.clear()
    }
}
