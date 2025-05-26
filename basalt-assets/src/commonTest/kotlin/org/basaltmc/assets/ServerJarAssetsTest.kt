package org.basaltmc.assets

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class ServerJarAssetsTest {
    // A real, nested bundler ZIP built with Python's `zipfile` module: an outer jar containing
    // `META-INF/versions/1.0/server-1.0.jar` (itself a real jar), matching the real `server.jar`
    // layout confirmed against the actual 26.2 download. The inner jar has two entries under
    // `data/minecraft/worldgen/noise/`.
    private val bundlerBytes = BUNDLER_FIXTURE_BASE64.decodeBase64()
    private val path = "./server_jar_assets_test.jar"

    // Each test gets its own unpack directory. Sharing one meant a test that unpacked successfully
    // left data behind that later tests were served from, so the ones asserting failures passed
    // for the wrong reason.
    private val dataRoot = "./server_jar_assets_test_data"

    @AfterTest
    fun cleanup() {
        platform.posix.remove(path)
        removeUnpacked(dataRoot)
    }

    private fun removeUnpacked(root: String) {
        for (name in listOf("test.json", "other.json")) platform.posix.remove("$root/worldgen/noise/$name")
        platform.posix.remove("$root/worldgen/noise")
        platform.posix.remove("$root/worldgen")
        platform.posix.remove("$root/.index")
        platform.posix.remove("$root/.unpacked")
        platform.posix.remove(root)
    }

    private fun assets(jarPath: String = path): ServerJarAssets =
        ServerJarAssets("1.0", dataRoot, silentLogger()) { jarPath }

    private fun silentLogger() =
        Logger("ServerJarAssetsTest").apply { minLevel = org.basaltmc.common.logging.LogLevel.ERROR }

    private fun withFixture(): ServerJarAssets {
        FileIO.writeBytes(path, bundlerBytes)
        return assets()
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
    fun deletesTheArchiveOnceItIsUnpacked() {
        withFixture()
        assertNull(FileIO.readBytes(path), "the archive should be gone once its data is on disk")
    }

    @Test
    fun stripsTheVanillaNamespaceFromTheOnDiskLayout() {
        withFixture()
        // Callers still use the full vanilla entry name, but the file itself does not repeat
        // `data/minecraft/` under the version directory.
        assertNotNull(FileIO.readBytes("$dataRoot/worldgen/noise/test.json"))
        assertNull(FileIO.readBytes("$dataRoot/data/minecraft/worldgen/noise/test.json"))
    }

    @Test
    fun neverConsultsTheSourceOnceUnpacked() {
        withFixture()

        var asked = false
        val reopened = ServerJarAssets("1.0", dataRoot, silentLogger()) { asked = true; path }

        assertEquals(false, asked, "a warm install must not resolve or download anything")
        assertEquals(
            """{"firstOctave": -9, "amplitudes": [1.0, 2.0]}""",
            reopened.readText("data/minecraft/worldgen/noise/test.json"),
        )
        assertEquals(2, reopened.entriesUnder("data/minecraft/worldgen/noise/").size)
    }

    @Test
    fun unpacksAgainWhenTheCacheIsForAnotherVersion() {
        withFixture()
        FileIO.writeText("$dataRoot/.unpacked", "0.9")
        FileIO.writeBytes(path, bundlerBytes)

        // A stale cache from another version must not be served: the archive is read again.
        assertEquals(
            """{"firstOctave": -9, "amplitudes": [1.0, 2.0]}""",
            assets().readText("data/minecraft/worldgen/noise/test.json"),
        )
    }

    @Test
    fun throwsWhenTheDownloadIsMissing() {
        assertFailsWith<AssetException> {
            ServerJarAssets("1.0", "./sja_missing_test_data", silentLogger()) { "./does_not_exist.jar" }
        }
    }

    @Test
    fun throwsWhenTheJarIsNotActuallyABundler() {
        // A plain (non-bundler) zip with no `META-INF/versions/...` entry at all.
        val plainZipPath = "./server_jar_assets_test_plain.jar"
        FileIO.writeBytes(plainZipPath, PLAIN_ZIP_FIXTURE_BASE64.decodeBase64())
        try {
            assertFailsWith<AssetException> {
                ServerJarAssets("1.0", "./sja_plain_test_data", silentLogger()) { plainZipPath }
            }
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
