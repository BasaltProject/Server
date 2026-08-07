package org.basaltmc.world

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.play.NibbleArray
import org.basaltmc.protocol.play.PalettedContainer
import org.basaltmc.registry.Biomes

/**
 * Turns a [Chunk] into the wire bytes `basalt-protocol`'s `MapChunkPacket.chunkData` and
 * `UpdateLightPacket` need. Biomes are always [Biomes.PLAINS]: `basalt-world` does not vary
 * biomes yet. Light is a uniform value per section, not real propagation: sections entirely
 * above the flat world's surface get full skylight, sections at or below get none. This is the
 * "luz básica" a flat world needs, not a general light engine (that belongs to a later pass with
 * real terrain to propagate light across).
 */
public object ChunkDataCodec {
    private const val BLOCK_DIRECT_BITS = 8
    private const val BIOME_DIRECT_BITS = 4
    private val PLAINS_ID = Biomes.REGISTRY.idOf(Biomes.PLAINS.id) ?: 0

    public fun encodeSections(chunk: Chunk): ByteArray {
        val buffer = ByteBuffer()
        val biomeValues = IntArray(64) { PLAINS_ID }
        for (i in 0 until chunk.sectionCount) {
            val section = chunk.section(i)
            buffer.writeShort(section.nonAirBlockCount.toShort())
            PalettedContainer.write(
                buffer,
                section.blockStateIds(),
                PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
                PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
                BLOCK_DIRECT_BITS,
            )
            PalettedContainer.write(
                buffer,
                biomeValues,
                PalettedContainer.BIOME_MIN_INDIRECT_BITS,
                PalettedContainer.BIOME_MAX_INDIRECT_BITS,
                BIOME_DIRECT_BITS,
            )
        }
        return buffer.toByteArray()
    }

    public data class LightData(
        val skyLightMask: LongArray,
        val emptySkyLightMask: LongArray,
        val skyLight: List<ByteArray>,
    )

    public fun encodeLight(chunk: Chunk): LightData {
        val surfaceY = chunk.heightmap()[0]
        var maskBits = 0L
        var emptyMaskBits = 0L
        val arrays = mutableListOf<ByteArray>()
        for (i in 0 until chunk.sectionCount) {
            val sectionMinY = WorldHeight.MIN_Y + i * 16
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
