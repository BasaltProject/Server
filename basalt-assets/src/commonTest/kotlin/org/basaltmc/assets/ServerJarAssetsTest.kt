package org.basaltmc.assets

import org.basaltmc.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class ServerJarAssetsTest {
    // A real, nested bundler ZIP built with Python's `zipfile` module: an outer jar containing
    // `META-INF/versions/1.0/server-1.0.jar` (itself a real jar), matching the real `server.jar`
    // layout confirmed against the actual 26.2 download. The inner jar has two entries under
    // `data/minecraft/worldgen/noise/`.
    private val bundlerBytes = BUNDLER_FIXTURE_BASE64.decodeBase64()
    private val path = "./server_jar_assets_test.jar"

    @AfterTest
    fun cleanup() {
        platform.posix.remove(path)
    }

    private fun withFixture(): ServerJarAssets {
        FileIO.writeBytes(path, bundlerBytes)
        return ServerJarAssets(path, "1.0")
    }

    @Test
    fun readsAnEntryFromTheUnwrappedInnerJar() {
        val assets = withFixture()
        val text = assets.readText("data/minecraft/worldgen/noise/test.json")
        assertEquals("""{"firstOctave": -9, "amplitudes": [1.0, 2.0]}""", text)
    }

    @Test
    fun readEntryReturnsNullForAMissingEntry() {
        val assets = withFixture()
        assertNull(assets.readEntry("data/minecraft/worldgen/noise/does_not_exist.json"))
    }

    @Test
    fun readTextThrowsForAMissingEntry() {
        val assets = withFixture()
        assertFailsWith<AssetException> { assets.readText("data/minecraft/worldgen/noise/does_not_exist.json") }
    }

    @Test
    fun entriesUnderListsEveryMatchingEntry() {
        val assets = withFixture()
        val entries = assets.entriesUnder("data/minecraft/worldgen/noise/")
        assertEquals(
            setOf("data/minecraft/worldgen/noise/test.json", "data/minecraft/worldgen/noise/other.json"),
            entries.toSet(),
        )
    }

    @Test
    fun throwsWhenTheServerJarIsMissing() {
        assertFailsWith<AssetException> { ServerJarAssets("./does_not_exist.jar", "1.0") }
    }

    @Test
    fun throwsWhenTheJarIsNotActuallyABundler() {
        // A plain (non-bundler) zip with no `META-INF/versions/...` entry at all.
        val plainZipPath = "./server_jar_assets_test_plain.jar"
        FileIO.writeBytes(plainZipPath, PLAIN_ZIP_FIXTURE_BASE64.decodeBase64())
        try {
            assertFailsWith<AssetException> { ServerJarAssets(plainZipPath, "1.0") }
        } finally {
            platform.posix.remove(plainZipPath)
        }
    }
}

private const val PLAIN_ZIP_FIXTURE_BASE64 =
    "UEsDBBQAAAAAACmvB111Y6MgDQAAAA0AAAAJAAAAaGVsbG8udHh0bm90IGEgYnVuZGxlclBLAQIUAxQAAAAAACmvB111Y6MgDQAAAA0AAAAJ" +
        "AAAAAAAAAAAAAACAAQAAAABoZWxsby50eHRQSwUGAAAAAAEAAQA3AAAANAAAAAAA"

private const val BUNDLER_FIXTURE_BASE64 =
    "UEsDBBQAAAAAAAavB10TE2MZowEAAKMBAAAkAAAATUVUQS1JTkYvdmVyc2lvbnMvMS4wL3NlcnZlci0xLjAuamFyUEsDBBQAAAAIAAavB10f" +
        "7ROqLgAAAC0AAAAnAAAAZGF0YS9taW5lY3JhZnQvd29ybGRnZW4vbm9pc2UvdGVzdC5qc29uq1ZKyywqLvFPLkksS1WyUtC11FFQSswt" +
        "yMksKU1JLQaKRBvqGegoGOkZxNYCAFBLAwQUAAAACAAGrwddjU0h3ikAAAAoAAAAKAAAAGRhdGEvbWluZWNyYWZ0L3dvcmxkZ2VuL25v" +
        "aXNlL290aGVyLmpzb26rVkrLLCou8U8uSSxLVbJS0DXWUVBKzC3IySwpTUktBopEG+oZxNYCAFBLAQIUAxQAAAAIAAavB10f7ROqLgAA" +
        "AC0AAAAnAAAAAAAAAAAAAACAAQAAAABkYXRhL21pbmVjcmFmdC93b3JsZGdlbi9ub2lzZS90ZXN0Lmpzb25QSwECFAMUAAAACAAGrwdd" +
        "jU0h3ikAAAAoAAAAKAAAAAAAAAAAAAAAgAFzAAAAZGF0YS9taW5lY3JhZnQvd29ybGRnZW4vbm9pc2Uvb3RoZXIuanNvblBLBQYAAAAA" +
        "AgACAKsAAADiAAAAAABQSwECFAMUAAAAAAAGrwddExNjGaMBAACjAQAAJAAAAAAAAAAAAAAAgAEAAAAATUVUQS1JTkYvdmVyc2lvbnMv" +
        "MS4wL3NlcnZlci0xLjAuamFyUEsFBgAAAAABAAEAUgAAAOUBAAAAAA=="

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
