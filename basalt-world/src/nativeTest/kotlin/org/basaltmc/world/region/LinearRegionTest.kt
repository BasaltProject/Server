package org.basaltmc.world.region

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

class LinearRegionTest {
    @Test
    fun anEmptyRegionRoundTrips() {
        val region = LinearRegion()
        val bytes = region.write()
        val read = LinearRegion.read(bytes)
        for (x in 0 until LinearRegion.DIMENSION) {
            for (z in 0 until LinearRegion.DIMENSION) {
                assertNull(read.chunkAt(x, z))
            }
        }
    }

    @Test
    fun aFewChunksRoundTripWithTheirTimestamps() {
        val region = LinearRegion()
        region.setChunk(0, 0, "chunk at 0,0".encodeToByteArray(), timestamp = 1000)
        region.setChunk(31, 31, "chunk at 31,31".encodeToByteArray(), timestamp = 2000)
        region.setChunk(15, 20, ByteArray(10_000) { (it % 200).toByte() }, timestamp = 3000)

        val read = LinearRegion.read(region.write())

        assertEquals("chunk at 0,0", read.chunkAt(0, 0)!!.decodeToString())
        assertEquals(1000, read.timestampAt(0, 0))
        assertEquals("chunk at 31,31", read.chunkAt(31, 31)!!.decodeToString())
        assertEquals(2000, read.timestampAt(31, 31))
        assertEquals(ByteArray(10_000) { (it % 200).toByte() }.toList(), read.chunkAt(15, 20)!!.toList())
        assertEquals(3000, read.timestampAt(15, 20))
        assertNull(read.chunkAt(1, 1))
    }

    @Test
    fun readsARealFileWrittenByTheReferenceLinearPyTool() {
        // Built with the actual `linear.py` reference implementation (not our own writer): four
        // synthetic chunks at (0,0)/(5,3)/(31,31)/(15,15), zstd level 3. Proves our reader parses
        // the real binary layout, not just whatever our own writer happens to produce.
        val bytes = REFERENCE_FIXTURE_BASE64.decodeBase64()
        val region = LinearRegion.read(bytes)

        assertTrue(region.chunkAt(0, 0)!!.decodeToString().startsWith("synthetic chunk blob 0,0 "))
        assertEquals(1000, region.timestampAt(0, 0))
        assertEquals("another chunk 5,3", region.chunkAt(5, 3)!!.decodeToString())
        assertEquals(2000, region.timestampAt(5, 3))
        assertEquals(ByteArray(5000) { 'x'.code.toByte() }.toList(), region.chunkAt(15, 15)!!.toList())
        assertEquals(12345, region.timestampAt(15, 15))
        assertTrue(region.chunkAt(31, 31)!!.decodeToString().startsWith("corner chunk 31,31"))
        assertEquals(3000, region.timestampAt(31, 31))
        assertNull(region.chunkAt(1, 1))
    }

    @Test
    fun tooShortInputThrows() {
        assertFailsWith<LinearFormatException> { LinearRegion.read(ByteArray(10)) }
    }

    @Test
    fun wrongSuperblockSignatureThrows() {
        val bytes = LinearRegion().write()
        bytes[0] = (bytes[0].toInt() xor 0xFF).toByte()
        assertFailsWith<LinearFormatException> { LinearRegion.read(bytes) }
    }

    @Test
    fun wrongFooterSignatureThrows() {
        val bytes = LinearRegion().write()
        bytes[bytes.size - 1] = (bytes[bytes.size - 1].toInt() xor 0xFF).toByte()
        assertFailsWith<LinearFormatException> { LinearRegion.read(bytes) }
    }

    @Test
    fun outOfRangeCoordinatesThrow() {
        val region = LinearRegion()
        assertFailsWith<IllegalArgumentException> { region.setChunk(32, 0, byteArrayOf(1), 0) }
        assertFailsWith<IllegalArgumentException> { region.chunkAt(-1, 0) }
    }
}

private const val REFERENCE_FIXTURE_BASE64 =
    "w/8TGDzKnZoBAAAAAAAAMDkDAAQAAAB9AAAAAAAAAAAotS/9ZKU3fQMABAUAAATiAAAD6AARAAAH0AATiAAAMDkAKgAAC7hzeW50" +
        "aGV0aWMgY2h1bmsgYmxvYiAwLDAgYW5vdGhlcjUsM3hjb3JuMzEsMzEgcGFkZGluZwgAK+hvzikI4eA6GSe7RmN4/u4OQG+MCPzy" +
        "sQJHbjsFJMP/Exg8yp2a"

private fun String.decodeBase64(): ByteArray {
    val table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    val clean = trimEnd('=')
    val output = ArrayList<Byte>((clean.length * 3) / 4 + 3)
    var buffer = 0
    var bitsCollected = 0
    for (char in clean) {
        val value = table.indexOf(char)
        require(value >= 0) { "invalid base64 character: $char" }
        buffer = (buffer shl 6) or value
        bitsCollected += 6
        if (bitsCollected >= 8) {
            bitsCollected -= 8
            output.add(((buffer shr bitsCollected) and 0xFF).toByte())
        }
    }
    return output.toByteArray()
}
