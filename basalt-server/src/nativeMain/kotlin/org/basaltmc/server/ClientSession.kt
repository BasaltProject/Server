package org.basaltmc.server

import org.basaltmc.auth.Cfb8Cipher
import org.basaltmc.auth.MinecraftCrypt
import org.basaltmc.auth.OfflineUuid
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionProfileProperty
import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.timings.Timings
import org.basaltmc.common.timings.time
import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.NativeMutex
import org.basaltmc.io.NativeThread
import org.basaltmc.io.SocketException
import org.basaltmc.io.TcpConnection
import org.basaltmc.io.Zlib
import org.basaltmc.io.sleepMillis
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.protocol.PacketFraming
import org.basaltmc.protocol.ProtocolState
import org.basaltmc.protocol.configuration.ClientInformationPacket
import org.basaltmc.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.protocol.configuration.KnownPack
import org.basaltmc.protocol.configuration.RegistryDataPacket
import org.basaltmc.protocol.configuration.RegistryEntry
import org.basaltmc.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.protocol.configuration.TagEntry
import org.basaltmc.protocol.configuration.TagGroup
import org.basaltmc.protocol.configuration.TagsPacket
import org.basaltmc.protocol.handshake.ClientIntent
import org.basaltmc.protocol.handshake.HandshakePacket
import org.basaltmc.protocol.login.GameProfile
import org.basaltmc.protocol.login.GameProfileProperty
import org.basaltmc.protocol.login.HelloPacket
import org.basaltmc.protocol.login.KeyPacket
import org.basaltmc.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.protocol.login.LoginCompressionPacket
import org.basaltmc.protocol.login.LoginFinishedPacket
import org.basaltmc.protocol.login.LoginStartPacket
import org.basaltmc.protocol.play.AcceptTeleportationPacket
import org.basaltmc.protocol.play.BlockPos
import org.basaltmc.protocol.play.ChatCommandPacket
import org.basaltmc.protocol.play.ChatMessagePacket
import org.basaltmc.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.protocol.play.ChunkBatchStartPacket
import org.basaltmc.protocol.play.ChunkBlockEntityInfo
import org.basaltmc.protocol.play.ClientCommandPacket
import org.basaltmc.protocol.play.ClientboundBlockChangedAckPacket
import org.basaltmc.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.protocol.play.ClientboundPlayerAbilitiesPacket
import org.basaltmc.protocol.play.CommandsPacket
import org.basaltmc.protocol.play.ContainerSetContentPacket
import org.basaltmc.protocol.play.ContainerSetSlotPacket
import org.basaltmc.protocol.play.EntityTeleportPacket
import org.basaltmc.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.protocol.play.GameEventPacket
import org.basaltmc.protocol.play.Gamemode
import org.basaltmc.protocol.play.GlobalPos
import org.basaltmc.protocol.play.Heightmap
import org.basaltmc.protocol.play.HeightmapType
import org.basaltmc.protocol.play.LoginPlayPacket
import org.basaltmc.protocol.play.MapChunkPacket
import org.basaltmc.protocol.play.PlayDisconnectPacket
import org.basaltmc.protocol.play.PlayKeepAlivePacket
import org.basaltmc.protocol.play.PlayerCombatKillPacket
import org.basaltmc.protocol.play.PlayerCommandPacket
import org.basaltmc.protocol.play.PlayerInfoEntry
import org.basaltmc.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.protocol.play.PlayerLoadedPacket
import org.basaltmc.protocol.play.PlayerPositionPacket
import org.basaltmc.protocol.play.RemoveEntitiesPacket
import org.basaltmc.protocol.play.RespawnPacket
import org.basaltmc.protocol.play.ServerboundPlayerAbilitiesPacket
import org.basaltmc.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.protocol.play.ServerboundPositionLookPacket
import org.basaltmc.protocol.play.ServerboundPositionPacket
import org.basaltmc.protocol.play.ServerboundSetCarriedItemPacket
import org.basaltmc.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.protocol.play.SetExperiencePacket
import org.basaltmc.protocol.play.SetHealthPacket
import org.basaltmc.protocol.play.SetHeldSlotPacket
import org.basaltmc.protocol.play.SetSimulationDistancePacket
import org.basaltmc.protocol.play.SignedChatCommandPacket
import org.basaltmc.protocol.play.SpawnInfo
import org.basaltmc.protocol.play.SpawnPositionPacket
import org.basaltmc.protocol.play.SystemChatPacket
import org.basaltmc.protocol.play.UpdateLightPacket
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.status.PingRequestPacket
import org.basaltmc.protocol.status.PongResponsePacket
import org.basaltmc.protocol.status.StatusRequestPacket
import org.basaltmc.protocol.status.StatusResponsePacket
import org.basaltmc.protocol.writeVarInt
import org.basaltmc.registry.Blocks
import org.basaltmc.server.command.CommandDispatcher
import org.basaltmc.server.command.PlayerCommandSource
import org.basaltmc.server.placeholder.PlaceholderSlots
import org.basaltmc.server.player.Difficulty
import org.basaltmc.server.player.FoodData
import org.basaltmc.server.player.PlayerPersistence
import org.basaltmc.server.player.PlayerRegistry
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.server.registry.VanillaBiomes
import org.basaltmc.server.registry.VanillaRegistryData
import org.basaltmc.server.registry.VanillaTagData
import org.basaltmc.server.ui.PlayerUi
import org.basaltmc.server.ui.UiConfigs
import org.basaltmc.server.world.ChunkGenerationQueue
import org.basaltmc.server.world.LoadedWorld
import org.basaltmc.server.world.WorldState
import org.basaltmc.storage.PlayerStore
import org.basaltmc.storage.StoredPlayer
import org.basaltmc.storage.StoredStats
import org.basaltmc.text.Component
import org.basaltmc.text.LangTable
import org.basaltmc.text.TextColor
import org.basaltmc.text.resolvePlainText
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkDataCodec
import org.basaltmc.world.ChunkPos
import org.basaltmc.world.ChunkTracker
import kotlin.concurrent.AtomicInt
import kotlin.random.Random
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Drives one client connection through Handshake, (Status | Login), Configuration and Play.
 *
 * Login is offline-mode when [onlineMode] is `null` (no key exchange, [LoginFinishedPacket]
 * follows [LoginStartPacket] directly), and online-mode otherwise: [HelloPacket] and [KeyPacket]
 * negotiate an AES-128/CFB8 shared secret (verified against real `javax.crypto` output, see
 * `Cfb8CipherTest`), after which every packet in both directions, starting with
 * [LoginCompressionPacket] itself, flows through that cipher.
 *
 * Configuration sends an empty known-pack list (Basalt has no datapacks yet), then the
 * `dimension_type`/`worldgen/biome` registries [VanillaRegistryData] hand-authors, then
 * [FinishConfigurationPacket]. Play sends the chunks around spawn first, then streams new ones in
 * (generating them on demand through [world]) and forgets old ones as the player crosses chunk
 * boundaries ([chunkTracker], the same square view area [SetChunkCacheRadiusPacket] already
 * declares, just re-centered instead of fixed at the spawn position), and wires movement/chat
 * through [playerRegistry] so other connected players see them. Breaking a block
 * ([handlePlayerAction]) follows the player's game mode and broadcasts through [WorldState]'s
 * single lock; placing one is acknowledged but not yet acted on, since it needs a held-item
 * inventory model that doesn't exist yet.
 *
 * Play also owns this player's survival state: [tickPlayer] runs hunger and regeneration on this
 * connection's own thread, slash commands go through [commands], and death and respawn are handled
 * here because both are packet sequences, not world state.
 */
@OptIn(ExperimentalUuidApi::class)
public class ClientSession(
    private val connection: TcpConnection,
    /**
     * Answers the server list ping. Takes the handshake so that a profile can be chosen from the
     * client's version, the address it connected to and where it connected from: those three are
     * everything that is known before login.
     */
    private val statusProvider: (protocolVersion: Int, host: String, address: String?) ->
    org.basaltmc.protocol.status.ServerStatusPayload,
    private val compressionThreshold: Int = 256,
    private val onlineMode: OnlineMode? = null,
    private val playerRegistry: PlayerRegistry? = null,
    /** Every loaded world, by name: a joining player goes into the one their stored row names. */
    private val worlds: Map<String, LoadedWorld> = emptyMap(),
    private val defaultWorld: LoadedWorld? = null,
    private val commands: CommandDispatcher? = null,
    private val defaultGamemode: Gamemode = Gamemode.SURVIVAL,
    private val difficulty: Difficulty = Difficulty.EASY,
    private val playerStore: PlayerStore? = null,
    /** Where this connection came from, or null when nothing should be remembered about it. */
    private val rememberAddress: String? = null,
    private val ui: () -> UiConfigs = { UiConfigs() },
    private val placeholders: PlaceholderRegistry = PlaceholderRegistry(),
    /** Where this connection's share of the server's time is recorded. Off unless `/timings` is on. */
    private val timings: Timings = Timings(),
) {
    private val logger = Logger("Session")

    /** What the console renders messages with; empty (so keys print as keys) when no language is loaded. */
    private val consoleLang: LangTable
        get() = commands?.environment?.lang ?: LangTable.EMPTY
    private val sendMutex = NativeMutex()
    private val incoming = ByteBuffer()
    private var state = ProtocolState.HANDSHAKE
    private var lastPacketId = -1

    /** Kept because the status response is built from it, several packets after it arrives. */
    private var handshake: HandshakePacket? = null

    // Read by this connection's PlayerSession, so the same placeholders answer for a logged-in
    // player and for a pending ping.
    internal val handshakeProtocol: Int get() = handshake?.protocolVersion ?: 0
    internal val handshakeHost: String get() = handshake?.hostName.orEmpty()
    internal val remoteAddress: String? get() = connection.remoteAddress

    // Written once per session rather than on every flush: it only changes when someone logs in.
    private var addressRecorded = false

    /**
     * This player's tab list, scoreboard and boss bar. Ticked from the outbound loop below, because
     * everything it does writes to this socket and the shared tick thread must never do that.
     */
    private val playerUi = PlayerUi(ui, placeholders, ::sendPlayPacket)
    private var uiTick = 0L

    // Vanilla's optimistic-locking counter for container changes: bumped on every window write so
    // the client can tell a click made against the current contents from a stale one.
    private var containerStateId = 0

    // Persisted counters, carried across sessions. Play time is counted by the same 20 Hz loop that
    // ticks hunger, so it is ticks played rather than wall-clock time connected.
    private var firstSeenEpochSeconds = 0L
    private var playTimeTicks = 0L
    private var compressionEnabled = false

    private var pendingUsername: String? = null
    private var challenge: ByteArray? = null
    private var decryptCipher: Cfb8Cipher? = null
    private var encryptCipher: Cfb8Cipher? = null

    private var pendingProfile: GameProfile? = null
    private var playerSession: PlayerSession? = null

    /** Resolved in [enterPlay] from the player's stored world, falling back to the default. */
    private var currentWorld: LoadedWorld? = null

    private val world: WorldState get() = requireNotNull(currentWorld).state
    private val generationQueue: ChunkGenerationQueue get() = requireNotNull(currentWorld).generationQueue
    private val worldName: String get() = currentWorld?.name ?: ""

    private val chunkTracker = ChunkTracker()
    private var viewRadius = 0
    private var centerChunkX = 0
    private var centerChunkZ = 0

    // Requested from the generation queue, not yet on the wire. Both this connection's thread and
    // the tick loop drain it, so it is guarded.
    private val chunkStreamMutex = NativeMutex()
    private val pendingChunks = mutableListOf<ChunkPos>()

    @OptIn(ExperimentalTime::class)
    private var lastKeepAlive: TimeSource.Monotonic.ValueTimeMark = TimeSource.Monotonic.markNow()
    private val streaming = AtomicInt(0)

    // Until the client has told us: enough to fill a view distance in a few seconds, far below the
    // rate the server can produce at. Guessing high is what floods a client that then stops
    // draining its socket at all.
    private var desiredChunksPerTick: Float = INITIAL_CHUNKS_PER_TICK
    private var sendBudget: Float = 0f

    /**
     * This connection's own outbound loop, and the only thing that writes to it unprompted.
     *
     * Sockets here block, so a client that stops draining parks whoever writes to it. That has to
     * be a thread belonging to this one connection: doing it from the tick loop stalled saving,
     * chunk unloading and every other player the instant one client fell behind. Doing it from the
     * read loop instead would work only for as long as the client keeps talking, which is not
     * something a server should have to rely on.
     */
    private fun startOutboundLoop() {
        if (streaming.compareAndSet(0, 1)) {
            NativeThread.start {
                while (streaming.value == 1) {
                    runCatching {
                        // A tick's worth of allowance, capped so a client that has been idle
                        // cannot then be handed an enormous burst all at once.
                        sendBudget = minOf(sendBudget + desiredChunksPerTick, MAX_BURST_CHUNKS)
                        val allowed = sendBudget.toInt()
                        if (allowed > 0) {
                            timings.of("player.chunks").time { sendBudget -= flushReadyChunks(allowed).toFloat() }
                        }
                        timings.of("player.tick").time { tickPlayer() }
                        timings.of("player.ui").time { tickUi() }
                        sendKeepAliveIfDue()
                    }
                    sleepMillis(OUTBOUND_INTERVAL_MILLIS)
                }
            }
        }
    }

    public fun run() {
        val readChunk = ByteArray(4096)
        try {
            while (true) {
                val read = connection.read(readChunk)
                if (read <= 0) return
                val cipher = decryptCipher
                if (cipher != null) {
                    incoming.writeBytes(cipher.update(readChunk, length = read, encrypting = false))
                } else {
                    incoming.writeBytes(readChunk, length = read)
                }
                drainFrames()
            }
        } catch (e: SocketException) {
            // A connection dropping is part of this class's normal lifecycle, not a failure worth
            // surfacing: server-list pings in particular reset the socket the instant they have
            // their answer. Letting it escape logged an uncaught-exception line on every ping.
            logger.debug { "Connection closed: ${e.message}" }
        } catch (e: Exception) {
            // One malformed (or misunderstood) packet must cost one connection, not the thread that
            // was reading it. Before this, a decoder that read past the end of a frame escaped as
            // an uncaught exception on a native thread, which killed the read loop and disconnected
            // the player with nothing in the log but a stack-trace line. It is logged loudly on
            // purpose: reaching here is always a bug in a decoder or a hostile client, never
            // routine, and the id in the message is what identifies which.
            logger.error(e) {
                "Dropping ${playerSession?.name ?: "connection"}: " +
                    "failed to handle packet 0x${lastPacketId.toString(16)} in $state"
            }
        } finally {
            streaming.value = 0
            leavePlayIfPresent()
            connection.close()
        }
    }

    private fun drainFrames() {
        while (true) {
            val frame =
                PacketFraming.tryReadFrame(incoming, compressionEnabled, decompress = Zlib::decompress)
                    ?: break
            incoming.discardReadBytes()
            handleFrame(ByteBuffer(frame.size).also { it.writeBytes(frame) })
        }
    }

    private fun handleFrame(body: ByteBuffer) {
        val packetId = body.readVarInt()
        // Remembered, not passed around: the only reader is the failure path in [run], and knowing
        // which packet a decoder died on is the difference between a one-line fix and a hunt.
        lastPacketId = packetId
        timings.of("network.packet").time { handleFrameBody(packetId, body) }
    }

    private fun handleFrameBody(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (state) {
            ProtocolState.HANDSHAKE -> handleHandshake(packetId, body)
            ProtocolState.STATUS -> handleStatus(packetId, body)
            ProtocolState.LOGIN -> handleLogin(packetId, body)
            ProtocolState.CONFIGURATION -> handleConfiguration(packetId, body)
            ProtocolState.PLAY -> handlePlay(packetId, body)
        }
    }

    private fun handleHandshake(
        packetId: Int,
        body: ByteBuffer,
    ) {
        if (packetId != HandshakePacket.ID) return
        val packet = HandshakePacket.read(body)
        handshake = packet
        state =
            when (packet.intent) {
                ClientIntent.STATUS -> ProtocolState.STATUS
                ClientIntent.LOGIN -> ProtocolState.LOGIN
                ClientIntent.TRANSFER -> return // not supported yet; drop the connection
            }
    }

    private fun handleStatus(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (packetId) {
            StatusRequestPacket.ID -> {
                val status =
                    statusProvider(
                        handshake?.protocolVersion ?: 0,
                        handshake?.hostName.orEmpty(),
                        connection.remoteAddress,
                    )
                sendUncompressed(StatusResponsePacket.ID) { StatusResponsePacket(status).write(it) }
            }
            PingRequestPacket.ID -> {
                val ping = PingRequestPacket.read(body)
                sendUncompressed(PongResponsePacket.ID) { PongResponsePacket(ping.time).write(it) }
            }
        }
    }

    private fun handleLogin(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (packetId) {
            LoginStartPacket.ID -> handleLoginStart(body)
            KeyPacket.ID -> handleKey(body)
            LoginAcknowledgedPacket.ID -> enterConfiguration()
        }
    }

    private fun handleLoginStart(body: ByteBuffer) {
        val loginStart = LoginStartPacket.read(body)
        val online = onlineMode
        if (online == null) {
            finishLogin(OfflineUuid.forUsername(loginStart.name), loginStart.name, emptyList())
            return
        }

        pendingUsername = loginStart.name
        val nonce = Random.nextBytes(4)
        challenge = nonce
        sendUncompressed(HelloPacket.ID) {
            HelloPacket("", online.keyPair.publicKeyDer, nonce, shouldAuthenticate = true).write(it)
        }
    }

    private fun handleKey(body: ByteBuffer) {
        val online = onlineMode
        val expectedChallenge = challenge
        val username = pendingUsername
        if (online == null || expectedChallenge == null || username == null) {
            connection.close()
            return
        }

        val keyPacket = KeyPacket.read(body)
        val decryptedChallenge = online.keyPair.decrypt(keyPacket.encryptedChallenge)
        if (!decryptedChallenge.contentEquals(expectedChallenge)) {
            connection.close()
            return
        }

        val secretKey = online.keyPair.decrypt(keyPacket.encryptedSecret)
        decryptCipher = Cfb8Cipher.create(secretKey)
        encryptCipher = Cfb8Cipher.create(secretKey)

        val serverHash = MinecraftCrypt.sessionHash("", secretKey, online.keyPair.publicKeyDer)
        val profile = online.sessionService.hasJoined(username, serverHash, clientAddress = null)
        if (profile == null) {
            connection.close()
            return
        }
        finishLogin(profile.id, profile.name, profile.properties)
    }

    private fun finishLogin(
        id: Uuid,
        name: String,
        properties: List<SessionProfileProperty>,
    ) {
        sendUncompressed(LoginCompressionPacket.ID) { LoginCompressionPacket(compressionThreshold).write(it) }
        compressionEnabled = true

        val profile = GameProfile(id, name, properties.map { GameProfileProperty(it.name, it.value, it.signature) })
        pendingProfile = profile
        val sessionId = OfflineUuid.forUsername("session:$name")
        sendCompressed(LoginFinishedPacket.ID) { LoginFinishedPacket(profile, sessionId).write(it) }
    }

    // --- Configuration ---

    private fun enterConfiguration() {
        state = ProtocolState.CONFIGURATION
        // Claims the client's own vanilla data pack rather than an empty list: the more correct
        // default for a server with no datapacks of its own, though tested live against a real
        // 26.2 client and confirmed this alone does not fill in registries Basalt never sends
        // (cat_variant, wolf_variant, and the rest a real client still requires non-empty).
        sendCompressed(SelectKnownPacksPacket.CLIENTBOUND_ID) {
            SelectKnownPacksPacket(listOf(KnownPack("minecraft", "core", "26.2"))).write(it)
        }
    }

    private fun handleConfiguration(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (packetId) {
            SelectKnownPacksPacket.SERVERBOUND_ID -> sendRegistriesThenFinish()
            FinishConfigurationPacket.ID -> enterPlay()
            ClientInformationPacket.ID, CONFIGURATION_KEEP_ALIVE_ID -> Unit
        }
    }

    private fun sendRegistriesThenFinish() {
        sendRegistry("minecraft:dimension_type", "minecraft:overworld", VanillaRegistryData.overworldDimensionType())
        // The full 55-biome Overworld set in one packet, in Biomes.REGISTRY order: each entry's
        // numeric id is its index here, which is what chunk biome palettes then reference.
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:worldgen/biome",
                VanillaBiomes.ALL.map { entry ->
                    RegistryEntry("minecraft:${entry.path}", with(VanillaBiomes) { entry.toNbt() })
                },
            ).write(it)
        }
        // These 13 are the only registries a real client hard-requires non-empty (see
        // VanillaRegistryData's KDoc); the rest of the full 29-registry set is a documented gap.
        // Entry names matter here, not just presence: spawn eggs for wolf/pig/cow/chicken/zombie
        // nautilus carry a default variant component resolving each registry's real `DEFAULT`
        // ResourceKey (confirmed live: chicken_variant crashed looking up specifically
        // minecraft:temperate, not just any entry). Cat and frog have no spawn egg and no DEFAULT
        // in the real jar, so any entry name works for them.
        sendRegistry("minecraft:wolf_variant", "minecraft:pale", VanillaRegistryData.wolfVariant())
        sendRegistry("minecraft:wolf_sound_variant", "minecraft:classic", VanillaRegistryData.wolfSoundVariant())
        sendRegistry("minecraft:pig_variant", "minecraft:temperate", VanillaRegistryData.pigVariant())
        sendRegistry("minecraft:pig_sound_variant", "minecraft:classic", VanillaRegistryData.pigSoundVariant())
        sendRegistry("minecraft:frog_variant", "minecraft:cold", VanillaRegistryData.frogVariant())
        sendRegistry("minecraft:cat_variant", "minecraft:black", VanillaRegistryData.catVariant())
        sendRegistry("minecraft:cat_sound_variant", "minecraft:classic", VanillaRegistryData.catSoundVariant())
        sendRegistry("minecraft:cow_sound_variant", "minecraft:classic", VanillaRegistryData.cowSoundVariant())
        sendRegistry("minecraft:cow_variant", "minecraft:temperate", VanillaRegistryData.cowVariant())
        sendRegistry("minecraft:chicken_sound_variant", "minecraft:classic", VanillaRegistryData.chickenSoundVariant())
        // egg/blue_egg/brown_egg each reference a distinct chicken variant directly, not just
        // DEFAULT (confirmed live: minecraft:chicken_variant/minecraft:cold failed after temperate
        // alone was fixed): see VanillaRegistryData.chickenVariant's KDoc.
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:chicken_variant",
                listOf("temperate", "cold", "warm").map { id ->
                    RegistryEntry("minecraft:$id", VanillaRegistryData.chickenVariant(id))
                },
            ).write(it)
        }
        sendRegistry(
            "minecraft:zombie_nautilus_variant",
            "minecraft:temperate",
            VanillaRegistryData.zombieNautilusVariant(),
        )
        sendRegistry("minecraft:painting_variant", "minecraft:humble", VanillaRegistryData.paintingVariant())
        // Not in the 13 "must be non-empty" registries, but several base items (redstone dust,
        // ingots, gems, resin brick) carry a default trim-material component that resolves a
        // specific entry by key regardless: confirmed live, a real client failed looking up
        // minecraft:trim_material/minecraft:redstone while building item data components. All
        // eleven vanilla items' referenced entries are needed (see VanillaRegistryData.trimMaterial).
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:trim_material",
                VanillaRegistryData.trimMaterials.map { (id, color) ->
                    RegistryEntry("minecraft:$id", VanillaRegistryData.trimMaterial(id, color))
                },
            ).write(it)
        }
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:damage_type",
                VanillaRegistryData.damageTypes.map { (id, value) -> RegistryEntry("minecraft:$id", value) },
            ).write(it)
        }
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:jukebox_song",
                VanillaRegistryData.jukeboxSongs.map { (id, value) -> RegistryEntry("minecraft:$id", value) },
            ).write(it)
        }
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:instrument",
                VanillaRegistryData.instruments.map { (id, value) -> RegistryEntry("minecraft:$id", value) },
            ).write(it)
        }
        // The remaining registries in RegistryDataLoader.SYNCHRONIZED_REGISTRIES that nothing in
        // item bootstrap needs populated yet: sent empty so the client's RegistryAccess has an
        // entry for them at all (a registry that was never sent doesn't exist client-side, which
        // breaks resolving tags scoped to it: confirmed live, see VanillaTagData's KDoc).
        for (emptyRegistryId in EMPTY_SYNCHRONIZED_REGISTRIES) {
            sendCompressed(RegistryDataPacket.ID) { RegistryDataPacket(emptyRegistryId, emptyList()).write(it) }
        }
        // Item/entity/block bootstrap looks up many tags by key directly (fire resistance,
        // repairable materials, horse armor, banner patterns...), independent of whether the tagged
        // registry itself is loaded: confirmed live, a real client failed looking up
        // minecraft:damage_type's is_fire tag with no `tags` packet sent at all. Sending every tag
        // name the real 26.2 jar ships (see VanillaTagData), each with empty entries: the lookup
        // only needs the key to exist, not be populated yet.
        sendCompressed(TagsPacket.ID) {
            TagsPacket(
                VanillaTagData.tagsByRegistry.map { (registryId, tagNames) ->
                    TagGroup(registryId, tagNames.map { TagEntry(it, emptyList()) })
                },
            ).write(it)
        }
        sendCompressed(FinishConfigurationPacket.ID) { }
    }

    private fun sendRegistry(
        registryId: String,
        entryName: String,
        value: NbtCompound,
    ) {
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(registryId, listOf(RegistryEntry(entryName, value))).write(it)
        }
    }

    // --- Play ---

    private fun enterPlay() {
        state = ProtocolState.PLAY
        val registry = playerRegistry
        val profile = pendingProfile
        if (registry == null || profile == null) {
            connection.close()
            return
        }

        val entityId = registry.allocateEntityId()

        // Which world this player belongs to is decided before anything is sent, because the login
        // packet already carries a position. A stored world that is no longer declared (its table
        // was removed from worlds.toml) is not an error: the player goes to the default world and
        // the log says so, which is the whole point of keeping player state outside worlds.
        val stored = playerStore?.players?.load(profile.id.toString())
        val requested = stored?.world
        val world =
            worlds[requested]
                ?: defaultWorld
                ?: run {
                    connection.close()
                    return
                }
        if (requested != null && requested != world.name) {
            logger.info { "${profile.name}'s world '$requested' is not loaded; sending them to '${world.name}'." }
        }
        currentWorld = world

        val chunkRadius = world.settings.viewDistance
        val (spawnX, spawnY, spawnZ) = world.spawn
        // Before the login packet rather than after it, because everything below this point (the
        // abilities packet, the command tree) describes this player and so needs them to exist.
        val session = PlayerSession(profile.id, profile.name, entityId, this, gamemode = defaultGamemode)
        session.x = spawnX
        session.y = spawnY
        session.z = spawnZ
        session.flying = defaultGamemode == Gamemode.SPECTATOR
        session.worldName = world.name
        // Before a single packet goes out: the login packet carries the game mode and the position,
        // so restoring afterwards would mean announcing one state and then correcting it.
        if (stored != null) {
            PlayerPersistence.restore(session, stored)
            firstSeenEpochSeconds = stored.firstSeenEpochSeconds
            playTimeTicks = stored.playTimeTicks
            // Only the *position* is meaningless when the stored world is gone. Health, hunger,
            // experience and game mode are the player's, not the world's, and dropping them along
            // with the coordinates would undo the whole reason player state lives outside worlds.
            if (requested != world.name) {
                session.x = spawnX
                session.y = spawnY
                session.z = spawnZ
            }
            logger.info {
                "Loaded ${session.name} from players.db " +
                    "(last seen ${Logger.formatEpochSeconds(stored.lastSeenEpochSeconds)})."
            }
        }
        playerSession = session

        sendCompressed(LoginPlayPacket.ID) {
            LoginPlayPacket(
                entityId = entityId,
                isHardcore = false,
                // Every world, so the client accepts being moved into any of them later.
                worldNames = worlds.values.map { it.levelKey },
                maxPlayers = 20,
                viewDistance = chunkRadius,
                simulationDistance = chunkRadius,
                reducedDebugInfo = false,
                enableRespawnScreen = true,
                doLimitedCrafting = false,
                // The session's mode, not the configured default: a returning player was restored
                // above, and sending the default here left the client showing a survival HUD while
                // its abilities and block breaking followed the creative mode the server believed.
                spawnInfo = spawnInfo(session.gamemode),
                onlineMode = onlineMode != null,
                enforcesSecureChat = false,
            ).write(it)
        }

        // Straight after the login packet, the same place vanilla's PlayerList.placeNewPlayer puts
        // them: abilities decide whether the client will even let itself fly, and without a command
        // tree its own parser rejects every slash command before one reaches the server.
        sendAbilities()
        sendCommandTree()

        // Without these, a real client's "loading terrain" screen never dismisses: it has no
        // center to count chunks around and no radius to know when it has enough (confirmed live).
        centerChunkX = session.x.toInt() shr 4
        centerChunkZ = session.z.toInt() shr 4
        viewRadius = chunkRadius
        // Seeds the tracker with exactly the square about to be sent below, so later crossings
        // compute deltas relative to it instead of re-sending everything as "newly loaded" on the
        // first move.
        chunkTracker.update(centerChunkX, centerChunkZ, viewRadius)
        sendCompressed(SetChunkCacheCenterPacket.ID) { SetChunkCacheCenterPacket(centerChunkX, centerChunkZ).write(it) }
        sendCompressed(SetChunkCacheRadiusPacket.ID) { SetChunkCacheRadiusPacket(chunkRadius).write(it) }
        sendCompressed(SetSimulationDistancePacket.ID) { SetSimulationDistancePacket(chunkRadius).write(it) }

        // Mirrors real vanilla's join order (PlayerList.placeNewPlayer/sendLevelInfo, confirmed by
        // decompiling the real jar): teleport and player info arrive before level data, and the
        // level_chunks_load_start game event comes right before chunks, not after. Sending chunks
        // first (as an earlier version of this code did) left a real client stuck on "loading
        // terrain" forever, even with correct chunk data and a correct batch wrapper.
        sendCompressed(SpawnPositionPacket.ID) {
            SpawnPositionPacket(
                GlobalPos(world.levelKey, BlockPos(spawnX.toInt(), spawnY.toInt(), spawnZ.toInt())),
                yaw = 0f,
                pitch = 0f,
            ).write(it)
        }
        sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(
                0,
                session.x,
                session.y,
                session.z,
                0.0,
                0.0,
                0.0,
                session.yaw,
                session.pitch,
                relativeFlags = 0,
            ).write(it)
        }

        val others = registry.snapshot()
        registry.add(session)
        logger.info { "${session.name} logged in with entity id $entityId at ($spawnX, $spawnY, $spawnZ)" }
        logger.info { "${session.name} joined the game" }

        sendCompressed(PlayerInfoUpdatePacket.ID) {
            PlayerInfoUpdatePacket
                .initializing(
                    others.map { PlayerInfoEntry(it.uuid, it.name, listed = true, gamemode = it.gamemode) } +
                        PlayerInfoEntry(session.uuid, session.name, listed = true, gamemode = session.gamemode),
                ).write(it)
        }

        // The HUD draws no hearts and no experience bar until it has been told what to draw.
        // Vanilla gets there by a different route (it marks both dirty on join and lets the next
        // player tick send them), which arrives at the same place one tick later.
        syncHealth()
        syncExperience()
        // parity: `PlayerList.placeNewPlayer` sends the inventory here, after the level info and the
        // player list, not among the chunk-radius packets above. Order matters on join: an earlier
        // version of this code put a packet in the middle of that sequence and left real clients
        // stuck on "loading terrain".
        sendInventory()

        // The client only starts tracking/waiting for level chunks once it sees this event: without
        // it (confirmed live) the loading screen never dismisses even after every chunk and a
        // correct chunk-batch wrapper arrive, since the client was never told to start expecting them.
        sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.LEVEL_CHUNKS_LOAD_START, 0f).write(it)
        }
        // The declared view distance above is a promise: send every chunk it actually covers, not
        // just whatever BasaltServer happened to pre-generate at startup. This one batch is the
        // only place that still waits for generation, because a client cannot enter the world
        // without ground under it; from here on chunks stream in through `flushReadyChunks`.
        // Ordered nearest-first so the queue builds what the player is looking at before the edges.
        val loginPositions =
            (-chunkRadius..chunkRadius)
                .flatMap { dx -> (-chunkRadius..chunkRadius).map { dz -> (centerChunkX + dx) to (centerChunkZ + dz) } }
                .sortedBy { squaredDistanceToCenter(it.first, it.second) }
        val loginChunks = generationQueue.awaitAll(loginPositions)
        sendCompressed(ChunkBatchStartPacket.ID) { }
        for (chunk in loginChunks) sendChunk(chunk)
        sendCompressed(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(loginChunks.size).write(it) }

        for (other in others) {
            other.connection.sendPlayPacket(PlayerInfoUpdatePacket.ID) {
                PlayerInfoUpdatePacket
                    .initializing(
                        listOf(PlayerInfoEntry(session.uuid, session.name, listed = true, gamemode = session.gamemode)),
                    ).write(it)
            }
            other.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(
                    session.entityId,
                    session.x,
                    session.y,
                    session.z,
                    0f,
                    0f,
                    onGround = true,
                ).write(it)
            }
            sendCompressed(EntityTeleportPacket.ID) {
                EntityTeleportPacket(
                    other.entityId,
                    other.x,
                    other.y,
                    other.z,
                    other.yaw,
                    other.pitch,
                    other.onGround,
                ).write(it)
            }
        }

        // Drawn here rather than left to the loop below. That loop sends chunks before it redraws
        // anything, so on a fresh join the scoreboard, tab list and boss bar waited behind the whole
        // terrain stream: seconds of a player staring at a bare screen for packets that were ready
        // immediately.
        tickUi()

        // Last, once the whole join sequence is on the wire. Started any earlier and this thread
        // interleaves chunk batches with the packets still being sent to finish joining, which a
        // client reads as a protocol desync; it only shows up when the machine is slow enough for
        // the two to overlap.
        startOutboundLoop()
    }

    /** The `CommonPlayerSpawnInfo` both joining and respawning need, which differ only in game mode. */
    private fun spawnInfo(gamemode: Gamemode): SpawnInfo =
        SpawnInfo(
            // The dimension *type* is the same for every world so far (overworld); the level key
            // is what distinguishes them, and what makes a world change a real dimension change.
            dimension = 0,
            dimensionName = currentWorld?.levelKey ?: "minecraft:overworld",
            hashedSeed = 0L,
            gamemode = gamemode,
            previousGamemode = 255,
            isDebug = false,
            isFlat = true,
            death = null,
            portalCooldown = 0,
            seaLevel = 63,
        )

    private fun sendChunk(chunk: Chunk) {
        val chunkData = ChunkDataCodec.encodeSections(chunk)
        val light = ChunkDataCodec.encodeLight(chunk)
        val heightmap = ChunkDataCodec.encodeHeightmap(chunk)
        sendCompressed(MapChunkPacket.ID) {
            MapChunkPacket(
                x = chunk.chunkX,
                z = chunk.chunkZ,
                heightmaps = listOf(Heightmap(HeightmapType.MOTION_BLOCKING, heightmap)),
                chunkData = chunkData,
                blockEntities =
                    chunk.blockEntities().map { blockEntity ->
                        ChunkBlockEntityInfo(
                            packedXZ = (blockEntity.localX shl 4) or blockEntity.localZ,
                            y = blockEntity.y,
                            typeId = blockEntityTypeId(blockEntity.type.toString()),
                            tag = blockEntity.data,
                        )
                    },
                skyLightMask = light.skyLightMask,
                blockLightMask = longArrayOf(0L),
                emptySkyLightMask = light.emptySkyLightMask,
                emptyBlockLightMask = longArrayOf(0L),
                skyLight = light.skyLight,
                blockLight = emptyList(),
            ).write(it)
        }
        sendCompressed(UpdateLightPacket.ID) {
            UpdateLightPacket(
                chunkX = chunk.chunkX,
                chunkZ = chunk.chunkZ,
                skyLightMask = light.skyLightMask,
                blockLightMask = longArrayOf(0L),
                emptySkyLightMask = light.emptySkyLightMask,
                emptyBlockLightMask = longArrayOf(0L),
                skyLight = light.skyLight,
                blockLight = emptyList(),
            ).write(it)
        }
    }

    private fun blockEntityTypeId(identifier: String): Int =
        when (identifier) {
            "minecraft:beehive" -> 33
            else -> error("no protocol block entity type ID registered for $identifier")
        }

    private fun handlePlay(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (packetId) {
            ServerboundPositionPacket.ID -> {
                val move = ServerboundPositionPacket.read(body)
                updatePosition(
                    move.x,
                    move.y,
                    move.z,
                    playerSession?.yaw ?: 0f,
                    playerSession?.pitch ?: 0f,
                    move.onGround,
                )
            }
            ServerboundPositionLookPacket.ID -> {
                val move = ServerboundPositionLookPacket.read(body)
                updatePosition(move.x, move.y, move.z, move.yaw, move.pitch, move.onGround)
            }
            ChatMessagePacket.ID -> {
                val chat = ChatMessagePacket.read(body)
                broadcastChat(chat.message)
            }
            ChatCommandPacket.ID -> runCommand(ChatCommandPacket.read(body).command)
            // A command carrying signed arguments. Nothing verifies the signatures yet, so it runs
            // exactly like its unsigned form: documented gap, same as signed chat.
            SignedChatCommandPacket.ID -> runCommand(SignedChatCommandPacket.read(body).command)
            ClientCommandPacket.ID -> {
                if (ClientCommandPacket.read(body).action == ClientCommandPacket.Action.PERFORM_RESPAWN) respawn()
            }
            ServerboundPlayerAbilitiesPacket.ID -> {
                // The client toggles its own flight and tells the server after the fact. Believed
                // only when the game mode actually allows it, which is what stops a survival client
                // announcing it is flying.
                val session = playerSession
                if (session !=
                    null
                ) {
                    session.flying = ServerboundPlayerAbilitiesPacket.read(body).flying && session.mayFly
                }
            }
            PlayerCommandPacket.ID -> {
                val command = PlayerCommandPacket.read(body)
                when (command.action) {
                    PlayerCommandPacket.Action.START_SPRINTING -> playerSession?.sprinting = true
                    PlayerCommandPacket.Action.STOP_SPRINTING -> playerSession?.sprinting = false
                    else -> Unit
                }
            }
            ServerboundPlayerActionPacket.ID -> {
                val action = ServerboundPlayerActionPacket.read(body)
                handlePlayerAction(action)
            }
            ServerboundSetCarriedItemPacket.ID -> {
                val slot = ServerboundSetCarriedItemPacket.read(body).slot
                if (slot in 0 until ServerboundSetCarriedItemPacket.HOTBAR_SIZE) {
                    playerSession?.inventory?.heldSlot = slot
                }
            }
            CONTAINER_CLICK_ID -> {
                // No click handling yet, so the click is refused rather than half-applied: the
                // window goes back out as it really is and the client drops its prediction. Read
                // nothing out of the body on purpose, since its shape is exactly the part that is
                // not implemented.
                sendInventory()
            }
            ServerboundUseItemOnPacket.ID -> {
                // Placing a block depends on which item the player is holding, and there's no
                // inventory/held-item state to consult yet: acknowledged (so the client's own
                // prediction doesn't get stuck waiting), not acted on. Documented gap.
                val useItemOn = ServerboundUseItemOnPacket.read(body)
                sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                    ClientboundBlockChangedAckPacket(useItemOn.sequence).write(it)
                }
            }
            ChunkBatchReceivedPacket.ID -> {
                // The client's own answer to "how fast can you take chunks". Ignoring it meant
                // sending at whatever rate the server could produce, which floods the client,
                // backs its socket up, and buries the server in encoding work for chunks that are
                // not going to be looked at. This is the protocol's flow control; it is not advice.
                desiredChunksPerTick =
                    ChunkBatchReceivedPacket
                        .read(body)
                        .desiredChunksPerTick
                        .coerceIn(MIN_CHUNKS_PER_TICK, MAX_CHUNKS_PER_TICK)
            }
            AcceptTeleportationPacket.ID,
            PlayKeepAlivePacket.SERVERBOUND_ID,
            PlayerLoadedPacket.ID,
            // `player_input`: the movement keys currently held, sent on every change. Sneaking
            // lives here now (it left `player_command`), so this is where crouch exhaustion would
            // come from if crouching cost anything in 26.2, which it does not. Named rather than
            // left to fall off the end of this `when` because it is constant traffic, not an
            // oddity: it arrives many times a second from every client.
            SERVERBOUND_PLAYER_INPUT_ID,
            -> Unit
        }
    }

    /**
     * Breaking a block, on whichever packet the player's game mode breaks on.
     *
     * parity: `ServerPlayerGameMode`. Creative destroys on `START_DESTROY_BLOCK` with no timing at
     * all; survival times the dig client-side and only then sends `STOP_DESTROY_BLOCK`. Matching
     * that split is what keeps the client's own prediction in step: a creative client expects the
     * block gone immediately, a survival one expects it to survive until it finishes digging.
     *
     * The gap is that survival's timing is *trusted* rather than verified. Vanilla re-derives the
     * dig time from block hardness and the held tool and rejects a client that finishes too early;
     * that needs hardness data and an inventory, neither of which exists yet. Adventure and
     * spectator cannot break anything, which they already could not in vanilla.
     */
    private fun handlePlayerAction(action: ServerboundPlayerActionPacket) {
        val gamemode = playerSession?.gamemode ?: defaultGamemode
        val breaksOn =
            when (gamemode) {
                Gamemode.CREATIVE -> ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK
                Gamemode.SURVIVAL -> ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK
                Gamemode.ADVENTURE, Gamemode.SPECTATOR -> null
            }
        if (action.action != breaksOn) {
            sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(action.sequence).write(it)
            }
            return
        }

        val broken = world.getBlockState(action.pos.x, action.pos.y, action.pos.z)
        val changed = world.setBlockState(action.pos.x, action.pos.y, action.pos.z, Blocks.AIR.stateId)
        sendCompressed(ClientboundBlockChangedAckPacket.ID) {
            ClientboundBlockChangedAckPacket(action.sequence).write(it)
        }
        if (!changed) return
        broken?.let { stateId ->
            Blocks.byStateId(stateId)?.let { playerSession?.stats?.increment(StoredStats.mined(it.block.toString())) }
        }

        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(action.pos, Blocks.AIR.stateId).write(it)
            }
        }
    }

    /** Everyone who can actually see this player: the tab list is server-wide, entities are not. */
    private fun playersInSameWorld(): List<PlayerSession> {
        val name = playerSession?.worldName ?: return emptyList()
        return playerRegistry?.snapshot()?.filter { it.worldName == name }.orEmpty()
    }

    private fun updatePosition(
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        pitch: Float,
        onGround: Boolean,
    ) {
        val session = playerSession ?: return
        session.food.addExhaustion(
            FoodData.movementExhaustion(x - session.x, z - session.z, session.sprinting, onGround),
        )
        // Horizontal only, and only on the ground, which is how vanilla tells walking apart from
        // falling and from being moved by something else.
        if (onGround) {
            val dx = x - session.x
            val dz = z - session.z
            session.stats.addWalked(kotlin.math.sqrt(dx * dx + dz * dz))
        }
        session.x = x
        session.y = y
        session.z = z
        session.yaw = yaw
        session.pitch = pitch
        session.onGround = onGround
        streamChunksAround(x, z)
        for (other in playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(
                    session.entityId,
                    x,
                    y,
                    z,
                    yaw,
                    pitch,
                    onGround,
                ).write(it)
            }
        }
    }

    /**
     * Re-centers [chunkTracker] on the player's current chunk and applies the resulting delta: newly
     * in-range chunks are generated on demand ([WorldState.getOrGenerateChunks], across a worker
     * pool, not serially) and sent, chunks that fell out of range are forgotten client-side. A
     * no-op when the player hasn't crossed into a new chunk, so this stays cheap on every movement
     * packet, not just the ones that matter.
     */
    private fun streamChunksAround(
        x: Double,
        z: Double,
    ) {
        val chunkX = x.toInt() shr 4
        val chunkZ = z.toInt() shr 4
        if (chunkX == centerChunkX && chunkZ == centerChunkZ) return
        val previousCenterX = centerChunkX
        val previousCenterZ = centerChunkZ
        centerChunkX = chunkX
        centerChunkZ = chunkZ

        val delta = chunkTracker.update(chunkX, chunkZ, viewRadius)
        if (delta.toLoad.isEmpty() && delta.toUnload.isEmpty()) return

        sendCompressed(SetChunkCacheCenterPacket.ID) { SetChunkCacheCenterPacket(chunkX, chunkZ).write(it) }
        chunkStreamMutex.withLock {
            for (pos in delta.toLoad) pendingChunks.add(pos)
            for (pos in delta.toUnload) pendingChunks.remove(pos)
        }
        for (pos in delta.toLoad) {
            generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
        }
        prefetchAhead(chunkX, chunkZ, movedX = chunkX - previousCenterX, movedZ = chunkZ - previousCenterZ)
        // Unloads go out straight away rather than waiting behind generation: the client can drop
        // what it no longer needs before the replacements are even built.
        for (pos in delta.toUnload) {
            sendCompressed(ForgetLevelChunkPacket.ID) { ForgetLevelChunkPacket(pos.x, pos.z).write(it) }
        }
        flushReadyChunks()
    }

    /**
     * Asks for a margin of chunks just beyond the view distance, in whichever direction the player
     * is actually travelling. Without this the first time a chunk is wanted is the moment it comes
     * into view, so at speed the player outruns generation no matter how fast a single chunk is;
     * with it, the chunks ahead are already warm by the time the view reaches them. They are never
     * sent from here, only generated: they go out normally once [chunkTracker] takes them in.
     *
     * Requested behind everything already visible ([PREFETCH_PRIORITY_PENALTY]), so speculative
     * work can never delay a chunk the player is currently staring at.
     */
    private fun prefetchAhead(
        chunkX: Int,
        chunkZ: Int,
        movedX: Int,
        movedZ: Int,
    ) {
        if (movedX == 0 && movedZ == 0) return
        val stepX = movedX.coerceIn(-1, 1)
        val stepZ = movedZ.coerceIn(-1, 1)
        for (margin in 1..PREFETCH_MARGIN_CHUNKS) {
            val aheadX = chunkX + stepX * (viewRadius + margin)
            val aheadZ = chunkZ + stepZ * (viewRadius + margin)
            for (offset in -viewRadius..viewRadius) {
                val x = if (stepX != 0) aheadX else chunkX + offset
                val z = if (stepZ != 0) aheadZ else chunkZ + offset
                generationQueue.request(x, z, squaredDistanceToCenter(x, z) + PREFETCH_PRIORITY_PENALTY)
            }
        }
    }

    /**
     * Sends whichever requested chunks have finished generating, nearest first, and leaves the rest
     * pending. Never blocks on generation.
     *
     * Driven from [TickLoop] rather than only from this connection's own read loop: a server that
     * only pushed chunks when the client happened to send something would stall the moment a client
     * went quiet, and "the client keeps sending position packets" is not a guarantee worth resting
     * chunk delivery on. The read loop calls it too, so a client that is streaming input gets its
     * chunks at input rate rather than waiting for the next tick.
     *
     * Capped per call so one flush cannot monopolize the socket while the client is also waiting on
     * movement and entity updates.
     */
    internal fun flushReadyChunks(limit: Int = MAX_BURST_CHUNKS.toInt()): Int {
        if (state != ProtocolState.PLAY) return 0
        val ready =
            chunkStreamMutex.withLock {
                if (pendingChunks.isEmpty()) return 0
                pendingChunks.sortBy { squaredDistanceToCenter(it.x, it.z) }
                val found = mutableListOf<Chunk>()
                val iterator = pendingChunks.iterator()
                while (iterator.hasNext() && found.size < limit) {
                    val pos = iterator.next()
                    val chunk = world.chunkIfLoaded(pos.x, pos.z)
                    if (chunk == null) {
                        // Requested, then unloaded again before it could be sent. Ask for it once
                        // more rather than waiting forever on a chunk nothing is going to produce:
                        // the tracker already believes the client has it, so nothing else would.
                        generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
                        continue
                    }
                    found.add(chunk)
                    iterator.remove()
                }
                found
            }
        if (ready.isEmpty()) return 0
        sendCompressed(ChunkBatchStartPacket.ID) { }
        for (chunk in ready) sendChunk(chunk)
        sendCompressed(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(ready.size).write(it) }
        return ready.size
    }

    private fun squaredDistanceToCenter(
        chunkX: Int,
        chunkZ: Int,
    ): Int {
        val dx = chunkX - centerChunkX
        val dz = chunkZ - centerChunkZ
        return dx * dx + dz * dz
    }

    /**
     * Relays a chat message to everyone and writes it to the console.
     *
     * The console line is `<Name> message`, which is what vanilla logs: `MinecraftServer
     * .logChatMessage` logs the message already decorated by its chat type, and `chat.type.text` is
     * exactly that shape. Until this session sent nothing to the log at all, so a player talking
     * was invisible to whoever was running the server.
     */
    private fun broadcastChat(message: String) {
        val session = playerSession ?: return
        val text = Component.text("<${session.name}> $message", TextColor.WHITE)
        logger.info { text.resolvePlainText(consoleLang) }
        for (player in playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(
                SystemChatPacket.ID,
            ) { SystemChatPacket(text, isActionBar = false).write(it) }
        }
    }

    /**
     * Runs a slash command on behalf of this player.
     *
     * The console line has no vanilla counterpart (vanilla only logs the *result* of a command, as
     * `chat.type.admin`, and only for the ones that broadcast). It is logged anyway because an
     * operator watching the console should be able to see what players are running, and console
     * output is not part of protocol parity: nothing a client can observe changes.
     */
    private fun runCommand(command: String) {
        val session = playerSession ?: return
        val dispatcher = commands ?: return
        logger.info { "${session.name} issued server command: /$command" }
        dispatcher.execute(PlayerCommandSource(session, dispatcher.environment.groups), command)
    }

    // --- Player state ---

    /**
     * The whole inventory, and which slot is in hand.
     *
     * Also the answer to a click: this build has no click handling, so a click is refused by
     * re-sending the window with a fresh state id. The client drops whatever it was predicting and
     * matches the server again, which is a read-only inventory rather than a desynchronised one.
     */
    private fun sendInventory() {
        val session = playerSession ?: return
        val state = ++containerStateId
        sendCompressed(ContainerSetContentPacket.ID) {
            ContainerSetContentPacket(
                containerId = ContainerSetContentPacket.PLAYER_INVENTORY,
                stateId = state,
                items = session.inventory.toWire(),
            ).write(it)
        }
        sendCompressed(SetHeldSlotPacket.ID) { SetHeldSlotPacket(session.inventory.heldSlot).write(it) }
    }

    /** Re-sends the whole window, for a change that could have touched any slot. */
    internal fun sendInventoryAll() {
        sendInventory()
    }

    /** One slot changed, for when re-sending forty of them would be silly. */
    internal fun sendInventorySlot(slot: Int) {
        val session = playerSession ?: return
        sendPlayPacket(ContainerSetSlotPacket.ID) {
            ContainerSetSlotPacket(
                containerId = ContainerSetContentPacket.PLAYER_INVENTORY,
                stateId = ++containerStateId,
                slot = slot,
                item = session.inventory.toWire(slot),
            ).write(it)
        }
    }

    /** Redraws whatever is due. Failures are swallowed: decoration must never drop a connection. */
    private fun tickUi() {
        if (!ui().anyEnabled) return
        val session = playerSession ?: return
        val context = PlaceholderContext.EMPTY.with(PlaceholderSlots.PLAYER, session)
        runCatching { playerUi.tick(uiTick++, context) }
            .onFailure { logger.debug { "UI update failed for ${session.name}: ${it.message}" } }
    }

    /**
     * One player tick, driven from this connection's own outbound loop rather than [TickLoop].
     *
     * That loop already runs at 20 Hz on this connection's thread, which is where anything that
     * writes to this socket has to happen (see [startOutboundLoop]): hunger and regeneration both
     * end in a `set_health` packet, so ticking them from the shared tick thread would park every
     * other player behind a client that has stopped reading.
     */
    private fun tickPlayer() {
        val session = playerSession ?: return
        playTimeTicks++
        if (session.dead) return
        if (session.gamemode == Gamemode.CREATIVE || session.gamemode == Gamemode.SPECTATOR) return

        val healthBefore = session.health
        val foodBefore = session.food.foodLevel
        val saturationBefore = session.food.saturationLevel
        session.food.tick(
            difficulty = difficulty,
            health = session.health,
            maxHealth = PlayerSession.MAX_HEALTH,
            heal = { amount -> session.health = minOf(session.health + amount, PlayerSession.MAX_HEALTH) },
            hurt = { amount -> hurt(amount, DEATH_STARVE) },
        )
        // Only when something actually moved: `set_health` every tick is 20 packets a second per
        // player for a bar that changes a few times a minute.
        if (session.health != healthBefore ||
            session.food.foodLevel != foodBefore ||
            session.food.saturationLevel != saturationBefore
        ) {
            syncHealth()
        }
    }

    /** Damage that ignores armour and absorption, because neither exists yet: a documented gap. */
    internal fun hurt(
        amount: Float,
        deathMessageId: String,
    ) {
        val session = playerSession ?: return
        if (session.dead || session.invulnerable) return
        session.stats.addDamageTaken(amount)
        session.health = maxOf(session.health - amount, 0f)
        if (session.health <= 0f) die(deathMessageId) else syncHealth()
    }

    /** parity: `Entity.kill`, which is unblockable: creative mode does not survive it. */
    internal fun killPlayer(deathMessageId: String) {
        val session = playerSession ?: return
        if (session.dead) return
        session.health = 0f
        die(deathMessageId)
    }

    /**
     * parity: `ServerPlayer.die`. The death message goes to everyone (`showDeathMessages` is on by
     * default) and the death screen itself is [PlayerCombatKillPacket]: without it the client never
     * learns it died and keeps playing.
     */
    private fun die(deathMessageId: String) {
        val session = playerSession ?: return
        session.dead = true
        session.health = 0f
        session.stats.increment(StoredStats.DEATHS)
        syncHealth()
        val message = Component.translatable("death.attack.$deathMessageId", Component.text(session.name))
        sendCompressed(PlayerCombatKillPacket.ID) { PlayerCombatKillPacket(session.entityId, message).write(it) }
        logger.info { message.resolvePlainText(consoleLang) }
        for (player in playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(SystemChatPacket.ID) {
                SystemChatPacket(message, isActionBar = false).write(it)
            }
        }
    }

    /**
     * parity: `PlayerList.respawn`, in the order it sends things: respawn, then position, then the
     * spawn point, then experience.
     *
     * The client throws its whole level away when it handles the respawn packet, so the chunks
     * around the spawn point have to be sent again; [chunkTracker] is reset to match, otherwise it
     * would believe the client still had them and send nothing.
     */
    private fun respawn() {
        val session = playerSession ?: return
        // parity: `handleClientCommand` returns without doing anything if the player is still
        // alive, so a client that sends this spuriously cannot teleport itself to spawn.
        if (session.health > 0f) return
        val (spawnX, spawnY, spawnZ) = requireNotNull(currentWorld).spawn
        session.resetForRespawn()
        session.x = spawnX
        session.y = spawnY
        session.z = spawnZ

        sendCompressed(RespawnPacket.ID) { RespawnPacket(spawnInfo(session.gamemode), dataToKeep = 0).write(it) }
        sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(
                0,
                session.x,
                session.y,
                session.z,
                0.0,
                0.0,
                0.0,
                session.yaw,
                session.pitch,
                relativeFlags = 0,
            ).write(it)
        }
        sendCompressed(SpawnPositionPacket.ID) {
            SpawnPositionPacket(
                GlobalPos(
                    requireNotNull(currentWorld).levelKey,
                    BlockPos(spawnX.toInt(), spawnY.toInt(), spawnZ.toInt()),
                ),
                yaw = 0f,
                pitch = 0f,
            ).write(it)
        }
        sendAbilities()
        syncHealth()
        syncExperience()

        centerChunkX = spawnX.toInt() shr 4
        centerChunkZ = spawnZ.toInt() shr 4
        chunkTracker.reset()
        val delta = chunkTracker.update(centerChunkX, centerChunkZ, viewRadius)
        sendCompressed(SetChunkCacheCenterPacket.ID) { SetChunkCacheCenterPacket(centerChunkX, centerChunkZ).write(it) }
        sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.LEVEL_CHUNKS_LOAD_START, 0f).write(it)
        }
        chunkStreamMutex.withLock { for (pos in delta.toLoad) pendingChunks.add(pos) }
        for (pos in delta.toLoad) generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
        flushReadyChunks()

        for (other in playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(session.entityId, spawnX, spawnY, spawnZ, 0f, 0f, onGround = true).write(it)
            }
        }
    }

    /**
     * parity: `ServerPlayer.setGameMode`, which tells the client through a game event rather than a
     * dedicated packet, then re-sends abilities because the new mode decides them.
     */
    internal fun setGamemode(mode: Gamemode) {
        val session = playerSession ?: return
        session.gamemode = mode
        if (!session.mayFly) session.flying = false
        if (mode == Gamemode.SPECTATOR) session.flying = true
        sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.CHANGE_GAME_MODE, mode.ordinal.toFloat()).write(it)
        }
        sendAbilities()
        // Broadcast to everyone, this player included, because the game event above only moves the
        // HUD: a client reads its *own* tab-list entry to decide whether it is a spectator, and
        // that is what turns off collision and hides it from other players. Sending one without the
        // other is what left spectator mode looking right and behaving like survival.
        for (player in playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(PlayerInfoUpdatePacket.ID) {
                PlayerInfoUpdatePacket.gameMode(session.uuid, mode).write(it)
            }
        }
    }

    /**
     * Moves this player into another world, or does nothing if they are already in it.
     *
     * A world change is a *level* change to the client, which is why each world has its own
     * [LoadedWorld.levelKey]: the respawn packet makes the client throw away its chunk cache and
     * rebuild it, and it only does that when the level key differs. Everything else here follows
     * from that: the tracker is reset because the client no longer has anything, the chunks are
     * re-requested from the new world's queue, and the players left behind are told to forget this
     * entity so it does not stand frozen in the world they are still in.
     */
    internal fun switchWorld(
        target: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val session = playerSession ?: return false
        val previous = currentWorld
        if (previous == null || previous.name == target.name) return false

        val leftBehind = playersInSameWorld().filter { it.uuid != session.uuid }
        currentWorld = target
        session.worldName = target.name
        session.x = x
        session.y = y
        session.z = z

        for (other in leftBehind) {
            other.connection.sendPlayPacket(RemoveEntitiesPacket.ID) {
                RemoveEntitiesPacket(listOf(session.entityId)).write(it)
            }
        }

        sendCompressed(RespawnPacket.ID) {
            RespawnPacket(spawnInfo(session.gamemode), dataToKeep = RespawnPacket.KEEP_ALL_DATA).write(it)
        }
        sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(0, x, y, z, 0.0, 0.0, 0.0, session.yaw, session.pitch, relativeFlags = 0).write(it)
        }
        sendAbilities()
        syncHealth()
        syncExperience()

        centerChunkX = x.toInt() shr 4
        centerChunkZ = z.toInt() shr 4
        viewRadius = target.settings.viewDistance
        chunkTracker.reset()
        val delta = chunkTracker.update(centerChunkX, centerChunkZ, viewRadius)
        sendCompressed(SetChunkCacheCenterPacket.ID) { SetChunkCacheCenterPacket(centerChunkX, centerChunkZ).write(it) }
        sendCompressed(SetChunkCacheRadiusPacket.ID) { SetChunkCacheRadiusPacket(viewRadius).write(it) }
        sendCompressed(GameEventPacket.ID) { GameEventPacket(GameEventPacket.LEVEL_CHUNKS_LOAD_START, 0f).write(it) }
        chunkStreamMutex.withLock {
            pendingChunks.clear()
            for (pos in delta.toLoad) pendingChunks.add(pos)
        }
        for (pos in delta.toLoad) generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
        flushReadyChunks()

        // Whoever is already there needs to start seeing them.
        for (other in playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(
                    session.entityId,
                    x,
                    y,
                    z,
                    session.yaw,
                    session.pitch,
                    false,
                ).write(it)
            }
        }
        logger.info { "${session.name} moved to world '${target.name}'." }
        return true
    }

    internal fun teleportTo(
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        pitch: Float,
    ) {
        sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(0, x, y, z, 0.0, 0.0, 0.0, yaw, pitch, relativeFlags = 0).write(it)
        }
        // Straight to updatePosition rather than waiting for the client to confirm: the world has
        // to agree the player is there now, so chunks around the destination start generating
        // immediately instead of one round trip later.
        updatePosition(x, y, z, yaw, pitch, onGround = false)
    }

    internal fun syncHealth() {
        val session = playerSession ?: return
        sendCompressed(SetHealthPacket.ID) {
            SetHealthPacket(session.health, session.food.foodLevel, session.food.saturationLevel).write(it)
        }
    }

    internal fun syncExperience() {
        val session = playerSession ?: return
        sendCompressed(SetExperiencePacket.ID) {
            SetExperiencePacket(
                session.experience.progress,
                session.experience.level,
                session.experience.total,
            ).write(it)
        }
    }

    private fun sendAbilities() {
        val session = playerSession ?: return
        sendCompressed(ClientboundPlayerAbilitiesPacket.ID) {
            ClientboundPlayerAbilitiesPacket(
                invulnerable = session.invulnerable,
                flying = session.flying,
                mayFly = session.mayFly,
                instabuild = session.instabuild,
            ).write(it)
        }
    }

    /**
     * Sends the command tree this player may use, which is what makes their client parse commands
     * at all: with no tree, everything after a slash is unknown client-side.
     *
     * Re-sent whenever their permission level changes, because the tree is filtered by it.
     */
    internal fun sendCommandTree() {
        val session = playerSession ?: return
        val dispatcher = commands ?: return
        val tree = dispatcher.treeFor(PlayerCommandSource(session, dispatcher.environment.groups))
        sendCompressed(CommandsPacket.ID) { tree.write(it) }
    }

    private fun leavePlayIfPresent() {
        val session = playerSession ?: return
        val registry = playerRegistry ?: return
        // Before removing them from the registry, so a save triggered elsewhere cannot race this
        // one into writing a player who is already half gone.
        savePlayerState()
        registry.remove(session.uuid)
        logger.info { "${session.name} left the game" }
        for (other in registry.snapshot()) {
            other.connection.sendPlayPacket(
                PlayerInfoRemovePacket.ID,
            ) { PlayerInfoRemovePacket(listOf(session.uuid)).write(it) }
        }
    }

    /**
     * Writes this player's row. Called on logout and from the periodic flush.
     *
     * Failures are logged and swallowed: a database that cannot be written is a serious problem,
     * but taking the connection down over it loses the session as well as the save.
     */
    internal fun savePlayerState() {
        val session = playerSession ?: return
        val store = playerStore ?: return
        if (!store.isOpen) {
            // The server is shutting down and already saved everyone before disconnecting them.
            // This connection's own thread is just catching up.
            logger.debug { "Skipping ${session.name}'s save: the database is closed." }
            return
        }
        runCatching {
            store.players.save(
                PlayerPersistence.toStored(session, worldName, firstSeenEpochSeconds, playTimeTicks),
            )
            // After the save, because there has to be a row to update.
            if (!addressRecorded && rememberAddress != null) {
                store.players.setLastIp(session.uuid.toString(), rememberAddress)
                addressRecorded = true
            }
        }.onFailure { logger.error(it) { "Failed to save ${session.name}" } }
    }

    /** This player's row as it stands right now, for the batched periodic flush. */
    internal fun storedSnapshot(): StoredPlayer? {
        val session = playerSession ?: return null
        return PlayerPersistence.toStored(session, worldName, firstSeenEpochSeconds, playTimeTicks)
    }

    /**
     * Sends to *this* connection from somewhere else: another player's thread broadcasting chat or
     * movement, a command, the tick loop.
     *
     * A write failure here is swallowed on purpose, and only here. The caller is not this
     * connection's owner and has nothing useful to do about its socket; the owning read loop is
     * already going to notice the close and run [leavePlayIfPresent]. Letting it propagate meant a
     * single player who had just disconnected aborted a broadcast partway through, so the players
     * after them in the list silently never got the message and whoever sent it was told the
     * command had failed.
     */
    internal fun sendPlayPacket(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        try {
            sendCompressed(id, writeBody)
        } catch (e: SocketException) {
            logger.debug { "Dropped a packet for a connection that is already gone: ${e.message}" }
        }
    }

    /**
     * Vanilla's own 15-second interval, driven from this connection's read loop rather than a
     * shared timer, so a client that has stopped draining its socket delays nothing but itself.
     */
    @OptIn(ExperimentalTime::class)
    private fun sendKeepAliveIfDue() {
        val now = TimeSource.Monotonic.markNow()
        if (lastKeepAlive.elapsedNow().inWholeSeconds < KEEP_ALIVE_INTERVAL_SECONDS) return
        lastKeepAlive = now
        sendCompressed(PlayKeepAlivePacket.CLIENTBOUND_ID) { PlayKeepAlivePacket(Random.nextLong()).write(it) }
    }

    internal fun disconnect(reason: Component) {
        sendCompressed(PlayDisconnectPacket.ID) { PlayDisconnectPacket(reason).write(it) }
        connection.close()
    }

    private fun sendUncompressed(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeUncompressed(wire, body.toByteArray())
        writeToSocket(wire.toByteArray())
    }

    private fun sendCompressed(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        val body = ByteBuffer()
        body.writeVarInt(id)
        writeBody(body)
        val wire = ByteBuffer()
        PacketFraming.writeCompressed(wire, body.toByteArray(), compressionThreshold, Zlib::compress)
        writeToSocket(wire.toByteArray())
    }

    /**
     * One packet reaches the socket at a time. This connection's own thread is not the only writer:
     * another player's thread broadcasts movement and chat straight into this session, and the tick
     * loop writes keep-alives, so without this two writers interleave their bytes mid-packet and
     * the client desynchronizes on the very next frame length. The cipher is inside the lock
     * because CFB8 is stateful: encrypting two packets concurrently corrupts both even if the
     * writes themselves were ordered.
     */
    private fun writeToSocket(bytes: ByteArray) {
        sendMutex.withLock {
            val cipher = encryptCipher
            connection.write(if (cipher != null) cipher.update(bytes, encrypting = true) else bytes)
        }
    }

    private companion object {
        // How far past the view distance to generate ahead of a moving player, in chunks.
        const val PREFETCH_MARGIN_CHUNKS = 2

        // Added to a prefetched chunk's priority so every visible chunk anywhere outranks it: the
        // largest squared distance inside the view is radius^2 + radius^2, well under this.
        const val PREFETCH_PRIORITY_PENALTY = 1 shl 20

        // Vanilla's own bounds for the rate a client asks for, and a ceiling on how much unsent
        // allowance can pile up while a client is quiet.
        const val INITIAL_CHUNKS_PER_TICK = 2f
        const val MIN_CHUNKS_PER_TICK = 0.01f
        const val MAX_CHUNKS_PER_TICK = 64f
        const val MAX_BURST_CHUNKS = 16f

        // Vanilla's own interval.
        const val KEEP_ALIVE_INTERVAL_SECONDS = 15L

        // One server tick: fast enough that chunks appear promptly, slow enough to be idle-cheap.
        const val OUTBOUND_INTERVAL_MILLIS = 50L

        // parity: `DamageTypes.STARVE`'s message id, which the client renders as "<name> starved to death".
        const val DEATH_STARVE = "starve"

        /** `ServerboundContainerClickPacket`, deliberately not decoded: see where it is handled. */
        const val CONTAINER_CLICK_ID = 0x12
    }
}

/** [org.basaltmc.protocol.configuration.ConfigurationKeepAlivePacket]'s id, matched here just to no-op it explicitly. */
private const val CONFIGURATION_KEEP_ALIVE_ID = 4

/** `ServerboundPlayerInputPacket`'s id, matched here just to no-op it explicitly. */
private const val SERVERBOUND_PLAYER_INPUT_ID = 0x2b

/**
 * The 10 registries in `RegistryDataLoader.SYNCHRONIZED_REGISTRIES` that nothing sent elsewhere in
 * [ClientSession.sendRegistriesThenFinish] populates yet. Sent with zero entries: matches the real
 * 29-registry synced set (see [VanillaRegistryData]'s KDoc) so the client's `RegistryAccess` has an
 * entry for each, which [VanillaTagData]'s tags for them need to resolve at all.
 */
private val EMPTY_SYNCHRONIZED_REGISTRIES =
    listOf(
        "minecraft:chat_type",
        "minecraft:trim_pattern",
        "minecraft:sulfur_cube_archetype",
        "minecraft:banner_pattern",
        "minecraft:enchantment",
        "minecraft:test_environment",
        "minecraft:test_instance",
        "minecraft:dialog",
        "minecraft:world_clock",
        "minecraft:timeline",
    )

private fun angleToByte(degrees: Float): Byte = ((degrees / 360f) * 256f).toInt().toByte()
