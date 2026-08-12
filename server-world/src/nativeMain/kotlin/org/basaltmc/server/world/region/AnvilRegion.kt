package org.basaltmc.server.world.region

import org.basaltmc.wire.io.Zlib

/** Thrown for a corrupt or unsupported Anvil (`.mca`) region file. */
public class AnvilFormatException(
    message: String,
) : Exception(message)

/**
 * Reads an Anvil (`.mca`) region into a [LinearRegion], for the one-time import an existing
 * vanilla, Paper or Spigot world needs. Layout taken from `linear.py`'s `open_region_anvil`: a
 * 4096-byte header of 1024 `(3-byte sector offset, 1-byte count)` entries, 1024 timestamps, then
 * chunks in 4096-byte sectors, each a big-endian length, a compression type byte and the NBT.
 *
 * Only the container is transcoded: the NBT reaches [LinearRegion.setChunk] decompressed but
 * untouched. Gzip (type 1) and the `.mcc` indirection for oversized chunks throw instead of
 * skipping a chunk silently, neither having come up in practice.
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
