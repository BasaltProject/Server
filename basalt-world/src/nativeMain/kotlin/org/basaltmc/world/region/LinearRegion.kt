package org.basaltmc.world.region

import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.FileIO
import org.basaltmc.io.Zstd

/** Thrown for a corrupt or unsupported Linear region file: bad signature, size mismatch, wrong version. */
public class LinearFormatException(
    message: String,
) : Exception(message)

/**
 * One 32x32-chunk Linear region (`.linear`), matching the real format's binary layout
 * (`LinearRegionFileFormatTools/linear.py`, the reference implementation, not guessed): a 32-byte
 * big-endian header (signature, version, newest timestamp, compression level, chunk count,
 * compressed length, 8 reserved bytes), a single zstd stream wrapping the *entire* region at once
 * (unlike Anvil's per-chunk zlib), and the signature repeated as an 8-byte footer. The zstd stream
 * decompresses to a fixed 1024-entry table of `(size, timestamp)` pairs (one per local chunk slot,
 * row-major, `size == 0` meaning no chunk there) followed by the chunk blobs themselves,
 * concatenated back to back with no padding, in the same slot order.
 *
 * This only handles the container: each chunk's bytes are opaque to it (real usage stores a
 * chunk's on-disk NBT there, but nothing here parses that). Coordinates are always local (0..31);
 * mapping a filename's region coordinates to absolute chunk coordinates is a caller concern.
 */
public class LinearRegion(
    private val chunks: Array<ByteArray?> = arrayOfNulls(DIMENSION * DIMENSION),
    private val timestamps: IntArray = IntArray(DIMENSION * DIMENSION),
) {
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
        chunks[i] = data
        timestamps[i] = timestamp
    }

    /**
     * Writes this region straight to [path], compressing as it goes.
     *
     * Peak memory is a fixed 64 KB output block plus an 8 KB chunk table, whatever the region's
     * size. The chunk blobs are already held individually, so nothing needs concatenating: the old
     * approach built the whole region uncompressed, then the whole compressed copy, then the framed
     * copy, tens of megabytes each. Kotlin/Native serves allocations that size from `mmap` and does
     * not give the pages back, so a session spent exploring grew by gigabytes it could never reuse.
     *
     * The header carries the compressed length, which is only known at the end, so it goes out as a
     * hole and is patched once the body is done.
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
            val compressed = bytes.copyOfRange(HEADER_SIZE, HEADER_SIZE + compressedLength)
            val decompressed = Zstd.decompress(compressed)
            if (decompressed.size < INNER_HEADER_SIZE) {
                throw LinearFormatException("decompressed region smaller than its own chunk table")
            }

            val table = ByteBuffer(INNER_HEADER_SIZE).apply { writeBytes(decompressed, 0, INNER_HEADER_SIZE) }
            val sizes = IntArray(DIMENSION * DIMENSION)
            val timestamps = IntArray(DIMENSION * DIMENSION)
            for (i in 0 until DIMENSION * DIMENSION) {
                sizes[i] = table.readInt()
                timestamps[i] = table.readInt()
            }

            val chunks = arrayOfNulls<ByteArray>(DIMENSION * DIMENSION)
            var offset = INNER_HEADER_SIZE
            var realChunkCount = 0
            for (i in 0 until DIMENSION * DIMENSION) {
                if (sizes[i] > 0) {
                    chunks[i] = decompressed.copyOfRange(offset, offset + sizes[i])
                    realChunkCount++
                }
                offset += sizes[i]
            }
            val sizeMismatch = offset != decompressed.size
            if (sizeMismatch) throw LinearFormatException("decompressed size does not match the chunk table")
            if (realChunkCount != declaredChunkCount) {
                throw LinearFormatException(
                    "chunk count mismatch: header says $declaredChunkCount, found $realChunkCount",
                )
            }
            return LinearRegion(chunks, timestamps)
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
