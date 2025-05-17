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
import kotlin.time.TimeSource

/**
 * Starts a real [BasaltServer] on its own [NativeThread] and waits for it to accept connections.
 * Retries the whole attempt on a bind failure: CI runners have shown occasional transient ones,
 * and a bind failure on the spawned thread no longer crashes the process (see [NativeThread]'s
 * KDoc), so a retry is safe.
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
                worldName = "world",
                logLevel = LogLevel.INFO,
            )
        NativeThread.start { BasaltServer(config, onlineMode).start() }
        val deadline = TimeSource.Monotonic.markNow()
        while (deadline.elapsedNow().inWholeMilliseconds < 500) {
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
