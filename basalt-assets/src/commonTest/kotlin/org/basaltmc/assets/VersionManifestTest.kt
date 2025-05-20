package org.basaltmc.assets

import io.ktor.client.HttpClient
import io.ktor.client.engine.mock.MockEngine
import io.ktor.client.engine.mock.respond
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpStatusCode
import io.ktor.http.headersOf
import io.ktor.utils.io.ByteReadChannel
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class VersionManifestTest {
    private val manifestJson =
        """
        {
            "versions": [
                { "id": "26.2", "url": "https://example.invalid/26.2.json" },
                { "id": "26.1", "url": "https://example.invalid/26.1.json" }
            ]
        }
        """.trimIndent()

    private val versionMetadataJson =
        """
        {
            "downloads": {
                "client": { "url": "https://example.invalid/client.jar", "sha1": "aaaa", "size": 111 },
                "server": { "url": "https://example.invalid/server.jar", "sha1": "bbbb", "size": 222 }
            }
        }
        """.trimIndent()

    private fun mockClient(): HttpClient {
        val engine =
            MockEngine { request ->
                val body =
                    when (request.url.toString()) {
                        "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json" -> manifestJson
                        "https://example.invalid/26.2.json" -> versionMetadataJson
                        else -> error("unexpected request to ${request.url}")
                    }
                respond(
                    content = ByteReadChannel(body),
                    status = HttpStatusCode.OK,
                    headers = headersOf(HttpHeaders.ContentType, "application/json"),
                )
            }
        return HttpClient(engine)
    }

    @Test
    fun resolvesAKnownVersionsDownloadArtifacts() {
        val metadata = VersionManifest(mockClient()).resolve("26.2")

        assertEquals("26.2", metadata.id)
        assertEquals(DownloadArtifact("https://example.invalid/client.jar", "aaaa", 111), metadata.client)
        assertEquals(DownloadArtifact("https://example.invalid/server.jar", "bbbb", 222), metadata.server)
    }

    @Test
    fun throwsForAnUnknownVersion() {
        assertFailsWith<AssetException> { VersionManifest(mockClient()).resolve("not-a-real-version") }
    }
}
