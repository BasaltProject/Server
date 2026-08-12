package org.basaltmc.server.assets

import io.ktor.client.HttpClient
import io.ktor.client.engine.mock.MockEngine
import io.ktor.client.engine.mock.respond
import io.ktor.http.HttpStatusCode
import io.ktor.utils.io.ByteReadChannel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class AssetCacheTest {
    private val cacheRoot = "build/test-data/asset_cache"

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
        platform.posix.remove("$cacheRoot/26.2/client.jar.verified")
        platform.posix.remove("$cacheRoot/26.2")
        platform.posix.remove(cacheRoot)
    }

    /** What `ensure` writes after a verified download, so "already cached" tests start from a real cache. */
    private fun markVerified(
        fileName: String = "client.jar",
        sha1: String = validSha1,
        size: Int = validBytes.size,
    ) {
        FileIO.writeText("$cacheRoot/26.2/$fileName.verified", "$sha1 $size")
    }

    private fun mockClient(bytes: ByteArray = validBytes): HttpClient =
        HttpClient(MockEngine { respond(content = ByteReadChannel(bytes), status = HttpStatusCode.OK) })

    private fun silentLogger() =
        Logger("AssetCacheTest").apply { minLevel = org.basaltmc.wire.common.logging.LogLevel.ERROR }

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
        markVerified()

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
    fun redownloadsWhenTheCachedFileWasNeverVerified() {
        FileIO.createDirectories("$cacheRoot/26.2")
        FileIO.writeBytes("$cacheRoot/26.2/client.jar", "corrupted".encodeToByteArray())

        val cache = AssetCache(mockClient(), cacheRoot, silentLogger())
        cache.ensure(metadata, AssetCache.Artifact.CLIENT)

        assertTrue(FileIO.readBytes("$cacheRoot/26.2/client.jar").contentEquals(validBytes))
    }

    /** A truncated or half-written download keeps its sidecar but no longer matches the recorded size. */
    @Test
    fun redownloadsWhenTheCachedFileNoLongerMatchesItsRecordedSize() {
        FileIO.createDirectories("$cacheRoot/26.2")
        FileIO.writeBytes("$cacheRoot/26.2/client.jar", validBytes.copyOf(4))
        markVerified()

        val cache = AssetCache(mockClient(), cacheRoot, silentLogger())
        cache.ensure(metadata, AssetCache.Artifact.CLIENT)

        assertTrue(FileIO.readBytes("$cacheRoot/26.2/client.jar").contentEquals(validBytes))
    }

    @Test
    fun redownloadsWhenTheSidecarRecordsADifferentVersionsHash() {
        FileIO.createDirectories("$cacheRoot/26.2")
        FileIO.writeBytes("$cacheRoot/26.2/client.jar", validBytes)
        markVerified(sha1 = "0000000000000000000000000000000000000000")

        val cache = AssetCache(mockClient(), cacheRoot, silentLogger())
        cache.ensure(metadata, AssetCache.Artifact.CLIENT)

        assertEquals("$validSha1 ${validBytes.size}", FileIO.readText("$cacheRoot/26.2/client.jar.verified")?.trim())
    }

    @Test
    fun recordsTheVerifiedHashAndSizeAfterDownloading() {
        val cache = AssetCache(mockClient(), cacheRoot, silentLogger())
        cache.ensure(metadata, AssetCache.Artifact.CLIENT)

        assertEquals("$validSha1 ${validBytes.size}", FileIO.readText("$cacheRoot/26.2/client.jar.verified")?.trim())
    }

    @Test
    fun throwsWhenTheDownloadedBytesDoNotMatchTheExpectedSha1() {
        val cache = AssetCache(mockClient("wrong bytes".encodeToByteArray()), cacheRoot, silentLogger())
        assertFailsWith<AssetException> { cache.ensure(metadata, AssetCache.Artifact.CLIENT) }
    }
}
