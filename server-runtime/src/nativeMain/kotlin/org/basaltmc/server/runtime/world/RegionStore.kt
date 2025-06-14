package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.region.ChunkNbt
import org.basaltmc.server.world.region.LinearRegion
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIO
import kotlin.time.TimeSource

/**
 * Keeps a few decompressed regions in memory, so a region file is unpacked once rather than once
 * per chunk. Linear compresses a whole region as one zstd stream, so reading any chunk unpacks all
 * 1024: doing that per chunk from every worker was the dominant source of memory pressure while
 * exploring.
 *
 * Decompression happens outside the lock. Holding it across a region of zstd starved the
 * save/unload sweep, which needs the same lock, until the server quietly stopped saving at all.
 *
 * Compression is outside it too: [mutex] only ever guards the cache map and region staging, while
 * disk writes run under [writeMutex] alone. A sweep compressing a dozen regions used to hold the
 * map lock for the whole 700ms, and every generation worker needing [loadChunk] stood in line
 * behind it. Lock order is [writeMutex] then [mutex], never the reverse.
 */
public class RegionStore(
    worldName: String,
    private val dimension: String,
    worldsRoot: String = WORLDS_ROOT,
    private val verticalBounds: ChunkHeight = ChunkHeight.OVERWORLD,
    private val capacity: Int = CAPACITY,
    /** Injectable so a test can reach the ceiling without writing a hundred megabytes. */
    private val byteBudget: Long = BYTE_BUDGET,
) {
    /**
     * Where this world's files live: `worlds/<name>/`, and nothing outside it belongs to this world.
     *
     * Player state deliberately does not live here (it is in `players.db`, beside `worlds/`, not
     * inside any of them). That is the whole point of the layout: vanilla writes
     * `playerdata/<uuid>.dat` into the world directory, which is why a world cannot be deleted,
     * copied or reset there without taking everyone's inventory with it. Here, deleting this
     * directory deletes exactly one world.
     */
    private val worldRoot: String = "$worldsRoot/$worldName"
    private val mutex = NativeMutex()

    /** Serializes region staging and file writes; see the class KDoc for the lock order. */
    private val writeMutex = NativeMutex()

    /** Insertion-ordered, used as an LRU: re-inserting on access moves an entry to the back. */
    private val cached = LinkedHashMap<Long, LinearRegion>()
    private val dirtyRegions = HashSet<Long>()

    // Summed on demand, not tracked: a dozen entries is nothing to add up, and a counter would
    // drift the moment a save wrote through a region already cached.
    private fun cachedBytes(): Long = cached.values.sumOf { it.residentBytes }

    /** What the last save spent where, so a slow one can be attributed instead of guessed at. */
    public var encodeMillis: Long = 0
        private set

    public var writeMillis: Long = 0
        private set

    public val cachedRegionCount: Int get() = mutex.withLock { cached.size }

    public val cachedRegionBytes: Long get() = mutex.withLock { cachedBytes() }

    public fun regionPath(
        regionX: Int,
        regionZ: Int,
    ): String = "$worldRoot/dimensions/minecraft/$dimension/region/r.$regionX.$regionZ.linear"

    /** Returns `null` if this chunk has never been saved, meaning it still needs to be generated. */
    public fun loadChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? {
        val regionX = chunkX shr 5
        val regionZ = chunkZ shr 5
        val key = key(regionX, regionZ)

        // Reading and decompressing a region happens outside the lock, on purpose. It is tens of
        // megabytes of zstd, and holding the lock across it meant eight generation workers kept
        // this store permanently busy, so the save/unload sweep could never get in: the server
        // stopped saving and unloading entirely while exploring, and memory grew because of it.
        val alreadyLoaded = mutex.withLock { touch(key) }
        val region =
            alreadyLoaded ?: run {
                val bytes = FileIO.readBytes(regionPath(regionX, regionZ)) ?: return null
                val decoded = LinearRegion.read(bytes)
                // Another worker may have loaded the same region meanwhile; theirs wins, since it
                // may already hold writes this one would not have.
                mutex.withLock { touch(key) ?: decoded.also { store(key, it) } }
            }

        val chunkBytes = mutex.withLock { region.chunkAt(chunkX and 31, chunkZ and 31) } ?: return null
        // Decoding happens outside the lock: it is the expensive half, and it touches nothing shared.
        val tag = NbtIO.read(chunkBytes).tag as NbtCompound
        // Rebuilding a chunk goes through the same block setters generation uses, which mark it
        // dirty. Nothing here differs from what is already on disk, so saving it again is pure cost.
        return ChunkNbt.read(tag, verticalBounds).also { it.markClean() }
    }

    /**
     * Stages [chunks] into their regions and writes every region they touched. Returns how many
     * region files were rewritten, the unit of real work: a region is recompressed whole however
     * few of its chunks changed.
     */
    public fun saveChunks(
        chunks: List<Chunk>,
        timestamp: Int,
    ): Int {
        if (chunks.isEmpty()) return 0
        return writeMutex.withLock {
            // Grouped by region and encoded one region at a time, outside the map lock. Encoding
            // the whole sweep up front held every chunk's NBT blob at once, and a thousand-chunk
            // sweep left a ~50 MB high-water mark in the sweeper thread's allocator pools for good;
            // region by region the transient peak is one region's worth.
            val byRegion = chunks.groupBy { key(it.chunkX shr 5, it.chunkZ shr 5) }
            val start = TimeSource.Monotonic.markNow()
            var encodeTotal = 0L
            for ((key, regionChunks) in byRegion) {
                val encodeStart = TimeSource.Monotonic.markNow()
                val encoded = regionChunks.map { it to NbtIO.write(ChunkNbt.write(it)) }
                encodeTotal += encodeStart.elapsedNow().inWholeMilliseconds
                mutex.withLock {
                    val region = region(regionX(key), regionZ(key)) ?: LinearRegion().also { store(key, it) }
                    for ((chunk, bytes) in encoded) {
                        region.setChunk(chunk.chunkX and 31, chunk.chunkZ and 31, bytes, timestamp)
                    }
                    dirtyRegions.add(key)
                }
            }
            encodeMillis = encodeTotal
            val touched = flushDirty()
            writeMillis = start.elapsedNow().inWholeMilliseconds - encodeTotal
            for (chunk in chunks) chunk.markClean()
            touched
        }
    }

    /** Writes out any region held with unsaved changes. */
    public fun flush(): Int = writeMutex.withLock { flushDirty() }

    /**
     * Caller must hold [writeMutex] and must not hold [mutex]. The snapshot is what lets the zstd
     * and the file writes run without the map lock: a dirty region is never evicted (see [store]),
     * so the reference stays valid, and [writeMutex] keeps [LinearRegion.setChunk] out of the file
     * write.
     */
    private fun flushDirty(): Int {
        val toWrite =
            mutex.withLock {
                if (dirtyRegions.isEmpty()) return@withLock emptyList()
                val snapshot = dirtyRegions.mapNotNull { key -> cached[key]?.let { key to it } }
                dirtyRegions.clear()
                snapshot
            }
        var written = 0
        for ((key, region) in toWrite) {
            val path = regionPath(regionX(key), regionZ(key))
            FileIO.createDirectories(path.substringBeforeLast('/'))
            region.writeTo(path)
            written++
        }
        return written
    }

    /** Caller must hold [mutex]. Moves an already-cached region to the back of the LRU and returns it. */
    private fun touch(key: Long): LinearRegion? = cached.remove(key)?.also { cached[key] = it }

    /** Caller must hold [mutex]. Reads from disk if needed; only the save path, which is already under the lock. */
    private fun region(
        regionX: Int,
        regionZ: Int,
    ): LinearRegion? {
        val key = key(regionX, regionZ)
        touch(key)?.let { return it }
        val bytes = FileIO.readBytes(regionPath(regionX, regionZ)) ?: return null
        return LinearRegion.read(bytes).also { store(key, it) }
    }

    /**
     * Caller must hold [mutex]. Evicts least recently used *clean* regions. A dirty one stays
     * cached until [flushDirty] writes it: evicting it here would mean compressing a region file
     * while holding the map lock, which is exactly the stall this store exists to avoid, and it is
     * what [flushDirty]'s snapshot relies on. Between flushes the cache can therefore run past its
     * budget by the dirty regions; the sweep flushes every interval, so the excursion is bounded.
     */
    private fun store(
        key: Long,
        region: LinearRegion,
    ) {
        cached[key] = region
        if (cached.size <= capacity && cachedBytes() <= byteBudget) return
        val iterator = cached.keys.iterator()
        while (iterator.hasNext() && (cached.size > capacity || cachedBytes() > byteBudget)) {
            val oldest = iterator.next()
            if (oldest == key || oldest in dirtyRegions) continue
            iterator.remove()
        }
    }

    private companion object {
        // A player near a region corner works across four at once, and prefetch reaches past them,
        // so four meant constant thrashing: every miss is a full region decompressed again.
        const val CAPACITY = 12

        // The count on its own bounds nothing: a region holds 1024 decompressed chunk blobs, so
        // twelve full ones are hundreds of megabytes, and the process grew with distance flown.
        // Whichever limit bites first wins, which keeps many sparse regions cheap and few full ones
        // bounded.
        const val BYTE_BUDGET = 96L * 1024 * 1024

        fun key(
            regionX: Int,
            regionZ: Int,
        ): Long = (regionX.toLong() and 0xFFFFFFFFL) or (regionZ.toLong() shl 32)

        fun regionX(key: Long): Int = key.toInt()

        fun regionZ(key: Long): Int = (key shr 32).toInt()
    }
}

/** Every world lives under this directory, and nothing else does. */
public const val WORLDS_ROOT: String = "worlds"
