package org.basaltmc.server.world

import org.basaltmc.server.world.light.BlockLight
import org.basaltmc.server.world.light.LightWindow
import org.basaltmc.server.world.light.SkyLight
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.play.BitStorage
import org.basaltmc.wire.protocol.play.NibbleArray
import org.basaltmc.wire.protocol.play.PalettedContainer

/**
 * Turns a [Chunk] into the wire bytes `MapChunkPacket.chunkData` and `UpdateLightPacket` need.
 * Biomes come from each section's quart-resolution palette.
 *
 * Sky light comes from [SkyLight], which lights each column and spreads sideways from there.
 */
public object ChunkDataCodec {
    private const val BLOCK_DIRECT_BITS = 8
    private const val BIOME_DIRECT_BITS = 4
    private const val BLOCKS_PER_SECTION = 16 * 16 * 16
    private const val BIOMES_PER_SECTION = 4 * 4 * 4

    public fun encodeSections(chunk: Chunk): ByteArray {
        val buffer = ByteBuffer()
        // One scratch per expansion, shared across all sections: expanding each section into its
        // own fresh array put ~400 KB of garbage behind every chunk sent.
        val blockScratch = IntArray(BLOCKS_PER_SECTION)
        val biomeScratch = IntArray(BIOMES_PER_SECTION)
        for (i in 0 until chunk.sectionCount) {
            val section = chunk.section(i)
            buffer.writeShort(section.nonAirBlockCount.toShort())
            // LevelChunkSection.write always writes two shorts (non-empty block count, then fluid
            // count), never just one: confirmed live, a real client overran the buffer reading the
            // next section's data because this field was missing entirely.
            buffer.writeShort(section.nonEmptyFluidCount.toShort())
            section.copyBlockStateIdsInto(blockScratch)
            PalettedContainer.write(
                buffer,
                blockScratch,
                PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
                PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
                BLOCK_DIRECT_BITS,
            )
            section.copyBiomeIdsInto(biomeScratch)
            PalettedContainer.write(
                buffer,
                biomeScratch,
                PalettedContainer.BIOME_MIN_INDIRECT_BITS,
                PalettedContainer.BIOME_MAX_INDIRECT_BITS,
                BIOME_DIRECT_BITS,
            )
        }
        return buffer.toByteArray()
    }

    /**
     * Just the biome containers, in section order: the body of one column in `chunks_biomes`.
     *
     * parity: `ClientboundChunksBiomesPacket.ChunkBiomeData.extractChunkData`, which writes the same
     * containers [encodeSections] does and nothing else, so a client can swap them in place without
     * touching blocks or light.
     */
    public fun encodeBiomes(chunk: Chunk): ByteArray {
        val buffer = ByteBuffer()
        val biomeScratch = IntArray(BIOMES_PER_SECTION)
        for (i in 0 until chunk.sectionCount) {
            chunk.section(i).copyBiomeIdsInto(biomeScratch)
            PalettedContainer.write(
                buffer,
                biomeScratch,
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
        val blockLightMask: LongArray = longArrayOf(0L),
        val emptyBlockLightMask: LongArray = longArrayOf(0L),
        val blockLight: List<ByteArray> = emptyList(),
    )

    /**
     * The light of one chunk, section by section, computed once and kept on the chunk itself.
     *
     * [window] is the chunk and its eight neighbours: light reaches less than a chunk, so that is
     * everything that can shine into this one.
     */
    public fun encodeLight(
        chunk: Chunk,
        hasSkylight: Boolean = true,
        window: LightWindow = LightWindow(chunk) { _, _ -> null },
    ): LightData {
        val sky =
            if (!hasSkylight) {
                null
            } else {
                chunk.skyLight {
                    val computed = SkyLight.compute(window)
                    Array(computed.size) { NibbleArray.pack(computed[it]) }
                }
            }
        val block =
            chunk.blockLight {
                val computed = BlockLight.compute(window) ?: return@blockLight emptyArray()
                Array(computed.size) { NibbleArray.pack(computed[it]) }
            }
        val (skyMask, emptySkyMask, skyArrays) = masked(sky)
        val (blockMask, emptyBlockMask, blockArrays) = masked(block)
        return LightData(skyMask, emptySkyMask, skyArrays, blockMask, emptyBlockMask, blockArrays)
    }

    /** Splits packed sections into the two masks and the arrays the packet carries. */
    private fun masked(sections: Array<ByteArray>?): Triple<LongArray, LongArray, List<ByteArray>> {
        if (sections == null || sections.isEmpty()) return Triple(longArrayOf(0L), longArrayOf(0L), emptyList())
        var maskBits = 0L
        var emptyMaskBits = 0L
        val arrays = mutableListOf<ByteArray>()
        for (i in sections.indices) {
            val packed = sections[i]
            if (packed.all { it == 0.toByte() }) {
                emptyMaskBits = emptyMaskBits or (1L shl i)
            } else {
                maskBits = maskBits or (1L shl i)
                arrays.add(packed)
            }
        }
        return Triple(longArrayOf(maskBits), longArrayOf(emptyMaskBits), arrays)
    }
}
