package org.basaltmc.assets

import org.basaltmc.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class LangAssetsTest {
    private val fakeClientJarPath = "build/test-data/lang_client.jar"

    // A real ZIP built with Python's `zipfile` module: `assets/minecraft/lang/en_us.json` with one entry.
    private val fixtureBytes = LANG_FIXTURE_BASE64.decodeBase64()

    @AfterTest
    fun cleanup() {
        platform.posix.remove(fakeClientJarPath)
    }

    @Test
    fun extractsAndParsesTheDefaultLocaleFromAJar() {
        FileIO.createDirectories(fakeClientJarPath.substringBeforeLast('/'))
        FileIO.writeBytes(fakeClientJarPath, fixtureBytes)
        val table = LangAssets.load(fakeClientJarPath)
        assertEquals("Stick", table.getOrDefault("item.minecraft.stick", "?"))
    }

    @Test
    fun missingClientJarThrows() {
        assertFailsWith<AssetException> { LangAssets.load("./does_not_exist.jar") }
    }

    @Test
    fun theCachedLoaderReadsWhatTheServerJarUnpackLeftBehind() {
        val versionDir = "build/test-data/lang_version"
        val langDir = "$versionDir/assets/minecraft/lang"
        FileIO.createDirectories(langDir)
        FileIO.writeText("$langDir/en_us.json", "{\"item.minecraft.stick\": \"Stick\"}")

        assertEquals("Stick", LangAssets.loadCachedOrEmpty(versionDir).getOrDefault("item.minecraft.stick", "?"))

        platform.posix.remove("$langDir/en_us.json")
        platform.posix.rmdir(langDir)
        platform.posix.rmdir("$versionDir/assets/minecraft")
        platform.posix.rmdir("$versionDir/assets")
        platform.posix.rmdir(versionDir)
    }

    @Test
    fun theCachedLoaderFallsBackToAnEmptyTableWithNothingUnpacked() {
        val table = LangAssets.loadCachedOrEmpty("build/test-data/lang_missing")
        assertEquals("fallback", table.getOrDefault("item.minecraft.stick", "fallback"))
    }

    @Test
    fun missingLocaleEntryThrows() {
        FileIO.createDirectories(fakeClientJarPath.substringBeforeLast('/'))
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
