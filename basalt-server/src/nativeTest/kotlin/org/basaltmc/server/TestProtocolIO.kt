package org.basaltmc.server

import org.basaltmc.auth.Cfb8Cipher
import org.basaltmc.auth.OnlineMode
import org.basaltmc.common.logging.LogLevel
import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpConnection
import org.basaltmc.io.Zlib
import org.basaltmc.protocol.PacketFraming
import org.basaltmc.protocol.writeVarInt
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.config.ColorMode
import kotlin.time.TimeSource

/**
 * Starts a real [BasaltServer] on its own [NativeThread] and waits for it to accept connections.
 * Retries the whole attempt on a bind failure: CI runners have shown occasional transient ones,
 * and a bind failure on the spawned thread no longer crashes the process (see [NativeThread]'s
 * KDoc), so a retry is safe.
 *
 * Ports live well outside Minecraft's own 25565-range on purpose. Fixed ports in that range
 * collide with any real server the developer happens to have running, and the failure is deeply
 * confusing: the test reaches that server instead of its own and fails on a protocol assertion
 * that has nothing to do with the change under test.
 *
 * [worldName] is a distinctly-scoped scratch name, not `"world"`: [BasaltServer] now really
 * generates and saves chunks (`NoiseWorldGenerator`/`RegionStore`), a real relative path a
 * production run could plausibly also use, so tests need their own name to never collide with (or
 * be mistaken for) an actual world directory. Not cleaned up automatically, same as
 * `AssetCacheTest`'s own scratch directory: harmless small files, left for local inspection.
 */
internal fun startTestServer(
    port: Int,
    onlineMode: OnlineMode? = null,
) {
    repeat(3) { attempt ->
        val config =
            BasaltConfig(
                port = port,
                motd = "A Basalt Server",
                maxPlayers = 20,
                onlineMode = onlineMode != null,
                // One world per port. Every server here owns its own RegionStore, and two of them
                // reading and writing the same region files at once is a genuine race: a worker
                // that reads a half-written region dies, and the startup that was waiting on its
                // chunks never finishes binding.
                worldName = "basalt_server_test_world_$port",
                viewDistance = 3,
                saveIntervalSeconds = 30,
                logLevel = LogLevel.INFO,
                logToFile = false,
                logKeepDays = 0,
                logColor = ColorMode.NEVER,
                generationMetrics = false,
            )
        NativeThread.start { BasaltServer(config, onlineMode).start() }
        // Generous, not tight: BasaltServer now generates and saves real chunks before it starts
        // listening, several real seconds on a cold asset cache, not the near-instant startup this
        // deadline was tuned for back when world generation was a fixed in-memory flat world.
        val deadline = TimeSource.Monotonic.markNow()
        while (deadline.elapsedNow().inWholeMilliseconds < 20000) {
            try {
                TcpConnection.connect("127.0.0.1", port).close()
                return
            } catch (_: Exception) {
                // not up yet, keep trying
            }
        }
        if (attempt < 2) {
            val backoff = TimeSource.Monotonic.markNow()
            while (backoff.elapsedNow().inWholeMilliseconds < 200) {
                // brief pause before retrying the bind
            }
        }
    }
}

/** Plain packet I/O helpers shared by the integration tests that drive [BasaltServer] over a real socket. */
internal fun sendUncompressed(
    connection: TcpConnection,
    id: Int,
    writeBody: (ByteBuffer) -> Unit = {},
) {
    val body = ByteBuffer()
    body.writeVarInt(id)
    writeBody(body)
    val wire = ByteBuffer()
    PacketFraming.writeUncompressed(wire, body.toByteArray())
    connection.write(wire.toByteArray())
}

internal fun sendCompressed(
    connection: TcpConnection,
    id: Int,
    threshold: Int,
    writeBody: (ByteBuffer) -> Unit,
) {
    val body = ByteBuffer()
    body.writeVarInt(id)
    writeBody(body)
    val wire = ByteBuffer()
    PacketFraming.writeCompressed(wire, body.toByteArray(), threshold, Zlib::compress)
    connection.write(wire.toByteArray())
}

internal fun readFrame(
    connection: TcpConnection,
    incoming: ByteBuffer,
    compressionEnabled: Boolean,
): ByteBuffer {
    val chunk = ByteArray(4096)
    while (true) {
        PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)?.let { frame ->
            incoming.discardReadBytes()
            return ByteBuffer(frame.size).also { it.writeBytes(frame) }
        }
        val n = connection.read(chunk)
        check(n > 0) { "connection closed while waiting for a frame" }
        incoming.writeBytes(chunk, length = n)
    }
}

internal fun sendUncompressedEncrypted(
    connection: TcpConnection,
    id: Int,
    cipher: Cfb8Cipher,
    writeBody: (ByteBuffer) -> Unit = {},
) {
    val body = ByteBuffer()
    body.writeVarInt(id)
    writeBody(body)
    val wire = ByteBuffer()
    PacketFraming.writeUncompressed(wire, body.toByteArray())
    connection.write(cipher.update(wire.toByteArray(), encrypting = true))
}

internal fun sendCompressedEncrypted(
    connection: TcpConnection,
    id: Int,
    threshold: Int,
    cipher: Cfb8Cipher,
    writeBody: (ByteBuffer) -> Unit,
) {
    val body = ByteBuffer()
    body.writeVarInt(id)
    writeBody(body)
    val wire = ByteBuffer()
    PacketFraming.writeCompressed(wire, body.toByteArray(), threshold, Zlib::compress)
    connection.write(cipher.update(wire.toByteArray(), encrypting = true))
}

internal fun readFrameEncrypted(
    connection: TcpConnection,
    incoming: ByteBuffer,
    compressionEnabled: Boolean,
    cipher: Cfb8Cipher,
): ByteBuffer {
    val chunk = ByteArray(4096)
    while (true) {
        PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)?.let { frame ->
            incoming.discardReadBytes()
            return ByteBuffer(frame.size).also { it.writeBytes(frame) }
        }
        val n = connection.read(chunk)
        check(n > 0) { "connection closed while waiting for a frame" }
        incoming.writeBytes(cipher.update(chunk, length = n, encrypting = false))
    }
}
