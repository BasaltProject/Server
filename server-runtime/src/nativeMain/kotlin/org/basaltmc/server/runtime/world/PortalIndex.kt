package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.portal.PortalShape
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIO
import org.basaltmc.wire.nbt.NbtLongArray

/**
 * Where the portals of one world are, chunk by chunk, whether or not that chunk is in memory.
 *
 * parity: `PoiManager`'s `nether_portal` index, which is what lets a portal on the other side be
 * found again instead of a second one being built beside it. This keeps the same promise with far
 * less machinery: a chunk's entry is rewritten every time the chunk is scanned, so what a loaded
 * chunk says is always current, and the rest survives on disk in `portals.nbt`.
 */
public class PortalIndex {
    private val byChunk = HashMap<Long, LongArray>()

    public var dirty: Boolean = false
        private set

    public val size: Int get() = byChunk.values.sumOf { it.size }

    /** Replaces what is known about a chunk with what its blocks actually say. */
    public fun scan(chunk: Chunk) {
        val found = mutableListOf<Long>()
        for (index in 0 until chunk.sectionCount) {
            val section = chunk.section(index)
            // A section whose palette holds no portal block at all is skipped without reading one.
            if (!section.mayContain { PortalShape.isPortal(it) }) continue
            val sectionMinY = chunk.verticalBounds.minY + index * SECTION_HEIGHT
            for (localY in 0 until SECTION_HEIGHT) {
                for (localZ in 0 until SECTION_WIDTH) {
                    for (localX in 0 until SECTION_WIDTH) {
                        if (!PortalShape.isPortal(section.getBlockStateId(localX, localY, localZ))) continue
                        found +=
                            pack(
                                chunk.chunkX * SECTION_WIDTH + localX,
                                sectionMinY + localY,
                                chunk.chunkZ * SECTION_WIDTH + localZ,
                            )
                    }
                }
            }
        }
        put(key(chunk.chunkX, chunk.chunkZ), found.toLongArray())
    }

    /** The nearest portal block to a point, or null when the index knows of none in range. */
    public fun nearest(
        x: Int,
        y: Int,
        z: Int,
        radius: Int,
    ): IntArray? {
        var best: IntArray? = null
        var bestDistance = Long.MAX_VALUE
        for (chunkX in ((x - radius) shr 4)..((x + radius) shr 4)) {
            for (chunkZ in ((z - radius) shr 4)..((z + radius) shr 4)) {
                for (packed in byChunk[key(chunkX, chunkZ)] ?: continue) {
                    val blockX = unpackX(packed)
                    val blockY = unpackY(packed)
                    val blockZ = unpackZ(packed)
                    if (blockX !in x - radius..x + radius || blockZ !in z - radius..z + radius) continue
                    val dx = (blockX - x).toLong()
                    val dy = (blockY - y).toLong()
                    val dz = (blockZ - z).toLong()
                    val distance = dx * dx + dy * dy + dz * dz
                    if (distance >= bestDistance) continue
                    bestDistance = distance
                    best = intArrayOf(blockX, blockY, blockZ)
                }
            }
        }
        return best
    }

    /** Reads `portals.nbt`, which is what a chunk that is no longer in memory is remembered by. */
    public fun load(worldRoot: String) {
        val bytes = FileIO.readBytes(pathFor(worldRoot)) ?: return
        val root = runCatching { NbtIO.read(bytes).tag as? NbtCompound }.getOrNull() ?: return
        for ((name, value) in root) {
            val positions = (value as? NbtLongArray)?.value ?: continue
            byChunk[name.toLongOrNull() ?: continue] = positions
        }
        dirty = false
    }

    public fun save(worldRoot: String) {
        if (!dirty) return
        val root = NbtCompound()
        for ((chunk, positions) in byChunk) {
            if (positions.isEmpty()) continue
            root[chunk.toString()] = NbtLongArray(positions)
        }
        FileIO.writeBytes(pathFor(worldRoot), NbtIO.write(root))
        dirty = false
    }

    private fun put(
        chunk: Long,
        positions: LongArray,
    ) {
        val previous = byChunk[chunk]
        if (previous != null && previous.contentEquals(positions)) return
        if (positions.isEmpty()) byChunk.remove(chunk) else byChunk[chunk] = positions
        dirty = true
    }

    private companion object {
        fun pathFor(worldRoot: String): String = "$worldRoot/portals.nbt"

        const val SECTION_HEIGHT = 16
        const val SECTION_WIDTH = 16

        fun key(
            chunkX: Int,
            chunkZ: Int,
        ): Long = (chunkX.toLong() shl 32) or (chunkZ.toLong() and 0xFFFFFFFFL)

        // 26 bits of x and z, 12 of y: the same shape a block position takes on the wire.
        fun pack(
            x: Int,
            y: Int,
            z: Int,
        ): Long = (x.toLong() and 0x3FFFFFF shl 38) or (z.toLong() and 0x3FFFFFF shl 12) or (y.toLong() and 0xFFF)

        fun unpackX(packed: Long): Int = (packed shr 38).toInt()

        fun unpackZ(packed: Long): Int = (packed shl 26 shr 38).toInt()

        fun unpackY(packed: Long): Int = (packed shl 52 shr 52).toInt()
    }
}
