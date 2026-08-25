package org.basaltmc.server.runtime

/**
 * What `/timings` and the server-facts placeholders report about the native heap. Split out of
 * [BasaltServer] because none of it depends on anything else the server owns.
 */
public class ServerMetrics {
    /**
     * Live bytes after the last collection, or null if nothing has been collected yet.
     *
     * The chunk counts elsewhere only account for what the server means to hold; if memory climbs
     * while they stay flat, the answer is here, in the gap between what is live and what has been
     * allocated and not yet collected.
     */
    @OptIn(kotlin.native.runtime.NativeRuntimeApi::class, ExperimentalStdlibApi::class)
    internal fun liveHeapBytes(): Long? =
        kotlin.native.runtime.GC.lastGCInfo
            ?.memoryUsageAfter
            ?.get("heap")
            ?.totalObjectsSizeBytes

    /**
     * Forces a collection first, so the figure is what is reachable *now* rather than whatever the
     * last scheduled GC saw mid-generation, transient worker state included. Only for the explicit
     * `/timings` report: a forced collection pauses the world and has no place on a placeholder path.
     */
    @OptIn(kotlin.native.runtime.NativeRuntimeApi::class)
    internal fun collectedLiveHeapBytes(): Long? {
        // Twice, not once: measured in isolation, a single collection leaves tens of KB per
        // recently generated chunk unswept, and the second pass reclaims it. Only worth paying on
        // the explicit report path.
        kotlin.native.runtime.GC
            .collect()
        kotlin.native.runtime.GC
            .collect()
        return liveHeapBytes()
    }

    @OptIn(kotlin.native.runtime.NativeRuntimeApi::class, ExperimentalStdlibApi::class)
    internal fun heapReport(): String {
        val info = kotlin.native.runtime.GC.lastGCInfo ?: return "No collection has run yet."
        val before = info.memoryUsageBefore["heap"]?.totalObjectsSizeBytes ?: 0L
        val after = info.memoryUsageAfter["heap"]?.totalObjectsSizeBytes ?: 0L
        return "Heap: ${after / 1_048_576} MB live after the last collection " +
            "(${before / 1_048_576} MB before it, ${info.epoch} collections so far)."
    }
}
