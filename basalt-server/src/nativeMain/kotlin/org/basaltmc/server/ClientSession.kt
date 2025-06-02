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
import org.basaltmc.protocol.play.ClientboundTakeItemEntityPacket
import org.basaltmc.protocol.play.CommandsPacket
import org.basaltmc.protocol.play.ContainerClosePacket
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
import org.basaltmc.protocol.play.OpenScreenPacket
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
import org.basaltmc.protocol.play.ServerboundContainerClickPacket
import org.basaltmc.protocol.play.ServerboundPickItemFromBlockPacket
import org.basaltmc.protocol.play.ServerboundPlayerAbilitiesPacket
import org.basaltmc.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.protocol.play.ServerboundPositionLookPacket
import org.basaltmc.protocol.play.ServerboundPositionPacket
import org.basaltmc.protocol.play.ServerboundSetCarriedItemPacket
import org.basaltmc.protocol.play.ServerboundSetCreativeModeSlotPacket
import org.basaltmc.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.protocol.play.SetEntityDataPacket
import org.basaltmc.protocol.play.SetExperiencePacket
import org.basaltmc.protocol.play.SetHealthPacket
import org.basaltmc.protocol.play.SetHeldSlotPacket
import org.basaltmc.protocol.play.SetSimulationDistancePacket
import org.basaltmc.protocol.play.SignedChatCommandPacket
import org.basaltmc.protocol.play.SpawnEntityPacket
import org.basaltmc.protocol.play.SpawnInfo
import org.basaltmc.protocol.play.SpawnPositionPacket
import org.basaltmc.protocol.play.SystemChatPacket
import org.basaltmc.protocol.play.UpdateLightPacket
import org.basaltmc.protocol.play.WireItemStack
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.status.PingRequestPacket
import org.basaltmc.protocol.status.PongResponsePacket
import org.basaltmc.protocol.status.StatusRequestPacket
import org.basaltmc.protocol.status.StatusResponsePacket
import org.basaltmc.protocol.writeVarInt
import org.basaltmc.random.LegacyRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.registry.ItemTools
import org.basaltmc.registry.Items
import org.basaltmc.server.command.CommandDispatcher
import org.basaltmc.server.command.PlayerCommandSource
import org.basaltmc.server.placeholder.PlaceholderSlots
import org.basaltmc.server.player.Difficulty
import org.basaltmc.server.player.FoodData
import org.basaltmc.server.player.Menu
import org.basaltmc.server.player.MenuSlot
import org.basaltmc.server.player.PlayerInventory
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
import org.basaltmc.server.world.RealBlockTags
import org.basaltmc.server.world.RealItemTags
import org.basaltmc.server.world.RealLootTables
import org.basaltmc.server.world.RealRecipes
import org.basaltmc.server.world.WorldState
import org.basaltmc.storage.PlayerStore
import org.basaltmc.storage.StoredItemStack
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
import org.basaltmc.world.crafting.CraftingInput
import kotlin.concurrent.AtomicInt
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid
import org.basaltmc.world.loot.ItemStack as LootItemStack

/**
 * Drives one client connection through Handshake, (Status | Login), Configuration and Play.
 *
 * Login is offline-mode when [onlineMode] is `null`; otherwise [HelloPacket]/[KeyPacket] negotiate
 * an AES-128/CFB8 secret and everything from [LoginCompressionPacket] on is enciphered.
 *
 * Play streams chunks through [chunkTracker] and wires movement, chat, breaking and placing
 * through [playerRegistry] and [WorldState]'s lock. This connection's thread also runs
 * [tickPlayer], [tryPickupNearbyItems], commands, death and respawn.
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
    /** Where a survival block break resolves its drops from. `null` means blocks never drop anything. */
    private val lootTables: RealLootTables? = null,
    /** Every crafting recipe the server knows. `null` means nothing can be crafted. */
    private val recipes: RealRecipes? = null,
    /** The real block tags, which decide whether the held tool is the right one. */
    private val blockTags: RealBlockTags? = null,
    /** The real item tags, sent to the client alongside the block ones. */
    private val itemTags: RealItemTags? = null,
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

    /** The skin layer mask from client settings, which arrive in Configuration before the player exists. */
    private var pendingSkinParts: Byte = PlayerSession.ALL_SKIN_PARTS
    private var playerSession: PlayerSession? = null

    /** Resolved in [enterPlay] from the player's stored world, falling back to the default. */
    private var currentWorld: LoadedWorld? = null

    private val world: WorldState get() = requireNotNull(currentWorld).state
    private val generationQueue: ChunkGenerationQueue get() = requireNotNull(currentWorld).generationQueue
    private val worldName: String get() = currentWorld?.name ?: ""

    /** The window this player has open. Their own inventory unless they opened a block's. */
    private var openMenu: Menu = Menu.playerInventory()

    /** The last container id handed out, so a freshly opened window never reuses the previous one. */
    private var nextContainerId = FIRST_BLOCK_CONTAINER_ID

    /** The crafting grid of [openMenu], indexed row-major. Sized for the largest grid any window has. */
    private val craftingGrid = arrayOfNulls<StoredItemStack>(MAX_GRID_SIZE)

    /** What [craftingGrid] currently makes, recomputed whenever it changes. */
    private var craftingResult: StoredItemStack? = null

    /**
     * What this player is holding on the cursor inside their open window, kept server-side rather
     * than taken from the client's own claim: trusting theirs is how a stack gets duplicated.
     */
    private var carriedItem: StoredItemStack? = null

    // The click-and-drag gesture in progress, if any. See [handleQuickCraft].
    private var quickCraftActive = false
    private var quickCraftType = 0
    private val quickCraftSlots = LinkedHashSet<Int>()

    /** Which dropped items this client has been told about. See [syncItemEntities]. */
    private val trackedItems = HashSet<Int>()

    /** Which of those have already had their final resting position sent. */
    private val settledItems = HashSet<Int>()

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
                        timings.of("player.items").time { syncItemEntities() }
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
            ClientInformationPacket.ID -> {
                // Read even in Configuration, before the player exists: it is where the skin layer
                // mask arrives, and it is the only thing this build takes from client settings.
                pendingSkinParts = ClientInformationPacket.read(body).skinParts
            }
            CONFIGURATION_KEEP_ALIVE_ID -> Unit
        }
    }

    private fun sendRegistriesThenFinish() {
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:world_clock",
                VanillaRegistryData.worldClocks.map { (path, value) -> RegistryEntry("minecraft:$path", value) },
            ).write(it)
        }
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:dimension_type",
                VanillaRegistryData.dimensionTypes.map { (path, value) ->
                    RegistryEntry("minecraft:$path", value)
                },
            ).write(it)
        }
        // The full vanilla set in one packet, in Biomes.REGISTRY order: each entry's
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
        // Client bootstrap looks tags up by key whether or not the tagged registry is loaded: a
        // real client failed on damage_type's is_fire with no `tags` packet sent at all. So every
        // tag name the 26.2 jar ships goes out (see VanillaTagData), existing being enough there.
        //
        // Members matter separately: the client evaluates its own tool rules against them, and an
        // empty `mineable/pickaxe` means no rule matches and every tool mines at bare-hand speed.
        // Known members are sent, the rest keep the empty-but-present shape.
        sendCompressed(TagsPacket.ID) {
            TagsPacket(
                VanillaTagData.tagsByRegistry.map { (registryId, tagNames) ->
                    TagGroup(registryId, tagNames.map { TagEntry(it, tagMembers(registryId, it)) })
                },
            ).write(it)
        }
        sendCompressed(FinishConfigurationPacket.ID) { }
    }

    /**
     * The numeric members of one tag, or empty when this build has no real data for that registry.
     *
     * Members are registry ids of the tagged thing: a block tag carries `minecraft:block` ids, not
     * block *state* ids, which are a different numbering entirely.
     */
    private fun tagMembers(
        registryId: String,
        tagName: String,
    ): List<Int> =
        when (registryId) {
            "minecraft:block" ->
                blockTags
                    ?.membersOf(tagName)
                    .orEmpty()
                    .mapNotNull(Blocks::blockIdOf)
            "minecraft:item" ->
                itemTags
                    ?.membersOf(tagName)
                    .orEmpty()
                    .mapNotNull { Items.REGISTRY.byIdentifier(it)?.protocolId }
            else -> emptyList()
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
        session.profileProperties = pendingProfile?.properties.orEmpty()
        session.skinParts = pendingSkinParts
        logger.info { "${session.name} joined the game" }

        sendCompressed(PlayerInfoUpdatePacket.ID) {
            PlayerInfoUpdatePacket
                .initializing(
                    others.map {
                        PlayerInfoEntry(
                            it.uuid,
                            it.name,
                            listed = true,
                            gamemode = it.gamemode,
                            properties = it.profileProperties,
                        )
                    } +
                        PlayerInfoEntry(
                            session.uuid,
                            session.name,
                            listed = true,
                            gamemode = session.gamemode,
                            properties = session.profileProperties,
                        ),
                ).write(it)
        }

        // Their own entity, not just everyone else's. A client reads its *own* skin layers from
        // the synced metadata as well (`Avatar.isModelPartShown`), so without this the player sees
        // themselves in third person with no hat, jacket or sleeves while everyone else sees them
        // correctly.
        sendCompressed(SetEntityDataPacket.ID) {
            SetEntityDataPacket.skinParts(session.entityId, session.skinParts).write(it)
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
                        listOf(
                            PlayerInfoEntry(
                                session.uuid,
                                session.name,
                                listed = true,
                                gamemode = session.gamemode,
                                properties = session.profileProperties,
                            ),
                        ),
                    ).write(it)
            }
            other.connection.sendPlayPacket(SpawnEntityPacket.ID) { buffer ->
                spawnPlayerPacket(session).write(buffer)
            }
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { buffer ->
                SetEntityDataPacket.skinParts(session.entityId, session.skinParts).write(buffer)
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
            sendCompressed(SpawnEntityPacket.ID) { buffer -> spawnPlayerPacket(other).write(buffer) }
            sendCompressed(SetEntityDataPacket.ID) { buffer ->
                SetEntityDataPacket.skinParts(other.entityId, other.skinParts).write(buffer)
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
            dimension = currentWorld?.declaration?.dimension?.registryIndex ?: 0,
            dimensionName = currentWorld?.levelKey ?: "minecraft:overworld",
            hashedSeed = 0L,
            gamemode = gamemode,
            previousGamemode = 255,
            isDebug = false,
            isFlat = currentWorld?.declaration?.type == org.basaltmc.server.config.WorldType.FLAT,
            death = null,
            portalCooldown = 0,
            seaLevel = currentWorld?.declaration?.dimension?.seaLevel ?: 63,
        )

    private fun sendChunk(chunk: Chunk) {
        val chunkData = ChunkDataCodec.encodeSections(chunk)
        val light = ChunkDataCodec.encodeLight(chunk, currentWorld?.declaration?.dimension?.hasSkylight ?: true)
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
            ServerboundSetCreativeModeSlotPacket.ID ->
                handleSetCreativeModeSlot(ServerboundSetCreativeModeSlotPacket.read(body))
            ServerboundContainerClickPacket.ID -> handleContainerClick(ServerboundContainerClickPacket.read(body))
            ContainerClosePacket.SERVERBOUND_ID -> {
                ContainerClosePacket.read(body)
                playerSession?.let(::closeOpenMenu)
            }
            ServerboundUseItemOnPacket.ID -> handleUseItemOn(ServerboundUseItemOnPacket.read(body))
            ServerboundPickItemFromBlockPacket.ID ->
                handlePickItemFromBlock(
                    ServerboundPickItemFromBlockPacket.read(body),
                )
            SERVERBOUND_CLIENT_INFORMATION_ID -> {
                // parity: `ServerPlayer.updateOptions`, which re-reads this every time the player
                // touches their video settings, not just once at login. Toggling a skin layer has
                // to reach everyone who can see them, this player included.
                val session = playerSession
                val skinParts = ClientInformationPacket.read(body).skinParts
                pendingSkinParts = skinParts
                if (session != null && session.skinParts != skinParts) {
                    session.skinParts = skinParts
                    val metadata = SetEntityDataPacket.skinParts(session.entityId, skinParts)
                    sendPlayPacket(SetEntityDataPacket.ID) { metadata.write(it) }
                    for (player in playersInSameWorld()) {
                        if (player.uuid == session.uuid) continue
                        player.connection.sendPlayPacket(SetEntityDataPacket.ID) { metadata.write(it) }
                    }
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
            -> Unit
            // `player_input`: the movement keys currently held, sent on every change. Sneaking
            // lives here now (it left `player_command`), and it decides whether right-clicking a
            // crafting table opens it or places against it.
            SERVERBOUND_PLAYER_INPUT_ID -> {
                val inputs = body.readByte().toInt()
                playerSession?.sneaking = (inputs and SNEAK_INPUT_FLAG) != 0
            }
        }
    }

    /**
     * Breaking a block, on whichever packet the player's game mode breaks on.
     *
     * // parity: ServerPlayerGameMode. Creative destroys on `START_DESTROY_BLOCK`, survival times
     * the dig client-side and sends `STOP_DESTROY_BLOCK`; matching that split is what keeps the
     * client's prediction in step. Adventure and spectator break nothing, as in vanilla.
     *
     * Survival's timing is trusted rather than re-derived from hardness and tool, so a client that
     * finishes early is believed.
     */
    private fun handlePlayerAction(action: ServerboundPlayerActionPacket) {
        when (action.action) {
            ServerboundPlayerActionPacket.Action.DROP_ITEM -> dropHeldItem(all = false)
            ServerboundPlayerActionPacket.Action.DROP_ALL_ITEMS -> dropHeldItem(all = true)
            else -> Unit
        }

        val gamemode = playerSession?.gamemode ?: defaultGamemode
        val targetHardness =
            world
                .getBlockState(action.pos.x, action.pos.y, action.pos.z)
                ?.let(Blocks::byStateId)
                ?.let { Blocks.hardnessOf(it.block) }
        val breaksOn =
            when (gamemode) {
                Gamemode.CREATIVE -> ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK
                Gamemode.SURVIVAL ->
                    // A block that comes apart instantly is destroyed by the client the moment the
                    // dig starts, and it never sends the end of one: waiting for `STOP` there is why
                    // flowers, grass and leaves could not be broken in survival at all.
                    if (targetHardness?.isInstant == true) {
                        ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK
                    } else {
                        ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK
                    }
                Gamemode.ADVENTURE, Gamemode.SPECTATOR -> null
            }
        // Bedrock and the portal frames stay put for anyone who is not in creative.
        if (gamemode == Gamemode.SURVIVAL && targetHardness?.isUnbreakable == true) {
            sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(action.sequence).write(it)
            }
            return
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
        val brokenBlock = broken?.let(Blocks::byStateId)
        brokenBlock?.let { playerSession?.stats?.increment(StoredStats.mined(it.block.toString())) }

        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(action.pos, Blocks.AIR.stateId).write(it)
            }
        }

        if (gamemode == Gamemode.SURVIVAL && brokenBlock != null) {
            // parity: `Player.hasCorrectToolForDrops`. A block marked as needing the right tool
            // gives nothing to the wrong one, which is what makes punching stone a waste of time
            // rather than a free cobblestone.
            val heldItem =
                playerSession
                    ?.inventory
                    ?.heldItem
                    ?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
            val needsTool = Blocks.hardnessOf(brokenBlock.block)?.requiresCorrectTool == true
            val correctTool =
                !needsTool ||
                    ItemTools.isCorrectForDrops(heldItem, brokenBlock.block) { tag, block ->
                        blockTags?.contains(tag, block) == true
                    }
            if (correctTool) dropLootFor(brokenBlock.block, action.pos)
        }
    }

    /**
     * `Q`/ctrl+`Q`: tosses the held stack (or all of it) into the world. parity:
     * `ServerPlayerGameMode` reads this off the same `DROP_ITEM`/`DROP_ALL_ITEMS`
     * [ServerboundPlayerActionPacket] this build does. Real vanilla arcs the toss forward from eye
     * height; this build drops it straight down at the player's own feet, a documented
     * simplification (see [WorldState.DroppedItem]'s own KDoc: nothing here ticks item physics).
     */
    private fun dropHeldItem(all: Boolean) {
        val session = playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val dropCount = if (all) held.count else 1
        val remaining = held.count - dropCount
        session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
        sendInventorySlot(heldSlot)

        // parity: `Player.drop` throws it forward from eye height along the look direction, rather
        // than dropping it on the player's own feet where they would instantly walk back into it.
        val yaw = session.yaw * PI / 180.0
        val pitch = session.pitch * PI / 180.0
        spawnDroppedItem(
            item = Identifier.parse(held.id),
            count = dropCount,
            x = session.x,
            y = session.y + THROW_EYE_HEIGHT,
            z = session.z,
            velocityX = -sin(yaw) * cos(pitch) * THROW_SPEED,
            velocityY = -sin(pitch) * THROW_SPEED + THROW_LIFT,
            velocityZ = cos(yaw) * cos(pitch) * THROW_SPEED,
        )
    }

    /**
     * Rolls [block]'s loot table (see [RealLootTables]) and drops each resulting stack, snapped to
     * the ground below [pos] rather than left floating in the hole the break just made.
     *
     * The roll itself is not RNG-parity with vanilla: a real block break consumes the world's own
     * `RandomSequences` entry (`minecraft:blocks/<path>`, `LootTable.getDefaultParamSet`'s own
     * `RandomSource`), so the exact same seed and break order would not currently reproduce
     * vanilla's drops. A documented gap, not a silent one: see [LegacyRandom]'s use below.
     */
    private fun dropLootFor(
        block: Identifier,
        pos: BlockPos,
    ) {
        val table = lootTables?.forBlock(block) ?: return
        val drops = table.generate(LegacyRandom(Random.nextLong()))
        if (drops.isEmpty()) return
        for (stack in drops) {
            // Falls under its own gravity now ([WorldState.tickItems]) instead of being snapped to
            // the ground up front, so a block broken in mid-air drops as it should.
            spawnDroppedItem(
                item = stack.item,
                count = stack.count,
                x = pos.x + 0.5,
                y = pos.y + 0.5,
                z = pos.z + 0.5,
                velocityY = BREAK_DROP_LIFT,
                pickupDelay = BREAK_DROP_PICKUP_DELAY,
            )
        }
    }

    private fun isAirLike(stateId: Int): Boolean =
        stateId == Blocks.AIR.stateId || stateId == Blocks.CAVE_AIR.stateId || stateId == Blocks.VOID_AIR.stateId

    /** Puts a new item entity into the world and tells everyone who can currently see it. */
    private fun spawnDroppedItem(
        item: Identifier,
        count: Int,
        x: Double,
        y: Double,
        z: Double,
        velocityX: Double = 0.0,
        velocityY: Double = 0.0,
        velocityZ: Double = 0.0,
        pickupDelay: Int = WorldState.DEFAULT_PICKUP_DELAY,
    ) {
        if (count <= 0) return
        if (Items.REGISTRY.byIdentifier(item) == null) return
        val entityId = playerRegistry?.allocateEntityId() ?: return
        // Only put into the world here. Telling anyone about it is [syncItemEntities]'s job, on
        // each connection's own thread: broadcasting from here reached whoever happened to be
        // online at the instant of the drop and nobody who walked up afterwards.
        world.spawnItem(
            WorldState.DroppedItem(
                entityId = entityId,
                uuid = Uuid.random(),
                item = item,
                count = count,
                x = x,
                y = y,
                z = z,
                velocityX = velocityX,
                velocityY = velocityY,
                velocityZ = velocityZ,
                pickupDelay = pickupDelay,
            ),
        )
    }

    /**
     * Brings this client's view of the dropped items up to date: spawns what came into range, moves
     * what is still falling, forgets what was picked up, despawned or left.
     *
     * Physics happen once for the world in [WorldState.tickItems]; this only reports them, so an
     * item falls at the same speed however many players watch. A settled item
     * ([WorldState.DroppedItem.resting]) stops generating packets, which is what makes a floor
     * covered in drops free to look at.
     */
    private fun syncItemEntities() {
        val session = playerSession ?: return
        val visible = HashSet<Int>()
        for (item in world.items()) {
            val dx = item.x - session.x
            val dz = item.z - session.z
            if (dx * dx + dz * dz > ITEM_VIEW_RADIUS_SQUARED) continue
            val protocolId = Items.REGISTRY.byIdentifier(item.item)?.protocolId ?: continue
            visible += item.entityId
            if (trackedItems.add(item.entityId)) {
                sendPlayPacket(SpawnEntityPacket.ID) {
                    SpawnEntityPacket(
                        entityId = item.entityId,
                        uuid = item.uuid,
                        entityType = SpawnEntityPacket.ITEM_ENTITY_TYPE,
                        x = item.x,
                        y = item.y,
                        z = item.z,
                        velocityX = item.velocityX,
                        velocityY = item.velocityY,
                        velocityZ = item.velocityZ,
                    ).write(it)
                }
                sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket.item(item.entityId, WireItemStack(protocolId, item.count)).write(it)
                }
            } else if (item.resting && settledItems.add(item.entityId)) {
                // One correction when it stops, and nothing while it falls.
                //
                // A real client simulates a dropped item itself from the velocity the spawn packet
                // carried, and it interpolates. Sending an absolute teleport every tick on top of
                // that overrides its simulation twenty times a second with positions computed on a
                // *different* 20 Hz loop than the one that produced them, so the two drift against
                // each other and the item visibly stutters. Vanilla only syncs when something the
                // client could not have predicted happens; this syncs the one thing that matters,
                // where it came to rest.
                sendPlayPacket(EntityTeleportPacket.ID) {
                    EntityTeleportPacket(
                        entityId = item.entityId,
                        x = item.x,
                        y = item.y,
                        z = item.z,
                        yaw = 0f,
                        pitch = 0f,
                        onGround = true,
                    ).write(it)
                }
            }
        }

        val gone = trackedItems.filterNot { it in visible }
        if (gone.isEmpty()) return
        trackedItems.removeAll(gone.toSet())
        settledItems.removeAll(gone.toSet())
        sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
    }

    /**
     * Whatever [WorldState.DroppedItem]s are close enough to this player right now, added to their
     * inventory and removed from the world. Driven from this connection's own outbound loop
     * ([tickPlayer], see [startOutboundLoop]'s own KDoc for why): the shared tick loop touches no
     * sockets, and picking something up always ends in one (an inventory update, at least).
     */
    private fun tryPickupNearbyItems() {
        val session = playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        for (candidate in world.nearbyItems(session.x, session.y, session.z, PICKUP_RADIUS)) {
            // parity: a player's own toss cannot be picked straight back up (`ItemEntity.pickupDelay`).
            if (!candidate.canBePickedUp) continue
            val removed = world.removeItem(candidate.entityId) ?: continue
            val leftover = session.inventory.add(StoredItemStack(removed.item.toString(), removed.count))
            val picked = removed.count - leftover
            if (picked <= 0) {
                // Nowhere for it to go: put it back exactly as it was, nothing happened.
                world.spawnItem(removed)
                continue
            }
            sendInventoryAll()
            if (leftover > 0) {
                // Only some of it fit: the same entity, fewer items, still sitting where it was.
                removed.count = leftover
                world.spawnItem(removed)
                val protocolId = Items.REGISTRY.byIdentifier(removed.item)?.protocolId
                if (protocolId != null) {
                    for (player in playersInSameWorld()) {
                        player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                            SetEntityDataPacket.item(removed.entityId, WireItemStack(protocolId, leftover)).write(it)
                        }
                    }
                }
            }
            // The pickup animation only: the entity itself is removed from every watcher's view by
            // their own [syncItemEntities], since it is no longer in the world.
            for (player in playersInSameWorld()) {
                player.connection.sendPlayPacket(ClientboundTakeItemEntityPacket.ID) {
                    ClientboundTakeItemEntityPacket(removed.entityId, session.entityId, picked).write(it)
                }
            }
        }
    }

    /**
     * Placing a block: resolves the held item to a block state by matching identifiers (so only
     * plain blocks whose item shares the block's own name place correctly), then places it in the
     * empty space [ServerboundUseItemOnPacket.blockHit] points at.
     *
     * parity: `ServerPlayerGameMode.useItemOn`. Oriented placement (stairs, logs, slabs facing the
     * player), off-hand placing, and interacting with a block instead of placing on it (opening a
     * container, flipping a lever) are all documented gaps: every block places in its default
     * state, only the main hand is read, and this build has no block-entity/container-interaction
     * model yet.
     */
    private fun handleUseItemOn(useItemOn: ServerboundUseItemOnPacket) {
        fun ack() {
            sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(useItemOn.sequence).write(it)
            }
        }

        val session = playerSession
        if (session == null || useItemOn.hand != ServerboundUseItemOnPacket.Hand.MAIN_HAND) return ack()
        if (session.gamemode == Gamemode.SPECTATOR || session.gamemode == Gamemode.ADVENTURE) return ack()

        val hit = useItemOn.blockHit
        // Interacting with the block comes first, and has to: the usual way to open a crafting
        // table is to right-click it with an empty hand, and the held-item checks below reject
        // exactly that. parity: `ServerPlayerGameMode.useItemOn` runs the block's own use before it
        // considers the item as something to place.
        //
        // Sneaking overrides it and places instead, which is how vanilla lets you put a block on
        // top of a table rather than opening it.
        if (!session.sneaking) {
            val clicked = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)?.let(Blocks::byStateId)
            if (clicked?.block == CRAFTING_TABLE) {
                openCraftingTable(session)
                return ack()
            }
        }

        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val blockStateId = held?.let { Blocks.REGISTRY.idOf(Identifier.parse(it.id)) }
        if (held == null || blockStateId == null) return ack()

        val placeAt = if (hit.inside) hit.pos else hit.pos.offset(hit.direction)
        val existing = world.getBlockState(placeAt.x, placeAt.y, placeAt.z)
        if (existing == null || !isAirLike(existing)) return ack()

        val changed = world.setBlockState(placeAt.x, placeAt.y, placeAt.z, blockStateId)
        ack()
        if (!changed) return

        if (session.gamemode == Gamemode.SURVIVAL) {
            val remaining = held.count - 1
            session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
            sendInventorySlot(heldSlot)
        }

        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(placeAt, blockStateId).write(it)
            }
        }
    }

    /**
     * Opens a crafting table's 3x3 window.
     *
     * The container id has to change every time a window is opened: the client tags every click
     * with it, and reusing one leaves clicks meant for a window that has already closed being
     * applied to the next one.
     */
    private fun openCraftingTable(session: PlayerSession) {
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.craftingTable(nextContainerId)
        recomputeCraftingResult()
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.CRAFTING_MENU,
                title = Component.translatable("container.crafting"),
            ).write(it)
        }
        sendInventory()
    }

    /** Back to the player's own window, giving them back whatever was on the grid or the cursor. */
    private fun closeOpenMenu(session: PlayerSession) {
        if (openMenu.containerId == 0) {
            // Their own inventory closing still has a 2x2 grid to empty.
            returnCraftingGrid(session)
            sendInventory()
            return
        }
        returnCraftingGrid(session)
        openMenu = Menu.playerInventory()
        recomputeCraftingResult()
        sendInventory()
    }

    /** Reads one slot of the open window, whatever it refers to. */
    private fun readSlot(
        session: PlayerSession,
        slot: MenuSlot,
    ): StoredItemStack? =
        when (slot) {
            is MenuSlot.Inventory -> session.inventory[slot.index]
            is MenuSlot.Grid -> craftingGrid.getOrNull(slot.index)
            MenuSlot.Result -> craftingResult
        }

    /** Writes one slot of the open window. The result slot is computed, never assigned. */
    private fun writeSlot(
        session: PlayerSession,
        slot: MenuSlot,
        stack: StoredItemStack?,
    ) {
        when (slot) {
            is MenuSlot.Inventory -> session.inventory[slot.index] = stack
            is MenuSlot.Grid -> {
                if (slot.index in craftingGrid.indices) craftingGrid[slot.index] = stack
                recomputeCraftingResult()
            }
            MenuSlot.Result -> Unit
        }
    }

    /** Whether a stack may be put into this slot at all. The result slot only ever gives. */
    private fun canPlaceInto(slot: MenuSlot): Boolean = slot != MenuSlot.Result

    /**
     * Recomputes what the grid makes. Called after anything touches it, which is what makes the
     * result appear and disappear as the player lays ingredients out.
     */
    private fun recomputeCraftingResult() {
        val recipes = this.recipes
        if (recipes == null) {
            craftingResult = null
            return
        }
        val width = openMenu.gridWidth
        val height = openMenu.gridHeight
        val cells =
            List(width * height) { index ->
                craftingGrid.getOrNull(index)?.let { LootItemStack(Identifier.parse(it.id), it.count) }
            }
        val result = recipes.resultFor(CraftingInput.of(width, height, cells))
        craftingResult = result?.let { StoredItemStack(it.item.toString(), it.count) }
    }

    /**
     * Taking the result: hands over one batch and consumes one item from every occupied grid cell.
     *
     * parity: `ResultSlot.onTake`. Crafting is only ever committed here, when the player actually
     * takes it, so looking at a result costs nothing and closing the window mid-recipe loses no
     * ingredients.
     */
    private fun takeCraftingResult(session: PlayerSession) {
        val result = craftingResult ?: return
        val carried = carriedItem
        if (carried != null) {
            // The cursor has to be empty or hold the same thing, and have room for a whole batch.
            if (carried.id != result.id || carried.components != result.components) return
            if (carried.count + result.count > PlayerInventory.MAX_STACK) return
            carriedItem = carried.copy(count = carried.count + result.count)
        } else {
            carriedItem = result
        }
        for (index in 0 until openMenu.gridSize) {
            val cell = craftingGrid.getOrNull(index) ?: continue
            val left = cell.count - 1
            craftingGrid[index] = if (left <= 0) null else cell.copy(count = left)
        }
        recomputeCraftingResult()
    }

    /**
     * Empties the crafting grid back into the player, dropping whatever will not fit.
     *
     * parity: `AbstractContainerMenu.removed`/`clearContainer`. Ingredients left on the grid when a
     * window closes are never destroyed, which is the whole reason this exists.
     */
    private fun returnCraftingGrid(session: PlayerSession) {
        for (index in craftingGrid.indices) {
            val cell = craftingGrid[index] ?: continue
            craftingGrid[index] = null
            val leftover = session.inventory.add(cell)
            if (leftover > 0) dropStack(session, cell.copy(count = leftover))
        }
        // The cursor goes the same way: it is not part of the inventory either.
        carriedItem?.let { held ->
            carriedItem = null
            val leftover = session.inventory.add(held)
            if (leftover > 0) dropStack(session, held.copy(count = leftover))
        }
        craftingResult = null
    }

    /**
     * A click inside the player's own window.
     *
     * // parity: AbstractContainerMenu.doClick, reduced to the modes that move items between slots.
     * The cursor is the server's ([carriedItem]), not the client's predicted one, so claiming to
     * hold a stack cannot duplicate it: the click is re-derived and the window sent back.
     *
     * The 2x2 crafting grid and its result slot are not modelled: [PlayerInventory.storageSlotOf]
     * answers `null` there, so those clicks change nothing.
     */
    private fun handleContainerClick(packet: ServerboundContainerClickPacket) {
        val session = playerSession ?: return
        if (packet.containerId != openMenu.containerId) return sendInventory()

        when (packet.mode) {
            ServerboundContainerClickPacket.Mode.PICKUP -> {
                // Slot -999 is the click outside the window: whatever is on the cursor is thrown.
                if (packet.slot == OUTSIDE_WINDOW_SLOT) {
                    val carried = carriedItem ?: return sendInventory()
                    val thrown = if (packet.button == RIGHT_BUTTON) 1 else carried.count
                    dropStack(session, carried.copy(count = thrown))
                    val left = carried.count - thrown
                    carriedItem = if (left <= 0) null else carried.copy(count = left)
                    return sendInventory()
                }
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                if (slot == MenuSlot.Result) {
                    takeCraftingResult(session)
                } else {
                    pickupClick(session, slot, half = packet.button == RIGHT_BUTTON)
                }
            }
            ServerboundContainerClickPacket.Mode.SWAP -> {
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                if (!canPlaceInto(slot)) return sendInventory()
                if (packet.button !in 0 until PlayerInventory.HOTBAR_SIZE) return sendInventory()
                val hotbar = MenuSlot.Inventory(PlayerInventory.HOTBAR_START + packet.button)
                val fromSlot = readSlot(session, slot)
                writeSlot(session, slot, readSlot(session, hotbar))
                writeSlot(session, hotbar, fromSlot)
            }
            ServerboundContainerClickPacket.Mode.THROW -> {
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                if (slot == MenuSlot.Result) {
                    // Throwing the result still has to craft it, ingredients and all.
                    val previouslyCarried = carriedItem
                    carriedItem = null
                    takeCraftingResult(session)
                    carriedItem?.let { dropStack(session, it) }
                    carriedItem = previouslyCarried
                } else {
                    val stack = readSlot(session, slot) ?: return sendInventory()
                    // Button 0 is `Q`, button 1 is ctrl+`Q`: one item or the whole stack.
                    val thrown = if (packet.button == 0) 1 else stack.count
                    dropStack(session, stack.copy(count = thrown))
                    val left = stack.count - thrown
                    writeSlot(session, slot, if (left <= 0) null else stack.copy(count = left))
                }
            }
            ServerboundContainerClickPacket.Mode.QUICK_MOVE -> {
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                quickMove(session, slot)
            }
            ServerboundContainerClickPacket.Mode.QUICK_CRAFT -> {
                // Mid-gesture stages answer with nothing: resending the window while the player is
                // still dragging is what makes the drag snap back under their cursor.
                if (!handleQuickCraft(session, packet)) return
            }
            ServerboundContainerClickPacket.Mode.CLONE -> {
                // parity: creative's own middle-click, and only with an empty cursor.
                if (session.gamemode != Gamemode.CREATIVE || carriedItem != null) return sendInventory()
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                val stack = readSlot(session, slot) ?: return sendInventory()
                carriedItem = stack.copy(count = PlayerInventory.MAX_STACK)
            }
            ServerboundContainerClickPacket.Mode.PICKUP_ALL -> {
                collectMatching(session, reversed = packet.button != 0)
            }
        }
        sendInventory()
    }

    /**
     * Double-click: sweeps matching stacks off the inventory and onto the cursor until it is full.
     *
     * parity: `AbstractContainerMenu.doClick`'s PICKUP_ALL branch, two passes and all. The first
     * pass deliberately skips already-full stacks so the loose ones are consumed first, which is
     * what makes double-clicking tidy an inventory up instead of just shuffling full stacks
     * around; the second pass takes whatever is still needed.
     */
    private fun collectMatching(
        session: PlayerSession,
        reversed: Boolean,
    ) {
        val carried = carriedItem ?: return
        var total = carried.count
        if (total >= PlayerInventory.MAX_STACK) return

        // Every slot of the *open window*, not just the player's own inventory: a crafting table
        // shows the grid too, and vanilla's own sweep gathers from everything the player can see.
        // Sweeping the inventory alone is why this did nothing useful with a table open.
        val slots =
            (0 until openMenu.slotCount)
                .mapNotNull(openMenu::slotAt)
                .filter { it != MenuSlot.Result }
                .let { if (reversed) it.reversed() else it }

        for (pass in 0..1) {
            for (slot in slots) {
                if (total >= PlayerInventory.MAX_STACK) break
                val existing = readSlot(session, slot) ?: continue
                if (existing.id != carried.id || existing.components != carried.components) continue
                if (pass == 0 && existing.count >= PlayerInventory.MAX_STACK) continue
                val taken = minOf(PlayerInventory.MAX_STACK - total, existing.count)
                total += taken
                val left = existing.count - taken
                writeSlot(session, slot, if (left <= 0) null else existing.copy(count = left))
            }
        }
        carriedItem = carried.copy(count = total)
    }

    /**
     * The click-and-drag gesture, in its three stages. Returns whether to send the window back:
     * only the last stage changes anything, and answering the others fights the drag in progress.
     *
     * // parity: AbstractContainerMenu.doClick's QUICK_CRAFT branch, bit layout included
     * (`getQuickcraftHeader` is `mask and 3`, `getQuickcraftType` is `mask shr 2 and 3`, the
     * opposite way round from how it reads), and its even share, single item or full stack split.
     * The client sends this for most drag-and-drop, so ignoring it left every later click acting
     * on a cursor the player had let go of.
     */
    private fun handleQuickCraft(
        session: PlayerSession,
        packet: ServerboundContainerClickPacket,
    ): Boolean {
        val stage = packet.button and 3
        val type = (packet.button shr 2) and 3
        val carried = carriedItem

        when {
            stage == QUICK_CRAFT_START -> {
                quickCraftType = type
                quickCraftSlots.clear()
                quickCraftActive = carried != null
                return false
            }
            stage == QUICK_CRAFT_ADD && quickCraftActive -> {
                val slot = openMenu.slotAt(packet.slot) ?: return false
                if (!canPlaceInto(slot)) return false
                val existing = readSlot(session, slot)
                val stackable =
                    carried != null &&
                        (existing == null || (existing.id == carried.id && existing.components == carried.components))
                if (stackable) quickCraftSlots += packet.slot
                return false
            }
            stage == QUICK_CRAFT_END && quickCraftActive -> {
                distributeQuickCraft(session)
                quickCraftActive = false
                quickCraftSlots.clear()
                return true
            }
            else -> {
                // Any other order is an interrupted gesture: forget it and resync.
                quickCraftActive = false
                quickCraftSlots.clear()
                return true
            }
        }
    }

    /** Spreads the cursor stack over the slots the drag collected. See [handleQuickCraft]. */
    private fun distributeQuickCraft(session: PlayerSession) {
        val carried = carriedItem ?: return
        val targets = quickCraftSlots.mapNotNull(openMenu::slotAt).filter(::canPlaceInto)
        if (targets.isEmpty()) return

        // parity: a one-slot drag is just a click on that slot, cursor rules and all.
        if (targets.size == 1) {
            pickupClick(session, targets.single(), half = quickCraftType == QUICK_CRAFT_TYPE_RIGHT)
            return
        }

        val perSlot =
            when (quickCraftType) {
                QUICK_CRAFT_TYPE_LEFT -> carried.count / targets.size
                QUICK_CRAFT_TYPE_RIGHT -> 1
                else -> PlayerInventory.MAX_STACK
            }
        if (perSlot <= 0) return

        var remaining = carried.count
        for (slot in targets) {
            if (remaining <= 0) break
            val already = readSlot(session, slot)?.count ?: 0
            val give = minOf(perSlot, PlayerInventory.MAX_STACK - already, remaining)
            if (give <= 0) continue
            writeSlot(session, slot, carried.copy(count = already + give))
            remaining -= give
        }
        // A middle-button drag is creative's own "fill them all" and costs the player nothing.
        carriedItem =
            when {
                quickCraftType == QUICK_CRAFT_TYPE_MIDDLE && session.gamemode == Gamemode.CREATIVE -> carried
                remaining <= 0 -> null
                else -> carried.copy(count = remaining)
            }
    }

    /**
     * Left/right click on a slot, against the cursor. parity: `AbstractContainerMenu.doClick`'s own
     * PICKUP branch: an empty cursor takes the slot (half of it on a right click), a loaded cursor
     * merges into a matching stack or swaps with a different one.
     */
    private fun pickupClick(
        session: PlayerSession,
        slot: MenuSlot,
        half: Boolean,
    ) {
        val inSlot = readSlot(session, slot)
        val carried = carriedItem
        when {
            carried == null -> {
                if (inSlot == null) return
                val taken = if (half) (inSlot.count + 1) / 2 else inSlot.count
                carriedItem = inSlot.copy(count = taken)
                val left = inSlot.count - taken
                writeSlot(session, slot, if (left <= 0) null else inSlot.copy(count = left))
            }
            !canPlaceInto(slot) -> Unit
            inSlot == null -> {
                val placed = if (half) 1 else carried.count
                writeSlot(session, slot, carried.copy(count = placed))
                val left = carried.count - placed
                carriedItem = if (left <= 0) null else carried.copy(count = left)
            }
            inSlot.id == carried.id && inSlot.components == carried.components -> {
                val room = PlayerInventory.MAX_STACK - inSlot.count
                if (room <= 0) return
                val moved = if (half) minOf(1, room, carried.count) else minOf(room, carried.count)
                writeSlot(session, slot, inSlot.copy(count = inSlot.count + moved))
                val left = carried.count - moved
                carriedItem = if (left <= 0) null else carried.copy(count = left)
            }
            else -> {
                // Two different items: they trade places, cursor for slot.
                writeSlot(session, slot, carried)
                carriedItem = inSlot
            }
        }
    }

    /**
     * Shift-click. parity: `Inventory`'s own halves, so a stack in the hotbar goes up into the
     * storage rows and one in the rows comes down to the hotbar, merging into matching stacks first.
     */
    private fun quickMove(
        session: PlayerSession,
        slot: MenuSlot,
    ) {
        // Shift-clicking the result crafts as many times as the grid allows, which is what makes it
        // the fast way to turn a stack of logs into planks.
        if (slot == MenuSlot.Result) {
            val previouslyCarried = carriedItem
            carriedItem = null
            while (craftingResult != null) {
                val batch = craftingResult ?: break
                if (session.inventory.add(batch) > 0) break
                carriedItem = null
                takeCraftingResult(session)
                carriedItem = null
            }
            carriedItem = previouslyCarried
            return
        }

        val stack = readSlot(session, slot) ?: return
        // Anything outside the inventory proper (the grid) comes back into it; inside it, the two
        // halves swap, hotbar to storage and storage to hotbar.
        val targets =
            when {
                slot !is MenuSlot.Inventory ->
                    (PlayerInventory.HOTBAR_START until PlayerInventory.MAIN_SLOTS) +
                        (0 until PlayerInventory.HOTBAR_START)
                slot.index < PlayerInventory.HOTBAR_START ->
                    (
                        PlayerInventory.HOTBAR_START until
                            PlayerInventory.MAIN_SLOTS
                    ).toList()
                else -> (0 until PlayerInventory.HOTBAR_START).toList()
            }

        var remaining = stack.count
        for (target in targets) {
            if (remaining <= 0) break
            val existing = session.inventory[target] ?: continue
            if (existing.id != stack.id || existing.components != stack.components) continue
            val room = PlayerInventory.MAX_STACK - existing.count
            if (room <= 0) continue
            val moved = minOf(room, remaining)
            session.inventory[target] = existing.copy(count = existing.count + moved)
            remaining -= moved
        }
        for (target in targets) {
            if (remaining <= 0) break
            if (session.inventory[target] != null) continue
            session.inventory[target] = stack.copy(count = remaining)
            remaining = 0
        }
        writeSlot(session, slot, if (remaining <= 0) null else stack.copy(count = remaining))
    }

    /** Throws a stack into the world from this player, the same arc `Q` uses. */
    private fun dropStack(
        session: PlayerSession,
        stack: StoredItemStack,
    ) {
        val yaw = session.yaw * PI / 180.0
        val pitch = session.pitch * PI / 180.0
        spawnDroppedItem(
            item = Identifier.parse(stack.id),
            count = stack.count,
            x = session.x,
            y = session.y + THROW_EYE_HEIGHT,
            z = session.z,
            velocityX = -sin(yaw) * cos(pitch) * THROW_SPEED,
            velocityY = -sin(pitch) * THROW_SPEED + THROW_LIFT,
            velocityZ = cos(yaw) * cos(pitch) * THROW_SPEED,
        )
    }

    /**
     * Serverbound, id 0x38: creative inventory editing, and the only path a creative client's
     * dragged item takes, including discarding it as slot `-1`. Never the window-click packet, so
     * without this the server keeps tracking the stack the player dragged away and every place or
     * drop acts on it.
     *
     * Ignored outside creative, as `ServerPlayerGameMode.handleCreativeModeItemAdd` does. A stack
     * carrying data components fails [WireItemStack.read] and costs the connection, the same
     * boundary as anywhere else a stack is read off the wire.
     */
    private fun handleSetCreativeModeSlot(packet: ServerboundSetCreativeModeSlotPacket) {
        val session = playerSession ?: return
        if (session.gamemode != Gamemode.CREATIVE) return
        // The client numbers this in container 0's own slots, not this server's storage order, and
        // the two are neither the same size nor the same layout: taking it at face value wrote a
        // hotbar edit into the armour slots, so the item was accepted and then never in hand.
        // Slot -1 (dragged out of the window to discard) and the crafting slots resolve to null.
        val slot = PlayerInventory.storageSlotOf(packet.slot) ?: return
        val stack =
            packet.item?.let { wire ->
                val id = Items.REGISTRY.identifierOf(wire.itemId) ?: return@let null
                StoredItemStack(id.toString(), wire.count)
            }
        session.inventory[slot] = stack
        sendInventorySlot(slot)
    }

    /**
     * Middle-click "pick block". In creative, always fills the current hotbar slot with a full
     * stack of the targeted block's item; in survival, only swaps it in from wherever this player
     * already carries it, matching vanilla: survival pick-block cannot conjure an item from
     * nothing. [ServerboundPickItemFromBlockPacket.includeData] (copying a sign's text, a
     * container's contents onto the picked stack) is a documented gap: no block entities yet.
     */
    private fun handlePickItemFromBlock(packet: ServerboundPickItemFromBlockPacket) {
        val session = playerSession ?: return
        val stateId = world.getBlockState(packet.pos.x, packet.pos.y, packet.pos.z) ?: return
        if (isAirLike(stateId)) return
        val block = Blocks.byStateId(stateId) ?: return
        val item = Items.REGISTRY.byIdentifier(block.block) ?: return
        val inventory = session.inventory
        val id = item.id.toString()

        val existing = inventory.indexOf(id)
        when {
            // Already in the hotbar: vanilla only moves the selection, it does not touch a slot.
            existing != null && existing >= PlayerInventory.HOTBAR_START -> {
                inventory.heldSlot = existing - PlayerInventory.HOTBAR_START
            }
            // Elsewhere in the inventory: swapped into a suitable hotbar slot, not duplicated.
            existing != null -> {
                inventory.heldSlot = inventory.suitableHotbarSlot()
                inventory.swap(PlayerInventory.HOTBAR_START + inventory.heldSlot, existing)
            }
            // Not carried at all: only creative may conjure one, and only ever a single item.
            session.gamemode == Gamemode.CREATIVE -> {
                inventory.heldSlot = inventory.suitableHotbarSlot()
                val target = PlayerInventory.HOTBAR_START + inventory.heldSlot
                // Every hotbar slot was full, so whatever is in the way moves to a free slot
                // rather than being destroyed; if nothing is free, vanilla overwrites it too.
                inventory[target]?.let { displaced -> inventory.firstFreeSlot()?.let { inventory[it] = displaced } }
                inventory[target] = StoredItemStack(id, 1)
            }
            else -> return
        }

        sendCompressed(SetHeldSlotPacket.ID) { SetHeldSlotPacket(inventory.heldSlot).write(it) }
        sendInventoryAll()
    }

    /**
     * The `add_entity` that makes another player exist for a client at all.
     *
     * Without it there is a tab-list entry and nothing in the world: every movement packet after it
     * refers to an entity the client was never told to create, so the player is simply invisible.
     * The uuid has to be the real profile one, because that is what the client matches against the
     * tab list to find the skin to draw.
     */
    private fun spawnPlayerPacket(target: PlayerSession): SpawnEntityPacket =
        SpawnEntityPacket(
            entityId = target.entityId,
            uuid = target.uuid,
            entityType = SpawnEntityPacket.PLAYER_ENTITY_TYPE,
            x = target.x,
            y = target.y,
            z = target.z,
            pitch = target.pitch,
            yaw = target.yaw,
            headYaw = target.yaw,
        )

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
     * Sends whichever requested chunks have finished generating, nearest first, leaving the rest
     * pending and never blocking on generation.
     *
     * Driven from [TickLoop] as well as this connection's read loop: pushing only when the client
     * sends something stalls the moment it goes quiet, and calling it from both means a client
     * streaming input gets chunks at input rate. Capped per call so one flush cannot monopolize
     * the socket.
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
                containerId = openMenu.containerId,
                stateId = state,
                items =
                    List(openMenu.slotCount) { menuSlot ->
                        openMenu.slotAt(menuSlot)?.let { session.inventory.toWire(readSlot(session, it)) }
                    },
                carriedItem = carriedItem?.let(session.inventory::toWire),
            ).write(it)
        }
        sendCompressed(SetHeldSlotPacket.ID) { SetHeldSlotPacket(session.inventory.heldSlot).write(it) }
    }

    /** Re-sends the whole window, for a change that could have touched any slot. */
    internal fun sendInventoryAll() {
        sendInventory()
    }

    /**
     * One slot changed, for when re-sending forty of them would be silly. [slot] is this server's
     * own storage index; it goes out translated to container 0's own numbering (see
     * [PlayerInventory.menuSlotOf]), because the client addresses its window, not our array.
     */
    internal fun sendInventorySlot(slot: Int) {
        val session = playerSession ?: return
        sendPlayPacket(ContainerSetSlotPacket.ID) {
            ContainerSetSlotPacket(
                containerId = openMenu.containerId,
                stateId = ++containerStateId,
                slot = openMenu.menuSlotOf(MenuSlot.Inventory(slot)) ?: PlayerInventory.menuSlotOf(slot),
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
        tryPickupNearbyItems()
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

        /** The largest crafting grid any window here has, so one array serves both. */
        const val MAX_GRID_SIZE = 9

        /** Container ids for opened blocks. 0 is the player's own window and is never reused. */
        const val FIRST_BLOCK_CONTAINER_ID = 1

        /** Wraps well before the varint gets long; vanilla wraps at 100 for the same reason. */
        const val MAX_CONTAINER_ID = 100

        val CRAFTING_TABLE: Identifier = Identifier.vanilla("crafting_table")

        /** How close (blocks) a player has to be to a dropped item to vacuum it up. Vanilla's own AABB-based range is close to this. */
        const val PICKUP_RADIUS = 1.5

        /** The click target vanilla uses for "outside the window", which throws the cursor stack. */
        const val OUTSIDE_WINDOW_SLOT = -999
        const val RIGHT_BUTTON = 1

        // parity: `AbstractContainerMenu`'s own quick-craft stages and drag types.
        const val QUICK_CRAFT_START = 0
        const val QUICK_CRAFT_ADD = 1
        const val QUICK_CRAFT_END = 2
        const val QUICK_CRAFT_TYPE_LEFT = 0
        const val QUICK_CRAFT_TYPE_RIGHT = 1
        const val QUICK_CRAFT_TYPE_MIDDLE = 2

        // parity: `Player.drop`'s own throw, and the small pop a broken block's drop gets.
        const val THROW_SPEED = 0.3
        const val THROW_LIFT = 0.1
        const val THROW_EYE_HEIGHT = 1.5
        const val BREAK_DROP_LIFT = 0.2

        /** A block's own drop is meant to be walked into immediately, unlike a deliberate toss. */
        const val BREAK_DROP_PICKUP_DELAY = 10

        /** How far a client is told about dropped items, squared. Well inside the view distance. */
        const val ITEM_VIEW_RADIUS_SQUARED = 64.0 * 64.0
    }
}

/** [org.basaltmc.protocol.configuration.ConfigurationKeepAlivePacket]'s id, matched here just to no-op it explicitly. */
private const val CONFIGURATION_KEEP_ALIVE_ID = 4

/** `ServerboundClientInformationPacket` in Play: the same settings body Configuration carries. */
private const val SERVERBOUND_CLIENT_INFORMATION_ID = 0x0e

/** `ServerboundPlayerInputPacket`'s id: one byte of held movement keys. */
private const val SERVERBOUND_PLAYER_INPUT_ID = 0x2b

/** Bit 5 of that byte, `shift` in vanilla's own flag order (forward, back, left, right, jump, shift, sprint). */
private const val SNEAK_INPUT_FLAG = 1 shl 5

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
        "minecraft:timeline",
    )

private fun angleToByte(degrees: Float): Byte = ((degrees / 360f) * 256f).toInt().toByte()
