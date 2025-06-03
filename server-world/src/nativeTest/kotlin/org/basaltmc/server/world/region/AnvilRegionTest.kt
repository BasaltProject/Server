package org.basaltmc.server.world.region

import org.basaltmc.wire.io.Zlib
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class AnvilRegionTest {
    // Builds a minimal, real-shaped `.mca` byte array by hand: a 2-sector header/timestamps block
    // followed by one 4096-byte-aligned sector per chunk, each `[u32 length][u8 compression type
    // 2][zlib data]` padded with zeros to the next sector boundary. Mirrors `write_region_anvil` in
    // the real reference tool, not just whatever `AnvilRegion.read` happens to expect.
    private fun buildAnvilFile(chunks: Map<Pair<Int, Int>, Pair<ByteArray, Int>>): ByteArray {
        val sectors = mutableListOf<ByteArray>()
        val header = ByteArray(4096)
        val timestamps = ByteArray(4096)
        var nextSector = 2

        for ((pos, entry) in chunks) {
            val (localX, localZ) = pos
            val (raw, timestamp) = entry
            val compressed = Zlib.compress(raw)
            val body = ByteArray(5 + compressed.size)
            val length = compressed.size + 1
            body[0] = (length ushr 24).toByte()
            body[1] = (length ushr 16).toByte()
            body[2] = (length ushr 8).toByte()
            body[3] = length.toByte()
            body[4] = 2 // zlib
            compressed.copyInto(body, 5)

            val sectorCount = (body.size + 4095) / 4096
            val padded = body.copyOf(sectorCount * 4096)
            sectors += padded

            val slot = localX + localZ * 32
            val headerOffset = slot * 4
            header[headerOffset] = (nextSector ushr 16).toByte()
            header[headerOffset + 1] = (nextSector ushr 8).toByte()
            header[headerOffset + 2] = nextSector.toByte()
            header[headerOffset + 3] = sectorCount.toByte()

            val tsOffset = slot * 4
            timestamps[tsOffset] = (timestamp ushr 24).toByte()
            timestamps[tsOffset + 1] = (timestamp ushr 16).toByte()
            timestamps[tsOffset + 2] = (timestamp ushr 8).toByte()
            timestamps[tsOffset + 3] = timestamp.toByte()

            nextSector += sectorCount
        }

        var result = header + timestamps
        for (sector in sectors) result += sector
        return result
    }

    @Test
    fun importsChunksIntoALinearRegion() {
        val file =
            buildAnvilFile(
                mapOf(
                    (0 to 0) to ("anvil chunk at 0,0".encodeToByteArray() to 1111),
                    (5 to 3) to (ByteArray(9000) { (it % 200).toByte() } to 2222),
                    (31 to 31) to ("corner chunk".encodeToByteArray() to 3333),
                ),
            )

        val region = AnvilRegion.read(file)

        assertEquals("anvil chunk at 0,0", region.chunkAt(0, 0)!!.decodeToString())
        assertEquals(1111, region.timestampAt(0, 0))
        assertEquals(ByteArray(9000) { (it % 200).toByte() }.toList(), region.chunkAt(5, 3)!!.toList())
        assertEquals(2222, region.timestampAt(5, 3))
        assertEquals("corner chunk", region.chunkAt(31, 31)!!.decodeToString())
        assertEquals(3333, region.timestampAt(31, 31))
        assertNull(region.chunkAt(1, 1))
    }

    @Test
    fun anEmptyRegionHasNoChunks() {
        val region = AnvilRegion.read(buildAnvilFile(emptyMap()))
        for (x in 0 until LinearRegion.DIMENSION) {
            for (z in 0 until LinearRegion.DIMENSION) {
                assertNull(region.chunkAt(x, z))
            }
        }
    }

    @Test
    fun canBeReWrittenAsALinearRegionAndReadBack() {
        val file = buildAnvilFile(mapOf((0 to 0) to ("round trip through both formats".encodeToByteArray() to 42)))
        val imported = AnvilRegion.read(file)
        val relinked = LinearRegion.read(imported.write())
        assertEquals("round trip through both formats", relinked.chunkAt(0, 0)!!.decodeToString())
        assertEquals(42, relinked.timestampAt(0, 0))
    }

    @Test
    fun unsupportedCompressionTypeThrows() {
        val header = ByteArray(4096)
        header[3] = 1 // sector offset 2, sector count 1
        header[2] = 2
        val timestamps = ByteArray(4096)
        val body = ByteArray(4096)
        body[3] = 1 // length 1 (just the compression type byte, no payload)
        body[4] = 1 // gzip: unsupported
        val file = header + timestamps + body
        assertFailsWith<AnvilFormatException> { AnvilRegion.read(file) }
    }

    @Test
    fun tooShortInputThrows() {
        assertFailsWith<AnvilFormatException> { AnvilRegion.read(ByteArray(100)) }
    }
}
