package org.basaltmc.server.world

import org.basaltmc.io.FileIO
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO
import org.basaltmc.world.Chunk
import org.basaltmc.world.region.ChunkNbt
import org.basaltmc.world.region.LinearRegion

/**
 * Saves and loads chunks as real `.linear` region files under `<worldName>/dimensions/minecraft/
 * <dimension>/region/`, the same layout a real 26.2 server uses (confirmed live earlier this
 * project's own history against a real vanilla server, not assumed). Only the overworld is wired
 * anywhere yet, but [dimension] is a parameter, not hardcoded, so nether/end need no rework later.
 */
public object WorldPersistence {
    public fun regionPath(
        worldName: String,
        dimension: String,
        regionX: Int,
        regionZ: Int,
    ): String = "$worldName/dimensions/minecraft/$dimension/region/r.$regionX.$regionZ.linear"

    /** Returns `null` if no region file covers this chunk yet, meaning it still needs to be generated. */
    public fun loadChunk(
        worldName: String,
        dimension: String,
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? {
        val path = regionPath(worldName, dimension, chunkX shr 5, chunkZ shr 5)
        val bytes = FileIO.readBytes(path) ?: return null
        val region = LinearRegion.read(bytes)
        val chunkBytes = region.chunkAt(chunkX and 31, chunkZ and 31) ?: return null
        val tag = NbtIO.read(chunkBytes).tag as NbtCompound
        return ChunkNbt.read(tag)
    }

    /** Merges [chunks] into whatever region files already exist on disk, grouped by which region each chunk falls into. */
    public fun saveChunks(
        worldName: String,
        dimension: String,
        chunks: List<Chunk>,
        timestamp: Int,
    ) {
        for ((regionCoord, regionChunks) in chunks.groupBy { (it.chunkX shr 5) to (it.chunkZ shr 5) }) {
            val (regionX, regionZ) = regionCoord
            val path = regionPath(worldName, dimension, regionX, regionZ)
            val existing = FileIO.readBytes(path)
            val region = if (existing != null) LinearRegion.read(existing) else LinearRegion()
            for (chunk in regionChunks) {
                val bytes = NbtIO.write(ChunkNbt.write(chunk))
                region.setChunk(chunk.chunkX and 31, chunk.chunkZ and 31, bytes, timestamp)
            }
            FileIO.createDirectories(path.substringBeforeLast('/'))
            FileIO.writeBytes(path, region.write())
        }
    }
}
