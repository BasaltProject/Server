package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.command.testGroups
import org.basaltmc.server.runtime.config.AnimationsConfig
import org.basaltmc.server.runtime.config.BasaltConfig
import org.basaltmc.server.runtime.config.ColorMode
import org.basaltmc.server.runtime.config.CommandsConfig
import org.basaltmc.server.runtime.config.ConfigSnapshot
import org.basaltmc.server.runtime.config.LiveConfigs
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.config.WorldsConfig
import org.basaltmc.server.runtime.config.testServerList
import org.basaltmc.server.runtime.sched.Ownership
import org.basaltmc.server.runtime.ui.UiConfigs
import org.basaltmc.wire.auth.Cfb8Cipher
import org.basaltmc.wire.auth.OnlineMode
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.SocketException
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.io.Zlib
import org.basaltmc.wire.io.sleepMillis
import org.basaltmc.wire.protocol.PacketFraming
import org.basaltmc.wire.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.wire.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.wire.protocol.handshake.ClientIntent
import org.basaltmc.wire.protocol.handshake.HandshakePacket
import org.basaltmc.wire.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.wire.protocol.login.LoginCompressionPacket
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.login.LoginStartPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.PlayKeepAlivePacket
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.concurrent.AtomicInt
import kotlin.concurrent.AtomicLong
import kotlin.concurrent.AtomicReference
import kotlin.time.TimeSource
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

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
    /** What the default group may run, for a test that has to drive the server through a command. */
    defaultPermissions: List<String> = listOf("basalt.command.help", "basalt.command.me"),
    /** A second, empty world, for a test about what crossing between two of them does. */
    withSecondWorld: Boolean = false,
): Int {
    // Ownership is enforced under test even with a single region: an access from the wrong thread
    // works by luck here, and luck is what stops working the day regions are turned on.
    Ownership.strict = true
    for (port in candidatePorts(preferredPort)) {
        val config =
            BasaltConfig(
                port = port,
                maxPlayers = 20,
                onlineMode = onlineMode != null,
                gamemode = "survival",
                whitelist = false,
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
        // The world goes with it, whole. A test world that survives its run carries the blocks that
        // run placed, its clock and its weather into the next one, and every test here asserts on a
        // world that has just been generated: that is how one run leaving dirt behind made the next
        // one refuse to place a block there and fail somewhere else entirely.
        deleteDirectory("$TEST_DATA_ROOT/worlds/test_world_$port")
        deleteDirectory("$TEST_DATA_ROOT/worlds/${secondWorldName(port)}")
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
                    ) +
                        // Void, so the second world costs a directory and no generation at all.
                        if (!withSecondWorld) {
                            emptyList()
                        } else {
                            listOf(
                                WorldDeclaration(
                                    secondWorldName(port),
                                    WorldDimension.OVERWORLD,
                                    WorldType.VOID,
                                    seed = 0L,
                                    isDefault = false,
                                ),
                            )
                        },
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
                                groups = testGroups(defaultPermissions),
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
    /**
     * Clock updates go out once a second to everyone, so they land in the middle of whatever else a
     * test is reading. Skipped for the same reason keep-alives are; the join sequence turns this off
     * where the clock is the packet under test.
     */
    skipClockUpdates: Boolean = true,
): ByteBuffer {
    val chunk = ByteArray(4096)
    ReadStallWatchdog.enter(connection)
    try {
        while (true) {
            PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)?.let { frame ->
                incoming.discardReadBytes()
                if (!isPeriodic(frame, skipClockUpdates)) {
                    return ByteBuffer(frame.size).also { it.writeBytes(frame) }
                }
            }
            incoming.writeBytes(chunk, length = readOrStall(connection, chunk))
        }
    } finally {
        ReadStallWatchdog.leave()
    }
}

/**
 * One read, with the watchdog's verdict attached. Closing a socket out from under a blocked `recv`
 * surfaces as a `SocketException` on a bad descriptor rather than as end of stream, so both endings
 * have to be translated or the real reason is lost behind an errno.
 */
private fun readOrStall(
    connection: TcpConnection,
    chunk: ByteArray,
): Int {
    val read =
        try {
            connection.read(chunk)
        } catch (e: SocketException) {
            if (ReadStallWatchdog.fired.value == 1) error(STALL_MESSAGE) else throw e
        }
    check(read > 0) {
        if (ReadStallWatchdog.fired.value == 1) STALL_MESSAGE else "connection closed while waiting for a frame"
    }
    return read
}

private const val STALL_MESSAGE = "no frame arrived within the read deadline, so the watchdog closed the socket"

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
    ReadStallWatchdog.enter(connection)
    try {
        while (true) {
            PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)?.let { frame ->
                incoming.discardReadBytes()
                return ByteBuffer(frame.size).also { it.writeBytes(frame) }
            }
            val n = readOrStall(connection, chunk)
            incoming.writeBytes(cipher.update(chunk, length = n, encrypting = false))
        }
    } finally {
        ReadStallWatchdog.leave()
    }
}

private fun isPeriodic(
    frame: ByteArray,
    skipClockUpdates: Boolean,
): Boolean {
    val id = ByteBuffer(frame.size).also { it.writeBytes(frame) }.readVarInt()
    return id == PlayKeepAlivePacket.CLIENTBOUND_ID || (skipClockUpdates && id == SetTimePacket.ID)
}

/** The name [startTestServer] gives its optional second world. */
internal fun secondWorldName(port: Int): String = "test_void_$port"

/** Reads and discards frames until one with this packet id arrives, returning its body past the id. */
internal fun drainUntil(
    connection: TcpConnection,
    incoming: ByteBuffer,
    packetId: Int,
    limit: Int = 400,
): ByteBuffer {
    var seen = 0
    repeat(limit) {
        val body = readFrame(connection, incoming, compressionEnabled = true)
        seen++
        if (body.readVarInt() == packetId) return body
    }
    error("packet 0x${packetId.toString(16)} never arrived after $seen other frames")
}

/** What a test knows about the player it just drove into Play. */
internal class LoggedIn(
    val connection: TcpConnection,
    val incoming: ByteBuffer,
    val threshold: Int,
    val entityId: Int,
    val name: String,
)

/**
 * The Login and Configuration handshake, stripped to what reaching Play needs.
 *
 * [PlayFlowIntegrationTest] asserts the whole sequence in order, which is its point; a test about
 * something else only needs to arrive, so this dispatches by packet id and ignores the rest.
 */
@OptIn(ExperimentalUuidApi::class)
internal fun loginToPlay(
    connection: TcpConnection,
    incoming: ByteBuffer,
    name: String,
    uuid: Uuid = Uuid.fromLongs(0L, 0L),
): LoggedIn {
    sendUncompressed(connection, HandshakePacket.ID) {
        HandshakePacket(776, "127.0.0.1", 0, ClientIntent.LOGIN).write(it)
    }
    sendUncompressed(connection, LoginStartPacket.ID) { LoginStartPacket(name, uuid).write(it) }

    val compressionBody = readFrame(connection, incoming, compressionEnabled = false)
    check(compressionBody.readVarInt() == LoginCompressionPacket.ID)
    val threshold = LoginCompressionPacket.read(compressionBody).threshold

    check(readFrame(connection, incoming, compressionEnabled = true).readVarInt() == LoginFinishedPacket.ID)
    sendCompressed(connection, LoginAcknowledgedPacket.ID, threshold) { }

    drainUntil(connection, incoming, SelectKnownPacksPacket.CLIENTBOUND_ID)
    sendCompressed(connection, SelectKnownPacksPacket.SERVERBOUND_ID, threshold) {
        SelectKnownPacksPacket(emptyList()).write(it)
    }

    drainUntil(connection, incoming, FinishConfigurationPacket.ID)
    sendCompressed(connection, FinishConfigurationPacket.ID, threshold) { }

    val loginPlay = LoginPlayPacket.read(drainUntil(connection, incoming, LoginPlayPacket.ID))
    return LoggedIn(connection, incoming, threshold, loginPlay.entityId, name)
}

/** Removes a directory and everything under it, so a test world cannot outlive its run. */
private fun deleteDirectory(path: String) {
    if (!FileIO.directoryExists(path)) return
    for (entry in FileIO.list(path)) {
        val child = "$path/$entry"
        if (FileIO.directoryExists(child)) deleteDirectory(child) else FileIO.delete(child)
    }
    platform.posix.rmdir(path)
}

/** Where every test artefact goes: inside build/, never in the module directory. */
internal const val TEST_DATA_ROOT: String = "build/test-data"

/**
 * Closes whichever socket a [readFrame] has been parked on for [deadlineMillis], so a
 * packet that will never arrive fails the test with a message instead of parking the whole run on
 * `recv`. It has cost hours: a blocked read looks exactly like a slow one.
 *
 * One thread for the process, not one per read: tests run sequentially here, so at most one read
 * is ever in flight. A socket timeout would be the real fix, but it belongs in `wire-io` and this
 * only has to stop a test from hanging.
 */
internal object ReadStallWatchdog {
    private val started = AtomicInt(0)
    private val clock = TimeSource.Monotonic.markNow()
    private val blockedOn = AtomicReference<TcpConnection?>(null)
    private val blockedSince = AtomicLong(0)

    /** Set when the watchdog has closed a socket, so the read that fails can say why it failed. */
    val fired: AtomicInt = AtomicInt(0)

    fun enter(connection: TcpConnection) {
        if (started.compareAndSet(0, 1)) start()
        blockedSince.value = clock.elapsedNow().inWholeMilliseconds
        blockedOn.value = connection
    }

    fun leave() {
        blockedOn.value = null
    }

    private fun start() {
        NativeThread.start {
            while (true) {
                sleepMillis(POLL_MILLIS)
                val connection = blockedOn.value ?: continue
                if (clock.elapsedNow().inWholeMilliseconds - blockedSince.value < deadlineMillis) continue
                fired.value = 1
                blockedOn.value = null
                runCatching { connection.close() }
            }
        }
    }

    /** A var only so the watchdog's own test can reach the deadline in a second instead of a minute. */
    var deadlineMillis: Long = 60_000L

    private const val POLL_MILLIS = 250L
}
