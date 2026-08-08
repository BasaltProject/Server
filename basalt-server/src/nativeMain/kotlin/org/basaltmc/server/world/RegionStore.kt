package org.basaltmc.server.world

import org.basaltmc.io.FileIO
import org.basaltmc.io.NativeMutex
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO
import org.basaltmc.world.Chunk
import org.basaltmc.world.region.ChunkNbt
import org.basaltmc.world.region.LinearRegion
import kotlin.time.TimeSource

/**
 * Keeps a few decompressed regions in memory so a region file is read and unpacked once rather than
 * once per chunk.
 *
 * Linear compresses a whole region as a single zstd stream, which is what makes it compact but also
 * means there is no way to pull one chunk out of a file cheaply: reading a chunk means unpacking
 * all 1024 of them. Doing that per chunk, from every generation worker at once, allocated tens of
 * megabytes per chunk loaded and was by far the dominant source of memory pressure while exploring.
 * Holding the region instead turns a chunk load into a lookup.
 *
 * Reading and decompressing happens outside the lock. Holding it across a region's worth of zstd
 * kept the store busy continuously while eight workers explored, and the save/unload sweep, which
 * needs the same lock, never got a turn: the server quietly stopped saving and unloading at all.
 */
public class RegionStore(
    private val worldName: String,
    private val dimension: String,
) {
    private val mutex = NativeMutex()

    /** Insertion-ordered, used as an LRU: re-inserting on access moves an entry to the back. */
    private val cached = LinkedHashMap<Long, LinearRegion>()
    private val dirtyRegions = HashSet<Long>()

    /** What the last save spent where, so a slow one can be attributed instead of guessed at. */
    public var encodeMillis: Long = 0
        private set

    public var writeMillis: Long = 0
        private set

    public val cachedRegionCount: Int get() = mutex.withLock { cached.size }

    public fun regionPath(
        regionX: Int,
        regionZ: Int,
    ): String = "$worldName/dimensions/minecraft/$dimension/region/r.$regionX.$regionZ.linear"

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
        return ChunkNbt.read(tag).also { it.markClean() }
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
        // Serializing outside the lock keeps the per-chunk NBT work off everyone else's path.
        val encodeStart = TimeSource.Monotonic.markNow()
        val encoded = chunks.map { it to NbtIO.write(ChunkNbt.write(it)) }
        encodeMillis = encodeStart.elapsedNow().inWholeMilliseconds

        // Grouped by region first, so each region is fetched once however many of its chunks
        // changed. Fetching per chunk could evict and reload the same region repeatedly, and every
        // eviction writes, so one save could rewrite the same file many times over.
        val byRegion = encoded.groupBy { (chunk, _) -> key(chunk.chunkX shr 5, chunk.chunkZ shr 5) }
        val writeStart = TimeSource.Monotonic.markNow()
        val touched =
            mutex.withLock {
                for ((key, entries) in byRegion) {
                    val region = region(regionX(key), regionZ(key)) ?: LinearRegion().also { store(key, it) }
                    for ((chunk, bytes) in entries) {
                        region.setChunk(chunk.chunkX and 31, chunk.chunkZ and 31, bytes, timestamp)
                    }
                    dirtyRegions.add(key)
                }
                flushLocked()
            }
        writeMillis = writeStart.elapsedNow().inWholeMilliseconds
        for ((chunk, _) in encoded) chunk.markClean()
        return touched
    }

    /** Writes out any region held with unsaved changes. */
    public fun flush(): Int = mutex.withLock { flushLocked() }

    private fun flushLocked(): Int {
        if (dirtyRegions.isEmpty()) return 0
        var written = 0
        for (key in dirtyRegions) {
            val region = cached[key] ?: continue
            val path = regionPath(regionX(key), regionZ(key))
            FileIO.createDirectories(path.substringBeforeLast('/'))
            region.writeTo(path)
            written++
        }
        dirtyRegions.clear()
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

    /** Caller must hold [mutex]. Evicts the least recently used region, writing it first if it changed. */
    private fun store(
        key: Long,
        region: LinearRegion,
    ) {
        cached[key] = region
        while (cached.size > CAPACITY) {
            val oldest = cached.keys.first()
            if (oldest == key) break
            if (dirtyRegions.remove(oldest)) {
                val path = regionPath(regionX(oldest), regionZ(oldest))
                FileIO.createDirectories(path.substringBeforeLast('/'))
                cached[oldest]?.writeTo(path)
            }
            cached.remove(oldest)
        }
    }

    private companion object {
        // A player near a region corner works across four at once, and prefetch reaches past them,
        // so four meant constant thrashing: every miss is a full region decompressed again.
        const val CAPACITY = 12

        fun key(
            regionX: Int,
            regionZ: Int,
        ): Long = (regionX.toLong() and 0xFFFFFFFFL) or (regionZ.toLong() shl 32)

        fun regionX(key: Long): Int = key.toInt()

        fun regionZ(key: Long): Int = (key shr 32).toInt()
    }
}
