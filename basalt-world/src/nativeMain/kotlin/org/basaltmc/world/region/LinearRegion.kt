package org.basaltmc.world.region

import org.basaltmc.io.ByteBuffer
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

    public fun write(compressionLevel: Int = 1): ByteArray {
        val inner = ByteBuffer(INNER_HEADER_SIZE)
        var newestTimestamp = 0
        var chunkCount = 0
        for (i in 0 until DIMENSION * DIMENSION) {
            val data = chunks[i]
            if (data != null) {
                inner.writeInt(data.size)
                inner.writeInt(timestamps[i])
                if (timestamps[i] > newestTimestamp) newestTimestamp = timestamps[i]
                chunkCount++
            } else {
                inner.writeInt(0)
                inner.writeInt(0)
            }
        }
        for (data in chunks) if (data != null) inner.writeBytes(data)

        val compressed = Zstd.compress(inner.toByteArray(), compressionLevel)

        val out = ByteBuffer(HEADER_SIZE + compressed.size + FOOTER_SIZE)
        out.writeLong(SIGNATURE)
        out.writeByte(VERSION.toByte())
        out.writeLong(newestTimestamp.toLong())
        out.writeByte(compressionLevel.toByte())
        out.writeShort(chunkCount.toShort())
        out.writeInt(compressed.size)
        out.writeLong(0L)
        out.writeBytes(compressed)
        out.writeLong(SIGNATURE)
        return out.toByteArray()
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
