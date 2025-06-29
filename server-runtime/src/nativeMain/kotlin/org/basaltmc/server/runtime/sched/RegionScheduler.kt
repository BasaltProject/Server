package org.basaltmc.server.runtime.sched

import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis

/**
 * A set of chunks that tick together, and the owner that ticks them.
 *
 * With the world in one piece there is one region per world and it owns every chunk of it. When
 * [RegionScheduler] starts splitting worlds there will be several, and this is
 * the type that gains a chunk set, a split and a merge. Nothing above it changes: the callers
 * already ask [RegionScheduler.regionFor] rather than assuming.
 */
public class Region internal constructor(
    /** Unique for the life of the process, so a log line can name which one did something. */
    public val id: Int,
    /** The world it belongs to. A region never spans two. */
    public val worldName: String,
    public val owner: Owner,
) {
    /**
     * The grid sections this region covers, empty while the world is in one piece.
     *
     * Empty means "everything": a world that is not split has one region and it owns whatever is
     * loaded, without a set to keep up to date.
     */
    internal var sections: Set<Long> = emptySet()

    /**
     * Whether this region is the one that runs what is not split yet.
     *
     * A world tick still has parts that are the whole world's rather than a region's: the weather,
     * the clock, the sleep vote. Running them once per region would run them several times, so they
     * run in the first region of the world and are named here rather than left to luck.
     */
    public var isPrimary: Boolean = true
        internal set

    /** Whether this region owns that chunk. True for everything while the world is one region. */
    public fun owns(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean = sections.isEmpty() || RegionMap.sectionKeyOfChunk(chunkX, chunkZ) in sections

    /** Whether this region owns the block, entity or player at these world coordinates. */
    public fun ownsPosition(
        x: Double,
        z: Double,
    ): Boolean = owns(floorToChunk(x), floorToChunk(z))

    private fun floorToChunk(value: Double): Int = (if (value < 0) value - 15.999 else value).toInt() shr 4
}

/**
 * Which regions exist and who ticks them.
 *
 * parity of model with `TickRegions`: how many regions there are is not a setting, it is a
 * consequence of where the players are; what the operator sets is whether the world is split at all
 * and how many threads tick regions at once. Today [regional] is off by default and every
 * world is one region, which is the vanilla order of tick.
 *
 * There is also one owner that belongs to nobody's region: the hour, the weather, the world border
 * and everything else that is global by nature. It is not "region zero" with a special case, it is
 * its own owner, and it exists in both modes.
 */
public class RegionScheduler(
    /** Whether a world may be split into several regions. Off is the vanilla order of tick. */
    public val regional: Boolean = false,
    /** How many regions tick at once when [regional] is on. One means they take turns. */
    public val threads: Int = 1,
) {
    private val mutex = NativeMutex()
    private val byWorld = HashMap<String, MutableList<Region>>()
    private var nextId = 0

    /** parity: Folia's own global region, for what does not belong to a piece of the map. */
    public val global: Owner = Owner("global")

    /** Every region there is, world by world, in the order they were made. */
    public val regions: List<Region> get() = mutex.withLock { byWorld.values.flatten() }

    /** The regions of one world. One today; the ones the world is split into when that lands. */
    public fun regionsOf(worldName: String): List<Region> = mutex.withLock { byWorld[worldName].orEmpty().toList() }

    /**
     * Who owns this position, making the world's first region if it has none yet.
     *
     * The coordinates decide once a world is split; while it is one region they are ignored, and
     * that is the point of asking with them anyway: the call sites are already right for the day
     * they stop being ignored. The answer must not be kept from one turn to the next, because a
     * region can merge into another between two of them.
     */
    public fun regionFor(
        worldName: String,
        chunkX: Int = 0,
        chunkZ: Int = 0,
    ): Region =
        mutex.withLock {
            val existing = byWorld[worldName]
            if (existing != null && existing.isNotEmpty()) {
                return@withLock existing.firstOrNull { it.owns(chunkX, chunkZ) } ?: existing.first()
            }
            newRegion(worldName)
        }

    /**
     * Works out what the regions of a world should be from the chunks it has loaded, and makes it
     * so: groups that grew together are merged, a group that fell apart is split.
     *
     * Only ever called at a tick boundary, before any region has started its turn: an entity or a
     * block changing hands mid-tick is the one thing this model cannot survive. With [regional] off
     * it does nothing at all, which is what keeps the default mode on exactly one region.
     *
     * A merge moves the work queued on the regions that go away onto the survivor. A split cannot:
     * a task is a closure and does not say which half it belonged to, so the survivor keeps it,
     * which is what Folia's own comment about "the region that keeps the data" amounts to.
     */
    public fun repartition(
        worldName: String,
        loadedChunks: Collection<Long>,
        onChange: (merged: Int, split: Int) -> Unit = { _, _ -> },
    ) {
        if (!regional) return
        val sections = HashSet<Long>(loadedChunks.size)
        for (chunk in loadedChunks) {
            sections.add(RegionMap.sectionKeyOfChunk((chunk shr 32).toInt(), chunk.toInt()))
        }
        val groups = RegionMap.components(sections)
        if (groups.isEmpty()) return
        mutex.withLock {
            val current = byWorld.getOrPut(worldName) { mutableListOf() }
            if (current.isEmpty()) current.add(newRegionLocked(worldName))
            var merged = 0
            var split = 0
            val kept = mutableListOf<Region>()
            val spare = current.toMutableList()
            for (group in groups) {
                // Whoever already owns most of this group keeps it, so the fewest tasks move.
                val owners = spare.filter { region -> region.sections.any { it in group } }
                val survivor =
                    when {
                        owners.isNotEmpty() -> owners.maxBy { region -> region.sections.count { it in group } }
                        // A group nobody owned yet: the world's first region takes it if it is free,
                        // and otherwise it is a new one.
                        spare.any { it.sections.isEmpty() } -> spare.first { it.sections.isEmpty() }
                        else -> newRegionLocked(worldName).also { split++ }
                    }
                for (other in owners) {
                    if (other === survivor) continue
                    // Everything it had queued belongs to the survivor now, not to nobody.
                    other.owner.drainInto(survivor.owner)
                    other.owner.close()
                    spare.remove(other)
                    merged++
                }
                survivor.sections = group
                spare.remove(survivor)
                kept.add(survivor)
            }
            // What is left owned sections nobody has loaded any more: its work goes to the first
            // region of the world rather than being dropped on the floor.
            for (gone in spare) {
                gone.owner.drainInto(kept.first().owner)
                gone.owner.close()
            }
            current.clear()
            current.addAll(kept)
            for ((index, region) in current.withIndex()) region.isPrimary = index == 0
            if (merged > 0 || split > 0) onChange(merged, split)
        }
    }

    private fun newRegion(worldName: String): Region = newRegionLocked(worldName)

    private fun newRegionLocked(worldName: String): Region {
        val region = Region(nextId++, worldName, Owner(worldName))
        byWorld.getOrPut(worldName) { mutableListOf() }.add(region)
        return region
    }

    /**
     * One turn of everything: the global owner first, then every region.
     *
     * [tickRegion] is what a region does with its own turn, which is the world tick. The order here
     * is the order a single thread runs them in, and it is what makes the default mode the same
     * game as before: with [regional] off there is one region per world and they run in the order
     * the worlds were loaded, exactly as the loop did when it walked the worlds itself.
     */
    public fun tick(
        onFailure: (Throwable) -> Unit,
        tickRegion: (Region) -> Unit,
    ) {
        if (!global.isCurrent) global.bindToCurrentThread()
        global.advanceTick()
        global.runPending(onFailure)
        val turn = regions
        if (workers == null || turn.size < 2) {
            for (region in turn) {
                if (!region.owner.isCurrent) region.owner.bindToCurrentThread()
                tickRegion(region)
            }
        } else {
            workers.runTurn(turn, onFailure, tickRegion)
        }
        global.runEndOfTick(onFailure)
    }

    /** Nothing else runs anywhere: the server is going down. */
    public fun close() {
        workers?.stop()
        global.close()
        for (region in regions) region.owner.close()
    }

    /**
     * The threads that tick regions at once, or null when they take turns on the caller's thread.
     *
     * One thread is not the same as none: with one worker the regions still run somewhere else,
     * which is measurable and pointless, so a single thread means the caller does it.
     */
    private val workers: RegionWorkers? =
        if (regional && threads > 1) RegionWorkers(threads) else null
}

/**
 * A fixed set of threads that tick regions at the same time, with a barrier at the end.
 *
 * Every region has to finish before the next tick starts: a region that ran ahead would see a world
 * a tick older than its neighbour's, and what crosses a border between them would be read twice or
 * not at all. parity of intent with `TickRegionScheduler`, minus its earliest-deadline ordering,
 * which is worth having only once there are more regions than threads to measure it with.
 */
private class RegionWorkers(
    threads: Int,
) {
    private val mutex = NativeMutex()
    private val running = kotlin.concurrent.AtomicInt(1)
    private val finished = kotlin.concurrent.AtomicInt(0)
    private var turn: List<Region> = emptyList()
    private var claimed = 0
    private var work: ((Region) -> Unit)? = null
    private var onFailure: ((Throwable) -> Unit)? = null

    init {
        repeat(threads) {
            NativeThread.start {
                while (running.value == 1) {
                    val claim = claimNext()
                    if (claim == null) {
                        sleepMillis(IDLE_MILLIS)
                        continue
                    }
                    val (region, job, failure) = claim
                    if (!region.owner.isCurrent) region.owner.bindToCurrentThread()
                    try {
                        job(region)
                    } catch (e: Throwable) {
                        failure(e)
                    }
                    finished.value = finished.value + 1
                }
            }
        }
    }

    /**
     * The next region of this turn, or null when there is none left.
     *
     * Everything about a turn is read under the same lock that writes it: a worker that read the
     * list without it kept a turn that was already over and ticked a region a second time, which
     * looked like a corrupted world rather than like a scheduling bug.
     */
    private fun claimNext(): Triple<Region, (Region) -> Unit, (Throwable) -> Unit>? =
        mutex.withLock {
            val job = work ?: return@withLock null
            val failure = onFailure ?: return@withLock null
            if (claimed >= turn.size) return@withLock null
            val region = turn[claimed++]
            Triple(region, job, failure)
        }

    fun runTurn(
        regions: List<Region>,
        onFailure: (Throwable) -> Unit,
        work: (Region) -> Unit,
    ) {
        mutex.withLock {
            turn = regions
            claimed = 0
            this.work = work
            this.onFailure = onFailure
            finished.value = 0
        }
        // The barrier: nobody starts the next tick until every region has finished this one. A
        // region that ran ahead would see a world a tick older than its neighbour's, and what
        // crosses the border between them would be read twice or not at all.
        var waited = 0
        while (finished.value < regions.size && waited < TURN_LIMIT_MILLIS) {
            sleepMillis(1)
            waited++
        }
        mutex.withLock {
            turn = emptyList()
            claimed = 0
            this.work = null
            this.onFailure = null
        }
    }

    fun stop() {
        running.value = 0
    }

    private companion object {
        const val IDLE_MILLIS = 1L
        const val TURN_LIMIT_MILLIS = 5000
    }
}
