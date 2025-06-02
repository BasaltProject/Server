package org.basaltmc.assets

import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.DelicateCryptographyApi
import dev.whyoleg.cryptography.algorithms.SHA1
import io.ktor.client.HttpClient
import io.ktor.client.call.body
import io.ktor.client.request.get
import kotlinx.coroutines.runBlocking
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO

/**
 * Local, versioned cache for official Mojang artifacts: `./.basalt/assets/{version}/`, outside
 * the repo tree and gitignored. Never redistributed, never committed: this class only ever writes
 * into that cache directory, nothing else in the repo touches it.
 */
public class AssetCache(
    private val client: HttpClient = HttpClient(),
    private val cacheRoot: String = "./.basalt/assets",
    private val logger: Logger = Logger("AssetCache"),
) {
    /** Which of a version's two published artifacts to fetch: server (`data/`) or client (`assets/`, `lang/`). */
    public enum class Artifact(
        internal val fileName: String,
    ) {
        CLIENT("client.jar"),
        SERVER("server.jar"),
    }

    /**
     * The local path to the cached, verified jar, downloading it first if needed.
     *
     * Hashing happens once, at download time, and lands in a `.verified` sidecar: re-hashing tens
     * of megabytes every boot costs startup time and proves nothing new. The size is re-checked
     * because a seek is free and catches the realistic corruption, a truncated download; an
     * in-place edit of the same length is the trade. A missing or mismatched sidecar re-downloads.
     */
    public fun ensure(
        metadata: VersionMetadata,
        artifact: Artifact,
    ): String {
        val download = if (artifact == Artifact.CLIENT) metadata.client else metadata.server
        val versionDir = "$cacheRoot/${metadata.id}"
        val path = "$versionDir/${artifact.fileName}"

        if (FileIO.readText("$path.verified")?.trim() == "${download.sha1} ${download.size}" &&
            FileIO.size(path) == download.size
        ) {
            logger.info { "${metadata.id} assets already downloaded and verified." }
            return path
        }

        logger.info { "Downloading ${metadata.id} assets (${download.size / 1_000_000} MB)..." }
        val bytes = runBlocking { client.get(download.url).body<ByteArray>() }
        val actualSha1 = sha1Hex(bytes)
        if (actualSha1 != download.sha1) {
            throw AssetException(
                "SHA-1 mismatch downloading ${metadata.id} assets: " +
                    "expected ${download.sha1}, got $actualSha1",
            )
        }

        FileIO.createDirectories(versionDir)
        FileIO.writeBytes(path, bytes)
        FileIO.writeText("$path.verified", "$actualSha1 ${bytes.size}")
        logger.info { "${metadata.id} assets downloaded and verified." }
        return path
    }

    @OptIn(DelicateCryptographyApi::class)
    private fun sha1Hex(bytes: ByteArray): String {
        val hasher = CryptographyProvider.Default.get(SHA1).hasher()
        val digest = hasher.hashBlocking(bytes)
        return digest.joinToString("") { (it.toInt() and 0xFF).toString(16).padStart(2, '0') }
    }
}
