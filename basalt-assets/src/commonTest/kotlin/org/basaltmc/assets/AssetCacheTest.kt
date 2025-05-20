package org.basaltmc.assets

import io.ktor.client.HttpClient
import io.ktor.client.engine.mock.MockEngine
import io.ktor.client.engine.mock.respond
import io.ktor.http.HttpStatusCode
import io.ktor.utils.io.ByteReadChannel
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class AssetCacheTest {
    private val cacheRoot = "./basalt_asset_cache_test"

    // sha1("hello world jar bytes") = e275da018ffa54b2b0082d257e7da2e359e36a93
    private val validBytes = "hello world jar bytes".encodeToByteArray()
    private val validSha1 = "e275da018ffa54b2b0082d257e7da2e359e36a93"

    private val metadata =
        VersionMetadata(
            id = "26.2",
            client = DownloadArtifact("https://example.invalid/client.jar", validSha1, validBytes.size.toLong()),
            server = DownloadArtifact("https://example.invalid/server.jar", validSha1, validBytes.size.toLong()),
        )

    @AfterTest
    fun cleanup() {
        platform.posix.remove("$cacheRoot/26.2/client.jar")
        platform.posix.remove("$cacheRoot/26.2")
        platform.posix.remove(cacheRoot)
    }

    private fun mockClient(bytes: ByteArray = validBytes): HttpClient =
        HttpClient(MockEngine { respond(content = ByteReadChannel(bytes), status = HttpStatusCode.OK) })

    private fun silentLogger() =
        Logger("AssetCacheTest").apply { minLevel = org.basaltmc.common.logging.LogLevel.ERROR }

    @Test
    fun downloadsAndCachesAFreshArtifact() {
        val cache = AssetCache(mockClient(), cacheRoot, silentLogger())
        val path = cache.ensure(metadata, AssetCache.Artifact.CLIENT)

        assertEquals("$cacheRoot/26.2/client.jar", path)
        assertTrue(FileIO.readBytes(path).contentEquals(validBytes))
    }

    @Test
    fun skipsRedownloadWhenAlreadyCachedAndValid() {
        FileIO.createDirectories("$cacheRoot/26.2")
        FileIO.writeBytes("$cacheRoot/26.2/client.jar", validBytes)

        var requested = false
        val engine =
            MockEngine {
                requested = true
                respond(content = ByteReadChannel(validBytes), status = HttpStatusCode.OK)
            }
        val cache = AssetCache(HttpClient(engine), cacheRoot, silentLogger())

        cache.ensure(metadata, AssetCache.Artifact.CLIENT)

        assertEquals(false, requested)
    }

    @Test
    fun redownloadsWhenTheCachedFileFailsVerification() {
        FileIO.createDirectories("$cacheRoot/26.2")
        FileIO.writeBytes("$cacheRoot/26.2/client.jar", "corrupted".encodeToByteArray())

        val cache = AssetCache(mockClient(), cacheRoot, silentLogger())
        cache.ensure(metadata, AssetCache.Artifact.CLIENT)

        assertTrue(FileIO.readBytes("$cacheRoot/26.2/client.jar").contentEquals(validBytes))
    }

    @Test
    fun throwsWhenTheDownloadedBytesDoNotMatchTheExpectedSha1() {
        val cache = AssetCache(mockClient("wrong bytes".encodeToByteArray()), cacheRoot, silentLogger())
        assertFailsWith<AssetException> { cache.ensure(metadata, AssetCache.Artifact.CLIENT) }
    }
}
