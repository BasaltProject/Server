package org.basaltmc.server.world

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.play.BitStorage
import org.basaltmc.wire.protocol.play.NibbleArray
import org.basaltmc.wire.protocol.play.PalettedContainer

/**
 * Turns a [Chunk] into the wire bytes `MapChunkPacket.chunkData` and `UpdateLightPacket` need.
 * Biomes come from each section's quart-resolution palette.
 *
 * Light is uniform per section rather than propagated: no skylight below the chunk's tallest
 * column, full skylight at or above it. A real light engine belongs to a later pass. The tallest
 * column, not `heightmap()[0]`, because that only holds for a flat world: with real terrain it
 * marked hills skyless and rendered the sky black over lit ground.
 */
public object ChunkDataCodec {
    private const val BLOCK_DIRECT_BITS = 8
    private const val BIOME_DIRECT_BITS = 4

    public fun encodeSections(chunk: Chunk): ByteArray {
        val buffer = ByteBuffer()
        for (i in 0 until chunk.sectionCount) {
            val section = chunk.section(i)
            buffer.writeShort(section.nonAirBlockCount.toShort())
            // LevelChunkSection.write always writes two shorts (non-empty block count, then fluid
            // count), never just one: confirmed live, a real client overran the buffer reading the
            // next section's data because this field was missing entirely.
            buffer.writeShort(section.nonEmptyFluidCount.toShort())
            PalettedContainer.write(
                buffer,
                section.blockStateIds(),
                PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
                PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
                BLOCK_DIRECT_BITS,
            )
            PalettedContainer.write(
                buffer,
                section.biomeIds(),
                PalettedContainer.BIOME_MIN_INDIRECT_BITS,
                PalettedContainer.BIOME_MAX_INDIRECT_BITS,
                BIOME_DIRECT_BITS,
            )
        }
        return buffer.toByteArray()
    }

    // Vanilla's Heightmap packs 256 values (relative to the chunk minimum, not absolute Y) into a
    // SimpleBitStorage sized `ceillog2(worldHeight + 1)` bits per entry: sending one raw long per
    // column instead (256 unpacked longs) doesn't crash but is silently ignored client-side ("size
    // does not match; expected: 37, got: 256"), confirmed live, which leaves the client's own
    // heightmap unprimed and the "loading terrain" screen never dismisses.
    public fun encodeHeightmap(chunk: Chunk): LongArray {
        val bitsPerEntry = ceilLog2(chunk.verticalBounds.height + 1)
        val storage = BitStorage(bitsPerEntry, 256)
        val values = chunk.heightmap()
        for (i in values.indices) storage[i] = values[i] - chunk.verticalBounds.minY
        return storage.data
    }

    private fun ceilLog2(value: Int): Int {
        var bits = 0
        while ((1 shl bits) < value) bits++
        return bits
    }

    public data class LightData(
        val skyLightMask: LongArray,
        val emptySkyLightMask: LongArray,
        val skyLight: List<ByteArray>,
    )

    public fun encodeLight(
        chunk: Chunk,
        hasSkylight: Boolean = true,
    ): LightData {
        if (!hasSkylight) return LightData(longArrayOf(), longArrayOf(), emptyList())
        val surfaceY = chunk.heightmap().max()
        var maskBits = 0L
        var emptyMaskBits = 0L
        val arrays = mutableListOf<ByteArray>()
        for (i in 0 until chunk.sectionCount) {
            val sectionMinY = chunk.verticalBounds.minY + i * 16
            if (sectionMinY + 16 <= surfaceY) {
                emptyMaskBits = emptyMaskBits or (1L shl i)
            } else {
                maskBits = maskBits or (1L shl i)
                arrays.add(NibbleArray.uniform(15))
            }
        }
        return LightData(longArrayOf(maskBits), longArrayOf(emptyMaskBits), arrays)
    }
}
