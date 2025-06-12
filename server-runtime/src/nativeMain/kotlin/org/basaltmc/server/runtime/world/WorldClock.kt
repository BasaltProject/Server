package org.basaltmc.server.runtime.world

import kotlin.math.floor
import kotlin.random.Random

/**
 * parity: `ServerClockManager.ClockInstance`.
 *
 * 26.2 replaced "day time plus game time" with clocks that carry a rate: a tick adds [rate] to a
 * fractional accumulator and only whole units move [totalTicks], which is what makes `/time rate
 * 0.5` run half-speed days rather than skipping every other tick.
 */
public class WorldClock(
    public var totalTicks: Long = 0L,
    public var rate: Float = 1f,
    public var paused: Boolean = false,
) {
    public var partialTick: Float = 0f
        private set

    /** Advances one tick. Returns whether [totalTicks] moved, so callers can skip a pointless sync. */
    public fun tick(advanceTime: Boolean): Boolean {
        if (paused || !advanceTime) return false
        partialTick += rate
        val whole = floor(partialTick).toInt()
        if (whole == 0) return false
        partialTick -= whole
        totalTicks += whole
        return true
    }

    public fun setTotalTicks(ticks: Long) {
        totalTicks = ticks
        partialTick = 0f
    }

    /** parity: `ServerClockManager.addTicks`, which clamps at zero rather than going negative. */
    public fun addTicks(ticks: Int) {
        totalTicks = maxOf(totalTicks + ticks, 0L)
    }

    /** What the client is told: zero rate when paused, so it stops interpolating instead of drifting. */
    public fun networkRate(advanceTime: Boolean): Float = if (paused || !advanceTime) 0f else rate

    public companion object {
        /** parity: the `day` timeline's period, which is what a marker's ticks are taken modulo. */
        public const val DAY_LENGTH: Int = 24000

        /** parity: the four `show_in_commands` markers of the `minecraft:day` timeline. */
        public val TIME_MARKERS: Map<String, Int> =
            mapOf(
                "day" to 1000,
                "noon" to 6000,
                "night" to 13000,
                "midnight" to 18000,
            )

        /** parity: `ClockTimeMarker.resolveTimeToMoveTo` for a marker with a period. */
        public fun timeOfNextMarker(
            from: Long,
            markerTicks: Int,
        ): Long {
            val delta = markerTicks - from.mod(DAY_LENGTH.toLong())
            return from + if (delta > 0) delta else DAY_LENGTH + delta
        }
    }
}

/**
 * Rain and thunder for one world.
 *
 * parity: `ServerLevel.advanceWeatherCycle`, counters included. The three countdowns are what make
 * the cycle work: [clearWeatherTime] forces clear weather while it runs (that is what `/weather
 * clear <duration>` sets), and outside it [rainTime] and [thunderTime] each count down to a *flip*
 * rather than to a state, which is why a fresh world with every counter at zero starts by drawing a
 * delay instead of raining on its first tick.
 */
public class WorldWeather(
    public var raining: Boolean = false,
    public var rainTime: Int = 0,
    public var thundering: Boolean = false,
    public var thunderTime: Int = 0,
    public var clearWeatherTime: Int = 0,
    private val random: Random = Random.Default,
) {
    public fun tick() {
        if (clearWeatherTime > 0) {
            clearWeatherTime--
            thunderTime = if (thundering) 0 else 1
            rainTime = if (raining) 0 else 1
            thundering = false
            raining = false
            return
        }
        when {
            thunderTime > 0 -> if (--thunderTime == 0) thundering = !thundering
            thundering -> thunderTime = sample(THUNDER_DURATION)
            else -> thunderTime = sample(THUNDER_DELAY)
        }
        when {
            rainTime > 0 -> if (--rainTime == 0) raining = !raining
            raining -> rainTime = sample(RAIN_DURATION)
            else -> rainTime = sample(RAIN_DELAY)
        }
    }

    /** parity: `MinecraftServer.setWeatherParameters`, which `/weather` calls with these four. */
    public fun set(
        clearTime: Int,
        weatherTime: Int,
        raining: Boolean,
        thundering: Boolean,
    ) {
        clearWeatherTime = clearTime
        rainTime = weatherTime
        thunderTime = weatherTime
        this.raining = raining
        this.thundering = thundering
    }

    /** A duration for a `/weather` that was given none, from the same ranges vanilla samples. */
    public fun randomDuration(range: IntRange): Int = sample(range)

    private fun sample(range: IntRange): Int = random.nextInt(range.first, range.last + 1)

    /** parity: `ServerLevel.getRainLevel`, as far as the client is concerned: on or off. */
    public val rainLevel: Float get() = if (raining) 1f else 0f
    public val thunderLevel: Float get() = if (thundering) 1f else 0f

    public companion object {
        /** parity: `ServerLevel.RAIN_DELAY`, `RAIN_DURATION`, `THUNDER_DELAY`, `THUNDER_DURATION`. */
        public val RAIN_DELAY: IntRange = 12000..180000
        public val RAIN_DURATION: IntRange = 12000..24000
        public val THUNDER_DELAY: IntRange = 12000..180000
        public val THUNDER_DURATION: IntRange = 3600..15600
    }
}
