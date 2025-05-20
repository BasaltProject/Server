package org.basaltmc.assets

import io.ktor.client.HttpClient
import io.ktor.client.request.get
import io.ktor.client.statement.bodyAsText
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive

/** One downloadable artifact from a version's metadata: `downloads.client`/`downloads.server`. */
public data class DownloadArtifact(
    val url: String,
    val sha1: String,
    val size: Long,
)

/** The subset of a version's `{version}.json` metadata this project needs. */
public data class VersionMetadata(
    val id: String,
    val client: DownloadArtifact,
    val server: DownloadArtifact,
)

/**
 * Resolves a Minecraft version id to its [VersionMetadata] via Mojang's public piston-meta API:
 * the version manifest lists every version with a metadata URL, and that per-version metadata
 * lists the real download URLs and SHA-1 hashes. Parses JSON by hand
 * (no `kotlinx-serialization` compiler plugin), matching `MojangSessionService`/`ServerStatusPayload`
 * elsewhere in this project. [client] is injectable so tests can use Ktor's `MockEngine`.
 */
public class VersionManifest(
    private val client: HttpClient = HttpClient(),
) {
    public fun resolve(versionId: String): VersionMetadata =
        runBlocking {
            val manifestBody = client.get(VERSION_MANIFEST_URL).bodyAsText()
            val manifest = Json.parseToJsonElement(manifestBody) as JsonObject
            val versions = manifest["versions"] as JsonArray
            val entry =
                versions
                    .map { it as JsonObject }
                    .firstOrNull { (it["id"] as JsonPrimitive).content == versionId }
                    ?: throw AssetException("Unknown Minecraft version: $versionId")
            val metadataUrl = (entry["url"] as JsonPrimitive).content

            val metadataBody = client.get(metadataUrl).bodyAsText()
            val metadata = Json.parseToJsonElement(metadataBody) as JsonObject
            val downloads = metadata["downloads"] as JsonObject
            VersionMetadata(
                id = versionId,
                client = downloads.parseArtifact("client"),
                server = downloads.parseArtifact("server"),
            )
        }

    private fun JsonObject.parseArtifact(key: String): DownloadArtifact {
        val artifact = this[key] as JsonObject
        return DownloadArtifact(
            url = (artifact["url"] as JsonPrimitive).content,
            sha1 = (artifact["sha1"] as JsonPrimitive).content,
            size = (artifact["size"] as JsonPrimitive).content.toLong(),
        )
    }

    private companion object {
        const val VERSION_MANIFEST_URL = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"
    }
}

public class AssetException(
    message: String,
) : Exception(message)
