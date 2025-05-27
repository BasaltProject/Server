package org.basaltmc.server.world

import org.basaltmc.io.FileIO
import org.basaltmc.io.NativeMutex
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO
import org.basaltmc.world.Chunk
import org.basaltmc.world.region.ChunkNbt
import org.basaltmc.world.region.LinearRegion

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
 * [CAPACITY] is deliberately small. Regions are large even decompressed, and generation walks
 * through space roughly in order, so a handful is enough to collapse the repeated work without the
 * cache itself becoming the memory problem.
 */
public class RegionStore(
    private val worldName: String,
    private val dimension: String,
) {
    private val mutex = NativeMutex()

    /** Insertion-ordered, used as an LRU: re-inserting on access moves an entry to the back. */
    private val cached = LinkedHashMap<Long, LinearRegion>()
    private val dirtyRegions = HashSet<Long>()

    public fun regionPath(
        regionX: Int,
        regionZ: Int,
    ): String = "$worldName/dimensions/minecraft/$dimension/region/r.$regionX.$regionZ.linear"

    /** Returns `null` if this chunk has never been saved, meaning it still needs to be generated. */
    public fun loadChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? {
        val chunkBytes =
            mutex.withLock {
                region(chunkX shr 5, chunkZ shr 5)?.chunkAt(chunkX and 31, chunkZ and 31)
            } ?: return null
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
        val encoded = chunks.map { it to NbtIO.write(ChunkNbt.write(it)) }
        val touched =
            mutex.withLock {
                for ((chunk, bytes) in encoded) {
                    val key = key(chunk.chunkX shr 5, chunk.chunkZ shr 5)
                    val region = region(chunk.chunkX shr 5, chunk.chunkZ shr 5) ?: LinearRegion().also { store(key, it) }
                    region.setChunk(chunk.chunkX and 31, chunk.chunkZ and 31, bytes, timestamp)
                    dirtyRegions.add(key)
                }
                flushLocked()
            }
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
            FileIO.writeBytes(path, region.write())
            written++
        }
        dirtyRegions.clear()
        return written
    }

    /** Caller must hold [mutex]. Returns `null` only when no file exists for this region yet. */
    private fun region(
        regionX: Int,
        regionZ: Int,
    ): LinearRegion? {
        val key = key(regionX, regionZ)
        cached.remove(key)?.let { existing ->
            cached[key] = existing
            return existing
        }
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
                cached[oldest]?.let { FileIO.writeBytes(path, it.write()) }
            }
            cached.remove(oldest)
        }
    }

    private companion object {
        const val CAPACITY = 4

        fun key(
            regionX: Int,
            regionZ: Int,
        ): Long = (regionX.toLong() and 0xFFFFFFFFL) or (regionZ.toLong() shl 32)

        fun regionX(key: Long): Int = key.toInt()

        fun regionZ(key: Long): Int = (key shr 32).toInt()
    }
}
