package org.basaltmc.server

import org.basaltmc.auth.Cfb8Cipher
import org.basaltmc.auth.OnlineMode
import org.basaltmc.common.logging.LogLevel
import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpConnection
import org.basaltmc.io.Zlib
import org.basaltmc.io.sleepMillis
import org.basaltmc.protocol.PacketFraming
import org.basaltmc.protocol.play.PlayKeepAlivePacket
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeVarInt
import org.basaltmc.server.command.testGroups
import org.basaltmc.server.config.AnimationsConfig
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.config.ColorMode
import org.basaltmc.server.config.CommandsConfig
import org.basaltmc.server.config.ConfigSnapshot
import org.basaltmc.server.config.LiveConfigs
import org.basaltmc.server.config.WorldDeclaration
import org.basaltmc.server.config.WorldDimension
import org.basaltmc.server.config.WorldSettings
import org.basaltmc.server.config.WorldType
import org.basaltmc.server.config.WorldsConfig
import org.basaltmc.server.config.testServerList
import org.basaltmc.server.ui.UiConfigs
import kotlin.concurrent.AtomicInt
import kotlin.time.TimeSource

/**
 * Starts a real [BasaltServer] on its own [NativeThread] and returns the port it ended up on:
 * callers must use that one, since a bind failure moves to the next of [candidatePorts].
 *
 * Ports stay away from 25565, so a server the developer is running cannot answer instead, and
 * below 32768, under Linux's ephemeral range, where a loopback `connect` can pick the same port as
 * its own source and succeed against itself before anything is listening.
 *
 * The world name is scoped to the test, since the server really writes chunks, and left behind
 * afterwards like `AssetCacheTest`'s scratch directory.
 */
internal fun startTestServer(
    preferredPort: Int,
    onlineMode: OnlineMode? = null,
): Int {
    for (port in candidatePorts(preferredPort)) {
        val config =
            BasaltConfig(
                port = port,
                maxPlayers = 20,
                onlineMode = onlineMode != null,
                gamemode = "survival",
                logLevel = LogLevel.INFO,
                logToFile = false,
                logKeepDays = 0,
                logColor = ColorMode.NEVER,
                generationMetrics = false,
            )
        // Everything a test server writes goes under build/: it is already gitignored, `clean`
        // wipes it, and writing into the module directory is how test worlds and databases ended up
        // committed to the repository once already.
        val playersDatabase = "$TEST_DATA_ROOT/players_$port.db"
        for (suffix in listOf("", "-wal", "-shm")) platform.posix.remove("$playersDatabase$suffix")
        // One world per port. Every server here owns its own RegionStore, and two of them reading
        // and writing the same region files at once is a genuine race: a worker that reads a
        // half-written region dies, and the startup that was waiting on its chunks never finishes
        // binding. A view distance of 3 keeps the chunk count each test has to read bounded.
        val worldsConfig =
            WorldsConfig(
                worlds =
                    listOf(
                        WorldDeclaration(
                            "test_world_$port",
                            WorldDimension.OVERWORLD,
                            WorldType.NORMAL,
                            seed = 0L,
                            isDefault = true,
                        ),
                    ),
                defaults = WorldSettings(difficulty = "easy", viewDistance = 3, saveIntervalSeconds = 30),
                worldsRoot = "$TEST_DATA_ROOT/worlds",
            )
        val startupFailed = AtomicInt(0)
        NativeThread.start {
            try {
                BasaltServer(
                    config,
                    onlineMode,
                    worldsConfig = worldsConfig,
                    live =
                        LiveConfigs(
                            ConfigSnapshot(
                                groups = testGroups(),
                                serverList = testServerList(),
                                commands = CommandsConfig(emptyMap(), emptyList(), emptyMap()),
                                animations = AnimationsConfig(emptyMap()),
                                ui = UiConfigs(),
                            ),
                        ),
                    playersDatabasePath = playersDatabase,
                    worldsRoot = "$TEST_DATA_ROOT/worlds",
                ).start()
            } catch (_: Exception) {
                // A bind that cannot succeed, so waiting out the deadline would only stall the run.
                startupFailed.value = 1
            }
        }
        // Long, because generating and saving a world before the first bind takes upwards of half a
        // minute on a two-core Windows runner. Waiting is free; giving up early is not, since every
        // retry generates another world and steals the cores the first one still needs.
        val deadline = TimeSource.Monotonic.markNow()
        while (deadline.elapsedNow().inWholeSeconds < 180) {
            if (startupFailed.value != 0) break
            try {
                TcpConnection.connect("127.0.0.1", port).close()
                return port
            } catch (_: Exception) {
                // Not up yet. Waiting between attempts rather than spinning: this used to hammer
                // connect() as fast as one core allowed, which on a two-core CI runner starved the
                // very server it was waiting for and made the wait a self-fulfilling failure.
                sleepMillis(50)
            }
        }
    }
    // Loudly, rather than leaving the test to fail later on a connect() nobody expects to be the
    // startup timing out.
    error("no test server came up on any of ${candidatePorts(preferredPort)}")
}

/** The asked-for port and two fallbacks, for a host that refuses to bind the first one at all. */
private fun candidatePorts(preferred: Int): List<Int> = listOf(preferred, preferred + 1000, preferred + 2000)

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

/**
 * The next frame that carries something the caller asked about, skipping keep-alives.
 *
 * The server sends those on its own timer, from each connection's own outbound thread, so they land
 * between any two other packets and a real client simply handles them whenever they arrive. A test
 * that asserted a strict global order therefore passed or failed on how fast the machine was.
 */
internal fun readFrame(
    connection: TcpConnection,
    incoming: ByteBuffer,
    compressionEnabled: Boolean,
): ByteBuffer {
    val chunk = ByteArray(4096)
    while (true) {
        PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)?.let { frame ->
            incoming.discardReadBytes()
            if (!isKeepAlive(frame)) return ByteBuffer(frame.size).also { it.writeBytes(frame) }
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

private fun isKeepAlive(frame: ByteArray): Boolean =
    ByteBuffer(frame.size).also { it.writeBytes(frame) }.readVarInt() == PlayKeepAlivePacket.CLIENTBOUND_ID

/** Where every test artefact goes: inside build/, never in the module directory. */
internal const val TEST_DATA_ROOT: String = "build/test-data"
