package org.basaltmc.world.region

import org.basaltmc.io.Zlib

/** Thrown for a corrupt or unsupported Anvil (`.mca`) region file. */
public class AnvilFormatException(
    message: String,
) : Exception(message)

/**
 * Reads a real Anvil (`.mca`) region file into a [LinearRegion], for the automatic one-time import
 * an existing vanilla/Paper/Spigot world needs when Basalt opens it (see [LinearRegion]'s own KDoc
 * for the target container format). Layout confirmed against `linear.py`'s `open_region_anvil`, the
 * reference tool this whole Linear stack is verified against, not guessed: a 4096-byte header of
 * 1024 `(3-byte big-endian sector offset, 1-byte sector count)` entries, a second 4096-byte block of
 * 1024 big-endian timestamps, then the chunk data itself in 4096-byte sectors, each a 4-byte
 * big-endian length (covering the compression-type byte and everything after it), a 1-byte
 * compression type, then that many bytes of (usually compressed) chunk NBT.
 *
 * This only transcodes the container: the chunk NBT bytes it hands to [LinearRegion.setChunk] are
 * exactly what Anvil had, decompressed but otherwise untouched, ready to be recompressed as part of
 * a whole region the next time [LinearRegion.write] runs. Only zlib (compression type 2, what every
 * modern save uses) and uncompressed (type 3) chunks are supported; gzip (type 1, pre-1.3-era
 * saves) and the external `.mcc` file indirection for oversized chunks (type 128+n) both throw
 * rather than silently skipping a chunk, since neither has come up in practice yet.
 */
public object AnvilRegion {
    private const val SECTOR_SIZE = 4096
    private const val HEADER_SIZE = SECTOR_SIZE
    private const val TIMESTAMPS_SIZE = SECTOR_SIZE
    private const val COMPRESSION_ZLIB = 2
    private const val COMPRESSION_NONE = 3

    public fun read(bytes: ByteArray): LinearRegion {
        if (bytes.size < HEADER_SIZE + TIMESTAMPS_SIZE) {
            throw AnvilFormatException("too short to be an Anvil region: ${bytes.size} bytes")
        }

        val region = LinearRegion()
        for (i in 0 until LinearRegion.DIMENSION * LinearRegion.DIMENSION) {
            val headerOffset = i * 4
            val sectorOffset =
                ((bytes[headerOffset].toInt() and 0xFF) shl 16) or
                    ((bytes[headerOffset + 1].toInt() and 0xFF) shl 8) or
                    (bytes[headerOffset + 2].toInt() and 0xFF)
            val sectorCount = bytes[headerOffset + 3].toInt() and 0xFF
            if (sectorOffset == 0 || sectorCount == 0) continue

            val timestamp = readInt32(bytes, TIMESTAMPS_SIZE + i * 4)
            val chunkStart = sectorOffset * SECTOR_SIZE
            val length = readInt32(bytes, chunkStart)
            val compressionType = bytes[chunkStart + 4].toInt() and 0xFF
            val dataStart = chunkStart + 5
            val compressed = bytes.copyOfRange(dataStart, dataStart + length - 1)

            val raw =
                when (compressionType) {
                    COMPRESSION_ZLIB -> Zlib.decompressUnknownSize(compressed)
                    COMPRESSION_NONE -> compressed
                    else -> throw AnvilFormatException(
                        "unsupported chunk compression type $compressionType at chunk $i",
                    )
                }

            val localX = i % LinearRegion.DIMENSION
            val localZ = i / LinearRegion.DIMENSION
            region.setChunk(localX, localZ, raw, timestamp)
        }
        return region
    }

    private fun readInt32(
        bytes: ByteArray,
        offset: Int,
    ): Int =
        ((bytes[offset].toInt() and 0xFF) shl 24) or
            ((bytes[offset + 1].toInt() and 0xFF) shl 16) or
            ((bytes[offset + 2].toInt() and 0xFF) shl 8) or
            (bytes[offset + 3].toInt() and 0xFF)
}
