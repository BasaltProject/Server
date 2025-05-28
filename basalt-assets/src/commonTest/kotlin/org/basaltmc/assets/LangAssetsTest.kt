package org.basaltmc.assets

import org.basaltmc.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class LangAssetsTest {
    private val fakeClientJarPath = "./basalt_lang_assets_test_client.jar"

    // A real ZIP built with Python's `zipfile` module: `assets/minecraft/lang/en_us.json` with one entry.
    private val fixtureBytes = LANG_FIXTURE_BASE64.decodeBase64()

    @AfterTest
    fun cleanup() {
        platform.posix.remove(fakeClientJarPath)
    }

    @Test
    fun extractsAndParsesTheDefaultLocaleFromAJar() {
        FileIO.writeBytes(fakeClientJarPath, fixtureBytes)
        val table = LangAssets.load(fakeClientJarPath)
        assertEquals("Stick", table.getOrDefault("item.minecraft.stick", "?"))
    }

    @Test
    fun missingClientJarThrows() {
        assertFailsWith<AssetException> { LangAssets.load("./does_not_exist.jar") }
    }

    @Test
    fun theCachedLoaderExtractsFromTheJarOnceAndReadsTheExtractedFileAfterwards() {
        val versionDir = "./basalt_lang_assets_test_version"
        FileIO.createDirectories(versionDir)
        FileIO.writeBytes("$versionDir/client.jar", fixtureBytes)

        assertEquals("Stick", LangAssets.loadCachedOrEmpty(versionDir).getOrDefault("item.minecraft.stick", "?"))

        // The jar is now irrelevant: the extracted copy is what gets read.
        platform.posix.remove("$versionDir/client.jar")
        assertEquals("Stick", LangAssets.loadCachedOrEmpty(versionDir).getOrDefault("item.minecraft.stick", "?"))

        platform.posix.remove("$versionDir/lang/en_us.json")
        platform.posix.rmdir("$versionDir/lang")
        platform.posix.rmdir(versionDir)
    }

    @Test
    fun theCachedLoaderFallsBackToAnEmptyTableWithNothingCached() {
        val table = LangAssets.loadCachedOrEmpty("./basalt_lang_assets_test_missing")
        assertEquals("fallback", table.getOrDefault("item.minecraft.stick", "fallback"))
    }

    @Test
    fun missingLocaleEntryThrows() {
        FileIO.writeBytes(fakeClientJarPath, fixtureBytes)
        assertFailsWith<AssetException> { LangAssets.load(fakeClientJarPath, locale = "xx_xx") }
    }
}

private const val LANG_FIXTURE_BASE64 =
    "UEsDBBQAAAAAAC1tB11GWCAoIQAAACEAAAAgAAAAYXNzZXRzL21pbmVjcmFmdC9sYW5nL2VuX3Vz" +
        "Lmpzb257Iml0ZW0ubWluZWNyYWZ0LnN0aWNrIjogIlN0aWNrIn1QSwECFAMUAAAAAAAtbQddRlgg" +
        "KCEAAAAhAAAAIAAAAAAAAAAAAAAAgAEAAAAAYXNzZXRzL21pbmVjcmFmdC9sYW5nL2VuX3VzLmpz" +
        "b25QSwUGAAAAAAEAAQBOAAAAXwAAAAAA"

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
