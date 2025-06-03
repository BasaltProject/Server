package org.basaltmc.server.runtime

import org.basaltmc.wire.io.processCpuMicros
import kotlin.concurrent.AtomicReference
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/**
 * CPU usage as a percentage of one core, `top`'s convention: eight busy workers read as roughly
 * 800, which is the figure an operator can check against their own machine.
 *
 * The percentage only exists between two readings, so the last one is kept and the average over
 * the gap reported. Every connection thread reads this, hence the compare-and-set: two samples at
 * once must not both subtract from a half-updated baseline. [MINIMUM_WINDOW_MILLIS] bounds how
 * often it resamples, since a shorter window is mostly noise.
 */
@OptIn(ExperimentalTime::class)
public class CpuMeter {
    private data class Sample(
        val micros: Long,
        val atMillis: Long,
        val percent: Double?,
    )

    private val start = TimeSource.Monotonic.markNow()
    private val last = AtomicReference<Sample?>(null)

    /**
     * Takes the first reading, so the first caller gets a number instead of the null that
     * establishing a baseline would otherwise return.
     */
    public fun prime() {
        percent()
    }

    public fun percent(): Double? {
        val micros = processCpuMicros() ?: return null
        val now = start.elapsedNow().inWholeMilliseconds
        val previous = last.value

        if (previous == null) {
            last.compareAndSet(null, Sample(micros, now, null))
            return null
        }
        val elapsed = now - previous.atMillis
        // Normally one reading a second is plenty. The exception is the very first answer after
        // priming: holding out for a full second there would report nothing at all for that second,
        // which is exactly when somebody is looking at a freshly started server.
        val window = if (previous.percent == null) FIRST_WINDOW_MILLIS else MINIMUM_WINDOW_MILLIS
        if (elapsed < window) return previous.percent

        val used = (micros - previous.micros).toDouble() / 1000.0
        val percent = (used / elapsed * 100).coerceAtLeast(0.0)
        last.compareAndSet(previous, Sample(micros, now, percent))
        return percent
    }

    private companion object {
        const val MINIMUM_WINDOW_MILLIS = 1000L
        const val FIRST_WINDOW_MILLIS = 50L
    }
}
