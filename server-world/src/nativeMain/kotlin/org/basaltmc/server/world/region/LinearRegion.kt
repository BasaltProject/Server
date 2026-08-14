package org.basaltmc.server.world.region

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.io.Zstd
import org.basaltmc.wire.io.ZstdStreamReader

/** Thrown for a corrupt or unsupported Linear region file: bad signature, size mismatch, wrong version. */
public class LinearFormatException(
    message: String,
) : Exception(message)

/**
 * One 32x32-chunk Linear region (`.linear`), laid out as `linear.py` has it: a 32-byte big-endian
 * header (signature, version, newest timestamp, compression level, chunk count, compressed length,
 * 8 reserved), one zstd stream over the whole region rather than Anvil's per-chunk zlib, and the
 * signature repeated as a footer. The stream holds 1024 `(size, timestamp)` pairs, row-major with
 * `size == 0` for an empty slot, then the chunk blobs back to back in the same order.
 *
 * Only the container: the chunk bytes are opaque here. Coordinates are local (0..31), and mapping
 * them to absolute chunk coordinates is the caller's job.
 */
public class LinearRegion(
    private val chunks: Array<ByteArray?> = arrayOfNulls(DIMENSION * DIMENSION),
    private val timestamps: IntArray = IntArray(DIMENSION * DIMENSION),
) {
    /** What this region costs in memory. A cache bounded by region count is bounded by nothing. */
    public var residentBytes: Long = chunks.sumOf { it?.size?.toLong() ?: 0L }
        private set

    public fun chunkAt(
        localX: Int,
        localZ: Int,
    ): ByteArray? = chunks[index(localX, localZ)]

    public fun timestampAt(
        localX: Int,
        localZ: Int,
    ): Int = timestamps[index(localX, localZ)]

    public fun setChunk(
        localX: Int,
        localZ: Int,
        data: ByteArray?,
        timestamp: Int,
    ) {
        val i = index(localX, localZ)
        residentBytes += (data?.size ?: 0) - (chunks[i]?.size ?: 0)
        chunks[i] = data
        timestamps[i] = timestamp
    }

    /**
     * Writes this region straight to [path], compressing as it goes: peak memory is a 64 KB output
     * block plus an 8 KB table whatever the region holds. Building the whole region uncompressed,
     * then compressed, then framed cost tens of megabytes each time, and Kotlin/Native serves
     * allocations that size from `mmap` without giving the pages back.
     *
     * The compressed length is only known at the end, so the header goes out with a hole and is
     * patched afterwards.
     */
    public fun writeTo(
        path: String,
        compressionLevel: Int = 1,
    ) {
        var newestTimestamp = 0
        var chunkCount = 0
        for (i in 0 until DIMENSION * DIMENSION) {
            val data = chunks[i] ?: continue
            if (timestamps[i] > newestTimestamp) newestTimestamp = timestamps[i]
            chunkCount++
        }

        val table = ByteArray(INNER_HEADER_SIZE)
        for (i in 0 until DIMENSION * DIMENSION) {
            val data = chunks[i]
            table.writeIntAt(i * 8, data?.size ?: 0)
            table.writeIntAt(i * 8 + 4, if (data != null) timestamps[i] else 0)
        }

        val header = ByteArray(HEADER_SIZE)
        header.writeLongAt(0, SIGNATURE)
        header[8] = VERSION.toByte()
        header.writeLongAt(9, newestTimestamp.toLong())
        header[17] = compressionLevel.toByte()
        header.writeShortAt(18, chunkCount.toShort())
        header.writeIntAt(20, 0) // patched below, once the compressed length is known
        header.writeLongAt(24, 0L)

        val writer = FileIO.openWrite(path)
        try {
            writer.write(header)
            var compressedSize = 0
            Zstd.compressStreaming(
                level = compressionLevel,
                sink = { block, length ->
                    writer.write(block, length)
                    compressedSize += length
                },
                pieces = { feed ->
                    feed(table, table.size)
                    for (data in chunks) if (data != null) feed(data, data.size)
                },
            )

            val footer = ByteArray(FOOTER_SIZE)
            footer.writeLongAt(0, SIGNATURE)
            writer.write(footer)

            val patch = ByteArray(4)
            patch.writeIntAt(0, compressedSize)
            writer.seek(20L)
            writer.write(patch)
        } finally {
            writer.close()
        }
    }

    /**
     * The whole file as bytes. For tests and callers with no path to write to; the server uses
     * [writeTo], which never holds a region-sized buffer at all.
     */
    public fun write(compressionLevel: Int = 1): ByteArray {
        var newestTimestamp = 0
        var chunkCount = 0
        for (i in 0 until DIMENSION * DIMENSION) {
            val data = chunks[i] ?: continue
            if (timestamps[i] > newestTimestamp) newestTimestamp = timestamps[i]
            chunkCount++
        }

        val table = ByteArray(INNER_HEADER_SIZE)
        for (i in 0 until DIMENSION * DIMENSION) {
            val data = chunks[i]
            table.writeIntAt(i * 8, data?.size ?: 0)
            table.writeIntAt(i * 8 + 4, if (data != null) timestamps[i] else 0)
        }

        val body = ByteBuffer()
        Zstd.compressStreaming(
            level = compressionLevel,
            sink = { block, length -> body.writeBytes(block, 0, length) },
            pieces = { feed ->
                feed(table, table.size)
                for (data in chunks) if (data != null) feed(data, data.size)
            },
        )
        val compressed = body.toByteArray()

        val out = ByteArray(HEADER_SIZE + compressed.size + FOOTER_SIZE)
        out.writeLongAt(0, SIGNATURE)
        out[8] = VERSION.toByte()
        out.writeLongAt(9, newestTimestamp.toLong())
        out[17] = compressionLevel.toByte()
        out.writeShortAt(18, chunkCount.toShort())
        out.writeIntAt(20, compressed.size)
        out.writeLongAt(24, 0L)
        compressed.copyInto(out, HEADER_SIZE)
        out.writeLongAt(HEADER_SIZE + compressed.size, SIGNATURE)
        return out
    }

    private fun index(
        localX: Int,
        localZ: Int,
    ): Int {
        require(localX in 0 until DIMENSION && localZ in 0 until DIMENSION) {
            "chunk coordinates out of range: ($localX, $localZ)"
        }
        return localX + localZ * DIMENSION
    }

    public companion object {
        public const val DIMENSION: Int = 32

        // From linear.py: LINEAR_SIGNATURE, the versions its own reader accepts (write_region_linear
        // itself only ever emits version 1), and the fixed 32-byte superblock/8-byte footer sizes.
        private val SIGNATURE: Long = 0xC3FF13183CCA9D9AUL.toLong()
        private const val VERSION: Int = 1
        private val SUPPORTED_VERSIONS = setOf(1, 2)
        private const val HEADER_SIZE = 32
        private const val FOOTER_SIZE = 8
        private const val INNER_HEADER_SIZE = DIMENSION * DIMENSION * 8

        public fun read(bytes: ByteArray): LinearRegion {
            if (bytes.size < HEADER_SIZE + FOOTER_SIZE) {
                throw LinearFormatException("too short to be a Linear region: ${bytes.size} bytes")
            }

            val header = ByteBuffer(HEADER_SIZE).apply { writeBytes(bytes, 0, HEADER_SIZE) }
            if (header.readLong() != SIGNATURE) throw LinearFormatException("bad superblock signature")
            val version = header.readByte().toInt()
            if (version !in SUPPORTED_VERSIONS) throw LinearFormatException("unsupported version $version")
            header.readLong() // newest timestamp: informational, not needed to decode
            header.readByte() // compression level: informational, zstd frames are self-describing
            val declaredChunkCount = header.readShort().toInt()
            val compressedLength = header.readInt()
            header.readLong() // reserved

            val footer = ByteBuffer(FOOTER_SIZE).apply { writeBytes(bytes, bytes.size - FOOTER_SIZE, FOOTER_SIZE) }
            if (footer.readLong() != SIGNATURE) throw LinearFormatException("bad footer signature")

            if (HEADER_SIZE + compressedLength + FOOTER_SIZE != bytes.size) {
                throw LinearFormatException("file size does not match the declared compressed length")
            }

            // Streamed straight into the table and then each chunk's own blob: the whole region
            // decompressed at once was megabytes of transient per read, and every worker thread's
            // allocator kept its high-water mark. Mirrors writeTo, which streams the other way.
            ZstdStreamReader(bytes, HEADER_SIZE, HEADER_SIZE + compressedLength).use { reader ->
                val tableBytes = ByteArray(INNER_HEADER_SIZE)
                runCatching { reader.readFully(tableBytes) }
                    .getOrElse { throw LinearFormatException("decompressed region smaller than its own chunk table") }

                val table = ByteBuffer(INNER_HEADER_SIZE).apply { writeBytes(tableBytes) }
                val sizes = IntArray(DIMENSION * DIMENSION)
                val timestamps = IntArray(DIMENSION * DIMENSION)
                for (i in 0 until DIMENSION * DIMENSION) {
                    sizes[i] = table.readInt()
                    timestamps[i] = table.readInt()
                }

                val chunks = arrayOfNulls<ByteArray>(DIMENSION * DIMENSION)
                var realChunkCount = 0
                for (i in 0 until DIMENSION * DIMENSION) {
                    if (sizes[i] <= 0) continue
                    val blob = ByteArray(sizes[i])
                    runCatching { reader.readFully(blob) }
                        .getOrElse { throw LinearFormatException("decompressed size does not match the chunk table") }
                    chunks[i] = blob
                    realChunkCount++
                }
                if (!reader.frameComplete) {
                    throw LinearFormatException("decompressed size does not match the chunk table")
                }
                if (realChunkCount != declaredChunkCount) {
                    throw LinearFormatException(
                        "chunk count mismatch: header says $declaredChunkCount, found $realChunkCount",
                    )
                }
                return LinearRegion(chunks, timestamps)
            }
        }
    }
}

// Big-endian scalar writes straight into a pre-sized array, so building a region file never needs
// an intermediate growing buffer. Matches ByteBuffer's own encoding, just without the copies.
private fun ByteArray.writeShortAt(
    offset: Int,
    value: Short,
) {
    this[offset] = (value.toInt() ushr 8).toByte()
    this[offset + 1] = value.toByte()
}

private fun ByteArray.writeIntAt(
    offset: Int,
    value: Int,
) {
    this[offset] = (value ushr 24).toByte()
    this[offset + 1] = (value ushr 16).toByte()
    this[offset + 2] = (value ushr 8).toByte()
    this[offset + 3] = value.toByte()
}

private fun ByteArray.writeLongAt(
    offset: Int,
    value: Long,
) {
    for (i in 0 until 8) this[offset + i] = (value ushr (56 - i * 8)).toByte()
}
