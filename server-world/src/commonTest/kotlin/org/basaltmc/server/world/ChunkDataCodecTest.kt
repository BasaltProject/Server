package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.PalettedContainer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ChunkDataCodecTest {
    @Test
    fun encodesOneEntryPerSection() {
        val chunk = FlatWorldGenerator.generate(0, 0)
        val bytes = ChunkDataCodec.encodeSections(chunk)
        assertTrue(bytes.isNotEmpty())
    }

    @Test
    fun blockSectionDecodesBackToTheOriginalStateIds() {
        val chunk = FlatWorldGenerator.generate(0, 0)
        val bytes = ChunkDataCodec.encodeSections(chunk)
        val buffer =
            org.basaltmc.wire.io
                .ByteBuffer()
        buffer.writeBytes(bytes)

        // First section (world floor: bedrock/dirt/dirt/grass/air) is the interesting one.
        // LevelChunkSection.write always writes two shorts (non-empty block count, then fluid
        // count): see ChunkDataCodec.encodeSections's KDoc-style comment.
        val nonAirCount = buffer.readShort()
        buffer.readShort()
        val blockValues = PalettedContainer.read(buffer, 4096, PalettedContainer.BLOCK_MAX_INDIRECT_BITS)
        PalettedContainer.read(buffer, 64, PalettedContainer.BIOME_MAX_INDIRECT_BITS)

        assertEquals(chunk.section(0).blockStateIds().toList(), blockValues.toList())
        assertEquals(chunk.section(0).nonAirBlockCount, nonAirCount.toInt())
    }

    @Test
    fun biomesAloneDecodeBackToWhatTheChunkHolds() {
        val chunk = FlatWorldGenerator.generate(0, 0)
        chunk.setBiomeId(0, 0, 0, 7)
        val buffer =
            org.basaltmc.wire.io
                .ByteBuffer()
        buffer.writeBytes(ChunkDataCodec.encodeBiomes(chunk))

        val first = PalettedContainer.read(buffer, 64, PalettedContainer.BIOME_MAX_INDIRECT_BITS)
        assertEquals(7, first[0])
        // One container per section and nothing else: no block data, no counts, no light.
        for (i in 1 until chunk.sectionCount) {
            PalettedContainer.read(buffer, 64, PalettedContainer.BIOME_MAX_INDIRECT_BITS)
        }
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun lightMasksCoverEverySectionExactlyOnce() {
        val chunk = FlatWorldGenerator.generate(0, 0)
        val light = ChunkDataCodec.encodeLight(chunk)
        val combined = light.skyLightMask[0] or light.emptySkyLightMask[0]
        val overlap = light.skyLightMask[0] and light.emptySkyLightMask[0]
        assertEquals(0L, overlap)
        for (i in 0 until WorldHeight.SECTION_COUNT) {
            assertTrue((combined shr i) and 1L == 1L, "section $i missing from both light masks")
        }
    }

    /** A dimension with no sky sends no sky light: what it has is what its own blocks give off. */
    @Test
    fun dimensionsWithoutSkylightSendNoSkyArrays() {
        val chunk = Chunk(0, 0, ChunkHeight.NETHER)
        val light = ChunkDataCodec.encodeLight(chunk, hasSkylight = false)
        assertEquals(0L, light.skyLightMask[0])
        assertEquals(0L, light.emptySkyLightMask[0])
        assertTrue(light.skyLight.isEmpty())
    }

    @Test
    fun aBurningBlockSendsBlockLightForItsOwnSection() {
        val chunk = Chunk(0, 0, ChunkHeight.NETHER)
        val glowstone = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("glowstone")))
        chunk.setBlockStateId(8, 40, 8, glowstone)

        val light = ChunkDataCodec.encodeLight(chunk, hasSkylight = false)

        // Its own section, and the two its fifteen levels reach into.
        val mask = light.blockLightMask[0]
        assertTrue((mask shr (40 / 16)) and 1L == 1L)
        assertEquals(3, light.blockLight.size)
    }

    @Test
    fun sectionsAboveTheFlatSurfaceHaveFullSkylight() {
        val chunk = FlatWorldGenerator.generate(0, 0)
        val light = ChunkDataCodec.encodeLight(chunk)
        val topSectionIndex = WorldHeight.SECTION_COUNT - 1
        assertTrue((light.skyLightMask[0] shr topSectionIndex) and 1L == 1L)
    }

    @Test
    fun theBottomSectionHasSkylightSinceTheGroundIsOnlyFourBlocksThick() {
        // section 0 spans y=-64..-49; the flat world's surface sits at y=-60, so the same
        // section holds both the shallow crust and 12 blocks of open sunlit air above it.
        val chunk = FlatWorldGenerator.generate(0, 0)
        val light = ChunkDataCodec.encodeLight(chunk)
        assertTrue((light.skyLightMask[0] and 1L) == 1L)
    }
}
