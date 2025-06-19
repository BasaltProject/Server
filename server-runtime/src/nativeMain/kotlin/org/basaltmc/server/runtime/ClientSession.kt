package org.basaltmc.server.runtime

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockEntityTypes
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.ConsumeEffect
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.ItemArmour
import org.basaltmc.server.registry.ItemFood
import org.basaltmc.server.registry.ItemTools
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.PlayerCommandSource
import org.basaltmc.server.runtime.command.commands.BossBarRegistry
import org.basaltmc.server.runtime.command.timePacket
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.item.ItemDamageEffects
import org.basaltmc.server.runtime.item.ItemWear
import org.basaltmc.server.runtime.placeholder.PlaceholderSlots
import org.basaltmc.server.runtime.player.Combat
import org.basaltmc.server.runtime.player.Difficulty
import org.basaltmc.server.runtime.player.FoodData
import org.basaltmc.server.runtime.player.Menu
import org.basaltmc.server.runtime.player.MenuSlot
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerPersistence
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.registry.VanillaBiomes
import org.basaltmc.server.runtime.registry.VanillaRegistryData
import org.basaltmc.server.runtime.registry.VanillaTagData
import org.basaltmc.server.runtime.ui.PlayerUi
import org.basaltmc.server.runtime.ui.UiConfigs
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.AdvancementTriggers
import org.basaltmc.server.runtime.world.BedProblem
import org.basaltmc.server.runtime.world.Beds
import org.basaltmc.server.runtime.world.BlockInteractions
import org.basaltmc.server.runtime.world.BlockPlacement
import org.basaltmc.server.runtime.world.Buckets
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.server.runtime.world.DamageProtection
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.DoubleBlocks
import org.basaltmc.server.runtime.world.EnchantmentAttributes
import org.basaltmc.server.runtime.world.EndPortals
import org.basaltmc.server.runtime.world.Explosions
import org.basaltmc.server.runtime.world.Fires
import org.basaltmc.server.runtime.world.FluidFlow
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.server.runtime.world.NeighbourUpdates
import org.basaltmc.server.runtime.world.NetherPortals
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.RealBlockTags
import org.basaltmc.server.runtime.world.RealDamageTypeTags
import org.basaltmc.server.runtime.world.RealDialogs
import org.basaltmc.server.runtime.world.RealEnchantments
import org.basaltmc.server.runtime.world.RealEntityTypeTags
import org.basaltmc.server.runtime.world.RealFluidTags
import org.basaltmc.server.runtime.world.RealItemTags
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.RealRecipes
import org.basaltmc.server.runtime.world.RealTimelineTags
import org.basaltmc.server.runtime.world.RealTimelines
import org.basaltmc.server.runtime.world.Signs
import org.basaltmc.server.runtime.world.SnowyBlocks
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.runtime.world.WorldWeather
import org.basaltmc.server.storage.BanKind
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.server.storage.StoredSpawnPoint
import org.basaltmc.server.storage.StoredStats
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkDataCodec
import org.basaltmc.server.world.ChunkPos
import org.basaltmc.server.world.ChunkTracker
import org.basaltmc.server.world.crafting.CraftingInput
import org.basaltmc.server.world.light.LightWindow
import org.basaltmc.server.world.portal.PortalShape
import org.basaltmc.wire.auth.Cfb8Cipher
import org.basaltmc.wire.auth.MinecraftCrypt
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.auth.OnlineMode
import org.basaltmc.wire.auth.SessionProfileProperty
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.common.timings.time
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.SocketException
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.io.Zlib
import org.basaltmc.wire.io.sleepMillis
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.PacketFraming
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.configuration.BrandPacket
import org.basaltmc.wire.protocol.configuration.ClientInformationPacket
import org.basaltmc.wire.protocol.configuration.FinishConfigurationPacket
import org.basaltmc.wire.protocol.configuration.KnownPack
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.configuration.RegistryEntry
import org.basaltmc.wire.protocol.configuration.SelectKnownPacksPacket
import org.basaltmc.wire.protocol.configuration.TagEntry
import org.basaltmc.wire.protocol.configuration.TagGroup
import org.basaltmc.wire.protocol.configuration.TagsPacket
import org.basaltmc.wire.protocol.handshake.ClientIntent
import org.basaltmc.wire.protocol.handshake.HandshakePacket
import org.basaltmc.wire.protocol.login.GameProfile
import org.basaltmc.wire.protocol.login.GameProfileProperty
import org.basaltmc.wire.protocol.login.HelloPacket
import org.basaltmc.wire.protocol.login.KeyPacket
import org.basaltmc.wire.protocol.login.LoginAcknowledgedPacket
import org.basaltmc.wire.protocol.login.LoginCompressionPacket
import org.basaltmc.wire.protocol.login.LoginDisconnectPacket
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.login.LoginStartPacket
import org.basaltmc.wire.protocol.play.AcceptTeleportationPacket
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.BlockDestructionPacket
import org.basaltmc.wire.protocol.play.BlockEntityDataPacket
import org.basaltmc.wire.protocol.play.BlockHitResult
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.BossEventPacket
import org.basaltmc.wire.protocol.play.ChatCommandPacket
import org.basaltmc.wire.protocol.play.ChatMessagePacket
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchStartPacket
import org.basaltmc.wire.protocol.play.ChunkBiomeData
import org.basaltmc.wire.protocol.play.ChunkBlockEntityInfo
import org.basaltmc.wire.protocol.play.ChunksBiomesPacket
import org.basaltmc.wire.protocol.play.ClientCommandPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockChangedAckPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.ClientboundPlayerAbilitiesPacket
import org.basaltmc.wire.protocol.play.ClientboundTakeItemEntityPacket
import org.basaltmc.wire.protocol.play.CommandSuggestionRequestPacket
import org.basaltmc.wire.protocol.play.CommandSuggestionsPacket
import org.basaltmc.wire.protocol.play.CommandsPacket
import org.basaltmc.wire.protocol.play.ContainerClosePacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.DamageEventPacket
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.EntityPositionSyncPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.wire.protocol.play.GameEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.GlobalPos
import org.basaltmc.wire.protocol.play.Heightmap
import org.basaltmc.wire.protocol.play.HeightmapType
import org.basaltmc.wire.protocol.play.LevelEventPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.OpenSignEditorPacket
import org.basaltmc.wire.protocol.play.PlayDisconnectPacket
import org.basaltmc.wire.protocol.play.PlayKeepAlivePacket
import org.basaltmc.wire.protocol.play.PlayerCombatKillPacket
import org.basaltmc.wire.protocol.play.PlayerCommandPacket
import org.basaltmc.wire.protocol.play.PlayerInfoEntry
import org.basaltmc.wire.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.play.PlayerLoadedPacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
import org.basaltmc.wire.protocol.play.Pose
import org.basaltmc.wire.protocol.play.RemoveEntitiesPacket
import org.basaltmc.wire.protocol.play.RemoveMobEffectPacket
import org.basaltmc.wire.protocol.play.RespawnPacket
import org.basaltmc.wire.protocol.play.RotateHeadPacket
import org.basaltmc.wire.protocol.play.ServerboundContainerClickPacket
import org.basaltmc.wire.protocol.play.ServerboundPickItemFromBlockPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerAbilitiesPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionLookPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCarriedItemPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCreativeModeSlotPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemPacket
import org.basaltmc.wire.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.wire.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SetExperiencePacket
import org.basaltmc.wire.protocol.play.SetHealthPacket
import org.basaltmc.wire.protocol.play.SetHeldSlotPacket
import org.basaltmc.wire.protocol.play.SetSimulationDistancePacket
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.protocol.play.SignUpdatePacket
import org.basaltmc.wire.protocol.play.SignedChatCommandPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.SpawnInfo
import org.basaltmc.wire.protocol.play.SpawnPositionPacket
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import org.basaltmc.wire.protocol.play.UpdateMobEffectPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.status.PingRequestPacket
import org.basaltmc.wire.protocol.status.PongResponsePacket
import org.basaltmc.wire.protocol.status.StatusRequestPacket
import org.basaltmc.wire.protocol.status.StatusResponsePacket
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.LangTable
import org.basaltmc.wire.text.TextColor
import org.basaltmc.wire.text.resolvePlainText
import kotlin.concurrent.AtomicInt
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.native.concurrent.ThreadLocal
import kotlin.random.Random
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid
import org.basaltmc.server.world.loot.ItemStack as LootItemStack

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
    org.basaltmc.wire.protocol.status.ServerStatusPayload,
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
    private val bossBars: BossBarRegistry? = null,
    /** Whether the whitelist is being enforced right now, which `/whitelist on` can change. */
    private val whitelistEnforced: () -> Boolean = { false },
    private val enchantments: RealEnchantments? = null,
    private val dialogs: RealDialogs? = null,
    private val timelines: RealTimelines? = null,
    /** What each player has been taught, which `limited_crafting` consults. */
    private val progress: PlayerProgress? = null,
    /** What turns a picked-up item into an advancement. Null where nothing loaded the definitions. */
    private val advancementTriggers: AdvancementTriggers? = null,
    private val blockTags: RealBlockTags? = null,
    /** The real item tags, sent to the client alongside the block ones. */
    private val itemTags: RealItemTags? = null,
    /** The fluid tags, which are what the client checks before it believes it is under water. */
    private val fluidTags: RealFluidTags? = null,
    private val timelineTags: RealTimelineTags? = null,
    private val damageTypeTags: RealDamageTypeTags? = null,
    private val entityTypeTags: RealEntityTypeTags? = null,
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

    /** How an enchanted stack resolves its ids, or null when no enchantment registry went out. */
    internal val enchantmentIds: ((Identifier) -> Int?)?
        get() = enchantments?.takeIf { it.size > 0 }?.let { registry -> { id -> registry.protocolIdOf(id) } }

    /** The same lookup backwards, for a stack coming *from* the client. */
    private fun enchantmentNameOf(protocolId: Int): Identifier? = enchantments?.entries?.getOrNull(protocolId)?.first

    /** What an enchantment takes off a durability cost, read from the definitions themselves. */
    private val damageEffects =
        ItemDamageEffects(
            definitionOf = { enchantments?.definitionOf(it) },
            itemInTag = { tag, item -> itemTags?.contains(tag, item) == true },
        )

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

    /** parity: `Entity.fallDistance`, added up from the movement rather than asked of the client. */
    private var fallDistance = 0.0

    /** Where this player is digging, for how long, and which crack stage everyone else has seen. */
    private var diggingAt: BlockPos? = null
    private var diggingTicks = 0
    private var diggingStage = -1

    /** Which slot is being eaten from, and how many ticks of the mouthful are left. */
    private var eatingSlot = -1
    private var eatingTicks = 0

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

    /** Summoned entities this client has been told about, so each is spawned once. */
    private val trackedEntities = HashSet<Int>()

    /** Which other players this client currently has as entities, kept by [syncPlayers]. */
    private val trackedPlayers = HashSet<Int>()

    /**
     * The flags and pose this player was last drawn with, so only changes go on the wire.
     *
     * They start at what an entity has by default, so a player who joins standing still and unlit
     * costs no packet at all: the client already believes exactly that.
     */
    private var lastSharedFlags = 0
    private var lastPose = Pose.STANDING

    /** What each of them was last seen holding and wearing, so only changes go on the wire. */
    private val trackedEquipment = HashMap<Int, List<StoredItemStack?>>()

    /** Static entities emitted by world generation that this client has been told about. */
    private val trackedGeneratedEntities = HashSet<Int>()

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
                        timings.of("player.players").time { syncPlayers() }
                        timings.of("player.posture").time { syncPosture() }
                        timings.of("player.items").time { syncItemEntities() }
                        timings.of("player.entities").time { syncSummonedEntities() }
                        timings.of("player.generated_entities").time { syncGeneratedEntities() }
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
            // The frame array is freshly ours (tryReadFrame copies or decompresses), so it can back
            // the buffer directly instead of being copied a second time.
            handleFrame(ByteBuffer.wrap(frame))
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
        // parity: `PlayerList.canPlayerLogin`, which checks the bans and the whitelist before the
        // connection is allowed to go any further.
        refuseLogin(id, name)?.let { reason ->
            sendUncompressed(LoginDisconnectPacket.ID) { LoginDisconnectPacket(reason).write(it) }
            connection.close()
            return
        }
        sendUncompressed(LoginCompressionPacket.ID) { LoginCompressionPacket(compressionThreshold).write(it) }
        compressionEnabled = true

        val profile = GameProfile(id, name, properties.map { GameProfileProperty(it.name, it.value, it.signature) })
        pendingProfile = profile
        val sessionId = OfflineUuid.forUsername("session:$name")
        sendCompressed(LoginFinishedPacket.ID) { LoginFinishedPacket(profile, sessionId).write(it) }
    }

    /** Why this login may not proceed, or null when it may. */
    private fun refuseLogin(
        id: Uuid,
        name: String,
    ): Component? {
        val access = playerStore?.takeIf { it.isOpen }?.access ?: return null
        access.banOf(id.toString(), BanKind.PLAYER)?.let { ban ->
            return Component.translatable("multiplayer.disconnect.banned.reason", Component.text(ban.reason))
        }
        connection.remoteAddress?.let { address ->
            access.banOf(address, BanKind.IP)?.let { ban ->
                return Component.translatable("multiplayer.disconnect.banned_ip.reason", Component.text(ban.reason))
            }
        }
        if (whitelistEnforced() && !access.isWhitelisted(id.toString())) {
            logger.info { "Refused $name: not on the whitelist." }
            return Component.translatable("multiplayer.disconnect.not_whitelisted")
        }
        return null
    }

    // --- Configuration ---

    private fun enterConfiguration() {
        state = ProtocolState.CONFIGURATION
        // parity: `ServerConfigurationPacketListenerImpl` opens with the brand, which is what F3
        // prints as `"<brand>" server`. Without it the client has nothing to name us with.
        val brand = BrandPacket("Basalt ${BuildInfo.MINECRAFT_VERSION}")
        sendCompressed(BrandPacket.ID) { brand.write(it) }
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
        // All eleven, not just one: structure cats send their variant id in set_entity_data, and a
        // registry with fewer entries than the id makes the client drop the connection on decode.
        // The numeric id of each variant is its index in this packet, which is what
        // GeneratedEntityPresentation's CAT_VARIANT_IDS must agree with.
        sendCompressed(RegistryDataPacket.ID) {
            RegistryDataPacket(
                "minecraft:cat_variant",
                VanillaRegistryData.catVariants.map { id ->
                    RegistryEntry("minecraft:$id", VanillaRegistryData.catVariant(id))
                },
            ).write(it)
        }
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
        // The enchantment registry, when this build could convert every file the jar ships. A
        // partial one disconnects clients at login (see RealEnchantments), so it is all or nothing
        // and the empty entry below covers the "nothing" case.
        val enchantmentEntries = enchantments?.entries.orEmpty()
        if (enchantmentEntries.isNotEmpty()) {
            sendCompressed(RegistryDataPacket.ID) {
                RegistryDataPacket(
                    "minecraft:enchantment",
                    enchantmentEntries.map { (id, value) -> RegistryEntry(id.toString(), value) },
                ).write(it)
            }
        }
        val dialogEntries = dialogs?.entries.orEmpty()
        if (dialogEntries.isNotEmpty()) {
            sendCompressed(RegistryDataPacket.ID) {
                RegistryDataPacket(
                    "minecraft:dialog",
                    dialogEntries.map { (id, value) -> RegistryEntry(id.toString(), value) },
                ).write(it)
            }
        }
        // Without these the client has a clock that ticks and nothing that reads it: the sky never
        // moves however much `/time` reports success.
        val timelineEntries = timelines?.entries.orEmpty()
        if (timelineEntries.isNotEmpty()) {
            sendCompressed(RegistryDataPacket.ID) {
                RegistryDataPacket(
                    "minecraft:timeline",
                    timelineEntries.map { (id, value) -> RegistryEntry(id.toString(), value) },
                ).write(it)
            }
        }
        for (emptyRegistryId in EMPTY_SYNCHRONIZED_REGISTRIES) {
            if (emptyRegistryId == "minecraft:enchantment" && enchantmentEntries.isNotEmpty()) continue
            if (emptyRegistryId == "minecraft:dialog" && dialogEntries.isNotEmpty()) continue
            if (emptyRegistryId == "minecraft:timeline" && timelineEntries.isNotEmpty()) continue
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
            "minecraft:fluid" ->
                fluidTags
                    ?.membersOf(Identifier.parse(tagName))
                    .orEmpty()
                    .mapNotNull(RealFluidTags.PROTOCOL_IDS::get)
            // The ids are indices into the timeline registry this server sent, in that same order.
            "minecraft:timeline" ->
                timelineTags
                    ?.membersOf(Identifier.parse(tagName))
                    .orEmpty()
                    .mapNotNull { id -> timelines?.ids?.indexOf(id)?.takeIf { it >= 0 } }
            "minecraft:entity_type" ->
                entityTypeTags
                    ?.membersOf(Identifier.parse(tagName))
                    .orEmpty()
                    .mapNotNull { EntityTypes[it]?.protocolId }
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
        val initialChunks = chunkTracker.update(centerChunkX, centerChunkZ, viewRadius)
        sendCompressed(SetChunkCacheCenterPacket.ID) { SetChunkCacheCenterPacket(centerChunkX, centerChunkZ).write(it) }
        sendCompressed(SetChunkCacheRadiusPacket.ID) { SetChunkCacheRadiusPacket(chunkRadius).write(it) }
        sendCompressed(SetSimulationDistancePacket.ID) { SetSimulationDistancePacket(chunkRadius).write(it) }

        // Mirrors real vanilla's join order (PlayerList.placeNewPlayer/sendLevelInfo, confirmed by
        // decompiling the real jar): teleport and player info arrive before level data, and the
        // level_chunks_load_start game event comes right before chunks, not after. Sending chunks
        // first (as an earlier version of this code did) left a real client stuck on "loading
        // terrain" forever, even with correct chunk data and a correct batch wrapper.
        // parity: `PlayerList.sendLevelInfo` sends the clocks just before the spawn position, and
        // the weather events only when it is actually raining. A client never told the time draws a
        // sun frozen wherever its last server left it.
        sendTime(world)
        if (world.weather.raining) sendWeather(world.weather)
        // parity: `CustomBossEvents.onPlayerConnect`, which shows a joining player every bar that
        // names them. Without it a `/bossbar` set before they joined is invisible until it changes.
        for (bar in bossBars?.barsFor(session).orEmpty()) {
            sendCompressed(BossEventPacket.ID) { BossEventPacket(bar.uuid, bar.addOperation()).write(it) }
        }
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
        // Their advancements and recipe book, before anything can ask about them: `limited_crafting`
        // consults the book on the first click.
        progress?.load(session.uuid.toString())
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
        // Only the ground the player lands on is waited for; the rest streams in through
        // `flushReadyChunks`. Waiting for the whole view distance made joining feel dead for three
        // seconds, because this runs on the read thread and nothing the client sent was read until
        // the last chunk was generated. One ring wider than what is sent, so every chunk that goes
        // out already has its eight neighbours.
        val awaited =
            (-(LOGIN_CHUNK_RADIUS + 1)..(LOGIN_CHUNK_RADIUS + 1))
                .flatMap { dx ->
                    (-(LOGIN_CHUNK_RADIUS + 1)..(LOGIN_CHUNK_RADIUS + 1)).map { dz ->
                        (centerChunkX + dx) to (centerChunkZ + dz)
                    }
                }.sortedBy { squaredDistanceToCenter(it.first, it.second) }
        generationQueue.awaitAll(awaited)
        val loginChunks =
            awaited
                .filter { position ->
                    abs(position.first - centerChunkX) <= LOGIN_CHUNK_RADIUS &&
                        abs(position.second - centerChunkZ) <= LOGIN_CHUNK_RADIUS
                }.mapNotNull { position -> world.state.chunkIfLoaded(position.first, position.second) }
        sendCompressed(ChunkBatchStartPacket.ID) { }
        for (chunk in loginChunks) sendChunk(chunk)
        sendCompressed(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(loginChunks.size).write(it) }

        // Everything else the view distance covers, queued for the outbound loop to stream.
        val sent = loginChunks.map { chunk -> chunk.chunkX to chunk.chunkZ }.toSet()
        val streaming = initialChunks.toLoad.filter { (it.x to it.z) !in sent }
        chunkStreamMutex.withLock { for (pos in streaming) pendingChunks.add(pos) }
        for (pos in streaming) generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))

        // Only the players who cannot see this one as an entity: for the rest, [syncPlayers] sends
        // the entry itself, immediately before the spawn, so the two never arrive out of order.
        for (other in others.filter { it.worldName != session.worldName }) {
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
        }
        // Spawning the players who can see each other is not done here any more: it is
        // [syncPlayers], on every connection's own loop. Doing it once at join meant a player who
        // arrived later, or walked in from another world, was never spawned at all, and one who
        // left stayed standing there: the world an entity is in has to be looked at again and
        // again, not once.

        // Drawn here rather than left to the loop below. That loop sends chunks before it redraws
        // anything, so on a fresh join the scoreboard, tab list and boss bar waited behind the whole
        // terrain stream: seconds of a player staring at a bare screen for packets that were ready
        // immediately.
        tickUi()

        // parity: `PlayerList.placeNewPlayer` announces it here, near the end and in yellow, not on
        // the first packet of the join. Everyone means everyone on the server, not just this world:
        // it is a server message, not a world one.
        broadcastToEveryone(
            Component
                .translatable("multiplayer.player.joined", Component.text(session.name))
                .color(TextColor.YELLOW),
        )

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
            isFlat = currentWorld?.declaration?.type == org.basaltmc.server.runtime.config.WorldType.FLAT,
            death = null,
            portalCooldown = 0,
            seaLevel = currentWorld?.declaration?.dimension?.seaLevel ?: 63,
        )

    /**
     * Sends one chunk again, for a change too large to express as block updates.
     *
     * `/place template` writes a whole structure at once: hundreds of blocks across several chunks,
     * where one packet per chunk is both smaller and simpler than the section updates would be.
     */
    internal fun resendChunk(
        chunkX: Int,
        chunkZ: Int,
    ) {
        val chunk = currentWorld?.state?.chunkIfLoaded(chunkX, chunkZ) ?: return
        sendPlayPacket(ChunkBatchStartPacket.ID) { }
        sendChunk(chunk)
        sendPlayPacket(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(1).write(it) }
    }

    /**
     * New biomes for columns this client already has, without resending them.
     *
     * parity: `ClientboundChunksBiomesPacket`, which vanilla sends from `/fillbiome` for exactly
     * this reason: a chunk resend would drop and rebuild the column, and the biome is the only
     * thing that changed.
     */
    internal fun sendBiomes(chunks: List<Pair<Int, Int>>) {
        val world = currentWorld ?: return
        val columns =
            chunks.mapNotNull { (chunkX, chunkZ) ->
                val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: return@mapNotNull null
                ChunkBiomeData(chunkX, chunkZ, ChunkDataCodec.encodeBiomes(chunk))
            }
        if (columns.isEmpty()) return
        sendCompressed(ChunksBiomesPacket.ID) { ChunksBiomesPacket(columns).write(it) }
    }

    private fun sendChunk(chunk: Chunk) {
        val chunkData = ChunkDataCodec.encodeSections(chunk)
        val world = currentWorld
        val light =
            ChunkDataCodec.encodeLight(
                chunk,
                world?.declaration?.dimension?.hasSkylight ?: true,
                // The chunk and the eight around it, straight off the chunk objects: light reaches
                // less than a chunk, so that is everything that can shine into this one.
                LightWindow(chunk) { chunkX, chunkZ -> world?.state?.chunkIfLoaded(chunkX, chunkZ) },
            )
        val heightmap = ChunkDataCodec.encodeHeightmap(chunk)
        sendCompressed(MapChunkPacket.ID) {
            MapChunkPacket(
                x = chunk.chunkX,
                z = chunk.chunkZ,
                heightmaps = listOf(Heightmap(HeightmapType.MOTION_BLOCKING, heightmap)),
                chunkData = chunkData,
                blockEntities =
                    chunk.blockEntities().mapNotNull { blockEntity ->
                        val typeId =
                            BlockEntityTypes.idOf(blockEntity.type)
                                ?: return@mapNotNull unknownBlockEntity(blockEntity.type)
                        ChunkBlockEntityInfo(
                            packedXZ = (blockEntity.localX shl 4) or blockEntity.localZ,
                            y = blockEntity.y,
                            typeId = typeId,
                            tag = blockEntity.data,
                        )
                    },
                skyLightMask = light.skyLightMask,
                blockLightMask = light.blockLightMask,
                emptySkyLightMask = light.emptySkyLightMask,
                emptyBlockLightMask = light.emptyBlockLightMask,
                skyLight = light.skyLight,
                blockLight = light.blockLight,
            ).write(it)
        }
        sendCompressed(UpdateLightPacket.ID) {
            UpdateLightPacket(
                chunkX = chunk.chunkX,
                chunkZ = chunk.chunkZ,
                skyLightMask = light.skyLightMask,
                blockLightMask = light.blockLightMask,
                emptySkyLightMask = light.emptySkyLightMask,
                emptyBlockLightMask = light.emptyBlockLightMask,
                skyLight = light.skyLight,
                blockLight = light.blockLight,
            ).write(it)
        }
    }

    /**
     * Leaves one block entity out of the chunk rather than failing the packet. It used to throw,
     * which dropped the player: flying into a village, whose banners this build had no ID for, was
     * enough. The block itself still arrives, only the data the client would decorate it with does
     * not, and the log names the type once per session so the gap is visible.
     */
    private fun unknownBlockEntity(type: Identifier): ChunkBlockEntityInfo? {
        if (loggedUnknownBlockEntities.add(type)) {
            logger.warn { "No protocol ID for block entity type $type; leaving it out of the chunk." }
        }
        return null
    }

    private val loggedUnknownBlockEntities = mutableSetOf<Identifier>()

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
            // parity: `ServerboundMovePlayerPacket.Rot`, which a client sends when it turns on the
            // spot. Ignoring it meant standing still and looking around reached nobody: the head
            // only ever moved when the feet did.
            MOVE_PLAYER_ROT_ID -> {
                val session = playerSession
                val yaw = body.readFloat()
                val pitch = body.readFloat()
                val onGround = (body.readByte().toInt() and 1) != 0
                if (session != null) updatePosition(session.x, session.y, session.z, yaw, pitch, onGround)
            }
            // parity: `ServerboundMovePlayerPacket.StatusOnly`, one byte: neither moved nor turned,
            // only landed or left the ground.
            MOVE_PLAYER_STATUS_ONLY_ID -> {
                val session = playerSession
                val onGround = (body.readByte().toInt() and 1) != 0
                if (session != null) {
                    updatePosition(session.x, session.y, session.z, session.yaw, session.pitch, onGround)
                }
            }
            ChatMessagePacket.ID -> {
                val chat = ChatMessagePacket.read(body)
                broadcastChat(chat.message)
            }
            ChatCommandPacket.ID -> runCommand(ChatCommandPacket.read(body).command)
            CommandSuggestionRequestPacket.ID -> sendSuggestions(CommandSuggestionRequestPacket.read(body))
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
                    PlayerCommandPacket.Action.STOP_SLEEPING -> wakeUp()
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
                    syncHeldAttributes()
                }
            }
            ServerboundSetCreativeModeSlotPacket.ID ->
                handleSetCreativeModeSlot(ServerboundSetCreativeModeSlotPacket.read(body))
            ServerboundContainerClickPacket.ID -> handleContainerClick(ServerboundContainerClickPacket.read(body))
            ContainerClosePacket.SERVERBOUND_ID -> {
                ContainerClosePacket.read(body)
                playerSession?.let(::closeOpenMenu)
            }
            SignUpdatePacket.ID -> writeSign(SignUpdatePacket.read(body))
            SWING_PACKET_ID -> handleSwing(body.readVarInt())
            ServerboundUseItemOnPacket.ID -> handleUseItemOn(ServerboundUseItemOnPacket.read(body))
            ServerboundUseItemPacket.ID -> handleUseItem(ServerboundUseItemPacket.read(body))
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
     * Whether the block in front of this player comes apart in a single tick, tool and enchantments
     * included. parity: `ServerPlayerGameMode.handleBlockBreakAction`, which asks the same question
     * before deciding that a dig is over the moment it starts.
     */
    private fun breaksInOneTick(
        block: Identifier?,
        hardness: Blocks.BlockHardness?,
    ): Boolean {
        if (block == null || hardness == null) return false
        if (hardness.isUnbreakable) return false
        if (hardness.isInstant) return true
        return destroyProgressPerTick(block, hardness) >= 1f
    }

    /** How much of a block this player takes off per tick, tool, enchantments and effects included. */
    private fun destroyProgressPerTick(
        block: Identifier,
        hardness: Blocks.BlockHardness? = Blocks.hardnessOf(block),
    ): Float {
        if (hardness == null || hardness.isUnbreakable) return 0f
        val held = playerSession?.inventory?.heldItem
        val heldItem = held?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
        return ItemTools.destroyProgressPerTick(
            item = heldItem,
            block = block,
            hardness = hardness.destroyTime,
            requiresCorrectTool = hardness.requiresCorrectTool,
            efficiencyLevel = held?.let { PlayerInventory.enchantmentLevel(it, EFFICIENCY) } ?: 0,
            hasteAmplifier = playerSession?.effects?.get(MobEffects.HASTE)?.amplifier ?: -1,
            miningFatigueAmplifier = playerSession?.effects?.get(MobEffects.MINING_FATIGUE)?.amplifier ?: -1,
            underwater = eyesInWater(),
            inAir = playerSession?.onGround == false,
        ) { tag, tagged -> blockTags?.contains(tag, tagged) == true }
    }

    /** parity: `Player.getDestroySpeed` asks whether the eyes are under water, not the feet. */
    private fun eyesInWater(): Boolean {
        val session = playerSession ?: return false
        val eyeY = session.y + Fluids.eyeHeight(session.sneaking)
        val state = world.getBlockState(floor(session.x).toInt(), floor(eyeY).toInt(), floor(session.z).toInt())
        return state != null && Fluids.of(state) == Submersion.WATER
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
            // parity: letting go before the animation ends eats nothing at all.
            ServerboundPlayerActionPacket.Action.RELEASE_USE_ITEM -> stopEating()
            ServerboundPlayerActionPacket.Action.SWAP_ITEM_WITH_OFFHAND -> swapWithOffhand()
            else -> Unit
        }
        // The cracks everyone else sees. The digger draws its own, which is why this never goes
        // back to them. parity: `ServerPlayerGameMode.destroyAndAck` and its `destroyProgress`.
        when (action.action) {
            ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK -> startDigging(action.pos)
            ServerboundPlayerActionPacket.Action.ABORT_DESTROY_BLOCK,
            ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK,
            -> stopDigging()
            else -> Unit
        }

        val gamemode = playerSession?.gamemode ?: defaultGamemode
        val targetHardness =
            world
                .getBlockState(action.pos.x, action.pos.y, action.pos.z)
                ?.let(Blocks::byStateId)
                ?.let { Blocks.hardnessOf(it.block) }
        val targetBlock =
            world
                .getBlockState(action.pos.x, action.pos.y, action.pos.z)
                ?.let(Blocks::byStateId)
                ?.block
        val breaksOn =
            when (gamemode) {
                Gamemode.CREATIVE -> ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK
                Gamemode.SURVIVAL ->
                    // A block that comes apart in one tick is destroyed by the client the moment the
                    // dig starts, and it never sends the end of one: waiting for `STOP` there is why
                    // flowers, grass and leaves could not be broken in survival at all, and why an
                    // efficiency pickaxe felt like a bare one.
                    if (breaksInOneTick(targetBlock, targetHardness)) {
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
        val leaves = meltedIce(broken, action.pos) ?: Blocks.AIR.stateId
        val changed = world.setBlockState(action.pos.x, action.pos.y, action.pos.z, leaves)
        sendCompressed(ClientboundBlockChangedAckPacket.ID) {
            ClientboundBlockChangedAckPacket(action.sequence).write(it)
        }
        if (!changed) return
        val brokenBlock = broken?.let(Blocks::byStateId)
        brokenBlock?.let { playerSession?.stats?.increment(StoredStats.mined(it.block.toString())) }

        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(action.pos, leaves).write(it)
            }
        }

        // parity: `Level.levelEvent(player, 2001, pos, state)`: the breaker predicts its own puff of
        // particles and its sound, everyone else needs telling.
        if (broken != null) {
            val effect = LevelEventPacket(LevelEventPacket.PARTICLES_DESTROY_BLOCK, action.pos, broken)
            for (other in playersInSameWorld()) {
                if (other.entityId == playerSession?.entityId) continue
                other.connection.sendPlayPacket(LevelEventPacket.ID) { effect.write(it) }
            }
        }
        stopDigging()
        refreshSnowUnder(action.pos)
        notifyNeighbours(action.pos)
        // Breaking the frame puts the portal out, which is the other half of lighting one.
        currentWorld?.let { loaded ->
            for (block in NetherPortals.extinguishAround(loaded, action.pos.x, action.pos.y, action.pos.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), Blocks.AIR.stateId)
            }
            // Whatever fluid was being held back by that block now has somewhere to go.
            for (block in FluidFlow.onBlockChanged(loaded, action.pos.x, action.pos.y, action.pos.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
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
        if (brokenBlock != null) wearHeldItemOnBreak(brokenBlock.block)
    }

    /**
     * parity: `Item.mineBlock`, which wears the held item only when it has a `minecraft:tool`
     * component, the block has some hardness, and that component costs something per block. A
     * creative player wears nothing, as `ItemStack.processDurabilityChange` decides up front.
     */
    private fun wearHeldItemOnBreak(block: Identifier) {
        val session = playerSession ?: return
        if (session.gamemode == Gamemode.CREATIVE) return
        if (Blocks.hardnessOf(block)?.isInstant != false) return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val cost = ItemWear.costPerBlock(held)
        if (cost <= 0) return
        val worn =
            ItemWear.hurt(held, cost, LegacyRandom(Random.nextLong())) { stack, amount, random ->
                damageEffects.process(stack, amount, random)
            }
        if (worn == held) return
        session.inventory[heldSlot] = worn
        sendInventorySlot(heldSlot)
    }

    /**
     * Sets fire where the flint and steel points, and lights a Nether portal if that fire lands
     * inside an obsidian frame.
     *
     * parity: `BaseFireBlock.onPlace` is what checks the frame, not the item, which is why any
     * fire in the right place opens a portal and why the portal replaces the fire rather than
     * burning next to it.
     */
    private fun igniteAt(
        session: PlayerSession,
        pos: BlockPos,
        heldSlot: Int,
        held: StoredItemStack,
    ) {
        val world = currentWorld ?: return
        val existing = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        if (!isAirLike(existing)) return

        val shape = NetherPortals.light(world, pos.x, pos.y, pos.z)
        if (shape != null) {
            val portal = PortalShape.portalState(shape.axis)
            for (block in shape.interior()) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), portal)
        } else {
            if (!NetherPortals.placeFire(world, pos.x, pos.y, pos.z)) return
            broadcastBlockUpdate(pos, FIRE_STATE)
            // parity: `BaseFireBlock.onPlace`, which is what starts fire acting at all.
            Fires.schedule(world, pos.x, pos.y, pos.z, LegacyRandom(Random.nextLong()))
        }

        if (session.gamemode == Gamemode.CREATIVE) return
        val worn =
            ItemWear.hurt(held, 1, LegacyRandom(Random.nextLong())) { stack, amount, random ->
                damageEffects.process(stack, amount, random)
            }
        if (worn == held) return
        session.inventory[heldSlot] = worn
        sendInventorySlot(heldSlot)
    }

    /** Arms an end portal frame, and opens the portal when that was the twelfth eye. */
    private fun placeEnderEye(
        session: PlayerSession,
        pos: BlockPos,
        heldSlot: Int,
        held: StoredItemStack,
    ) {
        val world = currentWorld ?: return
        val changed = EndPortals.placeEye(world, pos.x, pos.y, pos.z) ?: return
        for (block in changed) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        if (session.gamemode == Gamemode.CREATIVE) return
        val remaining = held.count - 1
        session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
        sendInventorySlot(heldSlot)
    }

    /**
     * What breaking a block leaves behind, which is water when it was ice over something solid or
     * liquid. parity: `IceBlock.playerDestroy`. Without it a frozen ocean answers a broken block
     * with a dry hole, and there is no way into the water at all.
     *
     * Silk touch is not read yet, so ice always melts here; the Nether evaporates it as vanilla does.
     */
    private fun meltedIce(
        broken: Int?,
        pos: BlockPos,
    ): Int? {
        if (broken == null || broken !in ICE_STATES) return null
        val world = currentWorld ?: return null
        if (world.declaration.dimension.hasCeiling) return null
        val below = world.state.getBlockState(pos.x, pos.y - 1, pos.z) ?: return null
        return if (Chunk.HeightmapType.MOTION_BLOCKING.matches(below)) Blocks.WATER.stateId else null
    }

    /** One block change, to everyone standing in this world. */
    private fun broadcastBlockUpdate(
        pos: BlockPos,
        stateId: Int,
    ) {
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(pos, stateId).write(it)
            }
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

    /** A newly placed sign: its block entity, then the editor for whoever put it there. */
    private fun openNewSign(
        pos: BlockPos,
        stateId: Int,
    ) {
        val world = currentWorld ?: return
        val type = Signs.typeOf(stateId) ?: return
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return
        chunk.setBlockEntity(Signs.blockEntity(pos.x and 15, pos.y, pos.z and 15, type))
        sendCompressed(OpenSignEditorPacket.ID) { OpenSignEditorPacket(pos, isFrontText = true).write(it) }
    }

    /** Reopens an existing sign, unless it has been waxed. Answers whether it did. */
    private fun editSign(pos: BlockPos): Boolean {
        val world = currentWorld ?: return false
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return false
        val existing = chunk.blockEntityAt(pos.x and 15, pos.y, pos.z and 15) ?: return false
        if (Signs.isWaxed(existing.data)) return false
        sendCompressed(OpenSignEditorPacket.ID) { OpenSignEditorPacket(pos, isFrontText = true).write(it) }
        return true
    }

    /**
     * What someone typed on a sign, kept and shown to everyone.
     *
     * parity: `ServerPlayer.updateSignText`, minus the checks this build has nothing to make: no
     * chat filtering, and no owner, since nothing records who placed it.
     */
    private fun writeSign(update: SignUpdatePacket) {
        val world = currentWorld ?: return
        val pos = update.pos
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        val type = Signs.typeOf(stateId) ?: return
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return
        val existing = chunk.blockEntityAt(pos.x and 15, pos.y, pos.z and 15)
        if (existing != null && Signs.isWaxed(existing.data)) return
        val data = Signs.withText(existing?.data ?: Signs.emptyData(), update.isFrontText, update.lines)
        chunk.setBlockEntity(Signs.blockEntity(pos.x and 15, pos.y, pos.z and 15, type, data))
        val typeId = BlockEntityTypes.idOf(type) ?: return
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(BlockEntityDataPacket.ID) {
                BlockEntityDataPacket(pos, typeId, data).write(it)
            }
        }
    }

    /**
     * Sets off an explosion, drops what it broke and hurts whoever was standing in it.
     *
     * parity: `ServerExplosion`. The knockback it gives is not sent: this build has no entity
     * motion packet yet, and that is the packet audit's business rather than this one's.
     */
    private fun blowUp(
        world: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
        power: Float,
    ) {
        val broken = Explosions.explode(world, x, y, z, power, LegacyRandom(Random.nextLong()))
        for (block in broken) {
            val at = BlockPos(block.x, block.y, block.z)
            broadcastBlockUpdate(at, Blocks.AIR.stateId)
            Blocks.byStateId(block.stateId)?.let { dropLootFor(it.block, at) }
        }
        for (block in broken) {
            NeighbourUpdates.afterChange(
                world,
                block.x,
                block.y,
                block.z,
                onReshaped = { stateId, bx, by, bz -> broadcastBlockUpdate(BlockPos(bx, by, bz), stateId) },
            ) { stateId, bx, by, bz ->
                val at = BlockPos(bx, by, bz)
                broadcastBlockUpdate(at, Blocks.AIR.stateId)
                Blocks.byStateId(stateId)?.let { dropLootFor(it.block, at) }
            }
        }
        for (player in playersInSameWorld()) {
            val distance =
                kotlin.math.sqrt(
                    (player.x - x) * (player.x - x) + (player.y - y) * (player.y - y) + (player.z - z) * (player.z - z),
                )
            val damage = Explosions.damageAt(distance, power, power * 2f)
            if (damage <= 0f) continue
            player.connection.hurt(damage, "badRespawnPoint", DamageTypes.BAD_RESPAWN_POINT)
        }
    }

    /**
     * Tells the blocks around [pos] that it changed, and clears up whatever stopped standing.
     *
     * parity: `Level.updateNeighborsAt`. What falls drops its loot with no tool involved, because
     * nobody mined it: vanilla's `dropResources` is called by the block itself.
     */
    private fun notifyNeighbours(pos: BlockPos) {
        val world = currentWorld ?: return
        NeighbourUpdates.afterChange(
            world,
            pos.x,
            pos.y,
            pos.z,
            onReshaped = { stateId, x, y, z -> broadcastBlockUpdate(BlockPos(x, y, z), stateId) },
        ) { stateId, x, y, z ->
            val at = BlockPos(x, y, z)
            broadcastBlockUpdate(at, Blocks.AIR.stateId)
            Blocks.byStateId(stateId)?.let { dropLootFor(it.block, at) }
        }
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
     * Brings this client's view of summoned entities up to date: spawns what it has not seen,
     * moves what has been moved by a command, and removes what was killed.
     *
     * Same shape as [syncItemEntities] and for the same reason: the world owns the entity, each
     * connection reports it on its own thread.
     */
    private fun syncSummonedEntities() {
        val session = playerSession ?: return
        for (entity in world.entities()) {
            if (entity.removed) continue
            if (trackedEntities.add(entity.entityId)) {
                sendPlayPacket(SpawnEntityPacket.ID) {
                    SpawnEntityPacket(
                        entityId = entity.entityId,
                        uuid = entity.uuid,
                        entityType = entity.protocolType,
                        x = entity.x,
                        y = entity.y,
                        z = entity.z,
                        pitch = entity.pitch,
                        yaw = entity.yaw,
                        headYaw = entity.yaw,
                        // A falling block is drawn from this: without it every one is stone.
                        objectData = entity.objectData,
                    ).write(it)
                }
                continue
            }
            // parity: `ServerEntity.sendChanges` sends a position *sync* for anything that has to be
            // exact, and it carries the velocity. That is what lets the client interpolate: a
            // teleport every tick snaps, which is what made a falling block judder down.
            sendPlayPacket(EntityPositionSyncPacket.ID) {
                EntityPositionSyncPacket(
                    entityId = entity.entityId,
                    x = entity.x,
                    y = entity.y,
                    z = entity.z,
                    velocityX = 0.0,
                    velocityY = entity.velocityY,
                    velocityZ = 0.0,
                    yaw = entity.yaw,
                    pitch = entity.pitch,
                    onGround = false,
                ).write(it)
            }
        }
        val gone = world.collectRemovedEntities().filter { trackedEntities.remove(it) }
        if (gone.isNotEmpty()) {
            sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
        }
    }

    /**
     * Spawns and removes the other players this client should be seeing, once per outbound tick.
     *
     * parity: `ChunkMap.TrackedEntity`, with the world as the test, since every player already
     * receives every chunk of their own view. The tab list stays server-wide, as in vanilla: only
     * the entity is per-world.
     */
    private fun syncPlayers() {
        val session = playerSession ?: return
        val registry = playerRegistry ?: return
        val visible = registry.snapshot().filter { it.uuid != session.uuid && it.worldName == session.worldName }

        for (other in visible) {
            if (!trackedPlayers.add(other.entityId)) continue
            // parity: the tab-list entry always precedes the entity. The client matches the two by
            // profile uuid to find the skin, and an entity that arrives first is drawn bald and
            // grey. Sending it again for someone already listed costs nothing and removes the race.
            sendPlayPacket(PlayerInfoUpdatePacket.ID) { buffer ->
                PlayerInfoUpdatePacket
                    .initializing(
                        listOf(
                            PlayerInfoEntry(
                                other.uuid,
                                other.name,
                                listed = true,
                                gamemode = other.gamemode,
                                properties = other.profileProperties,
                            ),
                        ),
                    ).write(buffer)
            }
            sendPlayPacket(SpawnEntityPacket.ID) { buffer -> spawnPlayerPacket(other).write(buffer) }
            sendPlayPacket(SetEntityDataPacket.ID) { buffer ->
                SetEntityDataPacket.skinParts(other.entityId, other.skinParts).write(buffer)
            }
            sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(other.entityId, other.x, other.y, other.z, other.yaw, other.pitch, other.onGround)
                    .write(it)
            }
            sendPlayPacket(RotateHeadPacket.ID) { RotateHeadPacket(other.entityId, other.yaw).write(it) }
        }

        for (other in visible) syncEquipmentOf(other)

        val stillThere = visible.map { it.entityId }.toSet()
        val gone = trackedPlayers.filter { it !in stillThere }
        if (gone.isEmpty()) return
        trackedPlayers.removeAll(gone.toSet())
        for (entityId in gone) trackedEquipment.remove(entityId)
        sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
    }

    /**
     * What [other] is holding and wearing, sent when it changes.
     *
     * parity: `ServerEntity.sendChanges`, which is what draws a sword in someone's hand and the
     * armour on their body. Without it every other player looks empty-handed and unarmoured however
     * they are actually equipped.
     */
    private fun syncEquipmentOf(other: PlayerSession) {
        val worn =
            listOf(
                other.inventory[PlayerInventory.HOTBAR_START + other.inventory.heldSlot],
                other.inventory[PlayerInventory.OFFHAND_SLOT],
            ) + (0 until PlayerInventory.ARMOUR_SLOTS).map { other.inventory[PlayerInventory.ARMOUR_START + it] }
        if (trackedEquipment[other.entityId] == worn) return
        val first = other.entityId !in trackedEquipment
        trackedEquipment[other.entityId] = worn
        // parity: `ServerEntity.sendPairingData` sends nothing for an entity carrying nothing, so
        // an empty-handed player costs no packet at all.
        if (first && worn.all { it == null }) return
        val slots =
            EQUIPMENT_SLOTS.mapIndexed { index, type ->
                SetEquipmentPacket.Slot(type, other.inventory.toWire(worn[index]))
            }
        sendPlayPacket(SetEquipmentPacket.ID) { SetEquipmentPacket(other.entityId, slots).write(it) }
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

    /** Spawns immutable entities emitted by loaded generated chunks and village templates. */
    private fun syncGeneratedEntities() {
        val session = playerSession ?: return
        val registry = playerRegistry ?: return
        val visible = HashSet<Int>()
        val entities =
            world.generatedEntities(
                registry::allocateEntityId,
                centerChunkX = floor(session.x).toInt() shr 4,
                centerChunkZ = floor(session.z).toInt() shr 4,
                radiusChunks = GENERATED_ENTITY_VIEW_RADIUS_CHUNKS,
            )
        for (entity in entities) {
            val descriptor = entity.descriptor
            val dx = descriptor.x - session.x
            val dz = descriptor.z - session.z
            if (dx * dx + dz * dz > GENERATED_ENTITY_VIEW_RADIUS_SQUARED) continue
            val typeId = GENERATED_ENTITY_TYPE_IDS[descriptor.type.path] ?: continue
            visible += entity.entityId
            if (!trackedGeneratedEntities.add(entity.entityId)) continue
            val isFrame = descriptor.type.path == "item_frame"
            sendPlayPacket(SpawnEntityPacket.ID) {
                SpawnEntityPacket(
                    entityId = entity.entityId,
                    uuid = entity.uuid,
                    entityType = typeId,
                    x = if (isFrame) floor(descriptor.x) else descriptor.x,
                    y = if (isFrame) floor(descriptor.y) else descriptor.y,
                    z = if (isFrame) floor(descriptor.z) else descriptor.z,
                    pitch = descriptor.pitch,
                    yaw = descriptor.yaw,
                    headYaw = descriptor.yaw,
                    objectData = if (isFrame) frameDirectionData(descriptor.data.getString("facing")) else 0,
                ).write(it)
            }
            if (isFrame && descriptor.data.getString("item") == "minecraft:elytra") {
                val elytra = Items.REGISTRY.byIdentifier(Identifier.vanilla("elytra")) ?: continue
                sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket.itemFrame(entity.entityId, WireItemStack(elytra.protocolId, 1)).write(it)
                }
            }
            val metadata = generatedEntityMetadata(descriptor.type.path, descriptor.data)
            if (metadata.isNotEmpty()) {
                sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket(entity.entityId, metadata).write(it)
                }
            }
            val equipment =
                generatedEntityEquipment(descriptor.data).mapNotNull { slot ->
                    val item = Items.REGISTRY.byIdentifier(Identifier.parse(slot.item)) ?: return@mapNotNull null
                    SetEquipmentPacket.Slot(slot.slot, WireItemStack(item.protocolId, slot.count))
                }
            if (equipment.isNotEmpty()) {
                sendPlayPacket(SetEquipmentPacket.ID) {
                    SetEquipmentPacket(entity.entityId, equipment).write(it)
                }
            }
        }

        val gone = trackedGeneratedEntities.filterNot { it in visible }
        if (gone.isEmpty()) return
        trackedGeneratedEntities.removeAll(gone.toSet())
        sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
    }

    private fun frameDirectionData(facing: String): Int =
        when (facing) {
            "north" -> 2
            "south" -> 3
            "west" -> 4
            "east" -> 5
            else -> 3
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
            val clickedState = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
            if (clickedState != null && Beds.isBed(clickedState)) {
                useBed(session, hit.pos, clickedState)
                return ack()
            }
            if (clickedState != null && BlockInteractions.isToggleable(clickedState)) {
                toggleBlock(hit.pos)
                return ack()
            }
            // parity: `SignBlockEntity.executeClickCommandsIfPresent` aside, clicking a sign that is
            // not waxed opens it for editing again.
            if (clickedState != null && Signs.isSign(clickedState) && editSign(hit.pos)) return ack()
        }

        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]

        // Flint and steel is not a block, so it has to be answered before the "is this placeable"
        // check below throws the click away. parity: `FlintAndSteelItem.useOn`.
        if (held != null && Identifier.parse(held.id) == FLINT_AND_STEEL) {
            igniteAt(session, hit.pos.offset(hit.direction), heldSlot, held)
            return ack()
        }
        // parity: `EnderEyeItem.useOn`, which acts on the frame itself rather than next to it.
        if (held != null && Identifier.parse(held.id) == ENDER_EYE) {
            placeEnderEye(session, hit.pos, heldSlot, held)
            return ack()
        }

        val heldBlock = held?.let { Identifier.parse(it.id) }
        // parity: `ServerPlayerGameMode.useItemOn` falls through to using the item itself, which is
        // what lets anyone eat while looking at the ground rather than only at the sky.
        if (held != null && heldBlock != null && startEating(session, heldBlock, heldSlot)) return ack()
        // A bucket aimed at a block: the client only sends `use_item` when nothing is in the way,
        // so without this, water poured anywhere near a wall went nowhere. parity: `BucketItem.use`.
        if (held != null && heldBlock != null && Buckets.isBucket(heldBlock)) {
            useBucketOn(session, heldBlock, heldSlot, held, hit)
            return ack()
        }
        val defaultStateId = heldBlock?.let { Blocks.REGISTRY.idOf(it) }
        if (held == null || heldBlock == null || defaultStateId == null) return ack()

        // parity: `BlockPlaceContext`: the clicked block is replaced when it is one of the things
        // that give way (grass, a fern, a snow layer, fire, a fluid), and the block goes next to it
        // otherwise. A flower, a firefly bush and leaves are not replaceable, so those get built on.
        val clickedForPlacing = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
        val replaceClicked =
            clickedForPlacing != null &&
                BlockShapes.isReplaceable(clickedForPlacing) &&
                BlockStateDefinitions.blockOf(clickedForPlacing) != heldBlock
        val placeAt = if (replaceClicked) hit.pos else hit.pos.offset(hit.direction)
        val existing = world.getBlockState(placeAt.x, placeAt.y, placeAt.z)
        if (existing == null || !BlockShapes.isReplaceable(existing)) return ack()

        val placedStateId =
            BlockPlacement.stateFor(
                block = heldBlock,
                defaultStateId = defaultStateId,
                face = hit.direction,
                clickY = hit.clickY,
                yaw = session.yaw,
                pitch = session.pitch,
                replacing = existing,
            )
        // The half a two-block one starts as, whatever the state table's first entry happens to be.
        val blockStateId = DoubleBlocks.firstHalf(placedStateId)
        // parity: `BlockItem.canPlace`: a block never goes down inside somebody. Without this you
        // can wall yourself in, which is the one thing vanilla will not let you do by accident.
        if (BlockPlacement.isObstructed(blockStateId, placeAt.x, placeAt.y, placeAt.z, buildersInTheWay())) {
            return ack()
        }
        // parity: `BlockItem.canPlace` also asks `canSurvive`, which is what refuses a torch hung
        // on nothing rather than letting it be placed and drop on the next update.
        val loadedWorld = currentWorld
        if (loadedWorld != null &&
            !NeighbourUpdates.isSupported(loadedWorld, placeAt.x, placeAt.y, placeAt.z, blockStateId)
        ) {
            return ack()
        }
        // parity: `setPlacedBy` on a door, a bed or a tall flower: the client asks for one block and
        // the game puts down two. Without the second one the first has no partner and `updateShape`
        // takes it straight back out, which is what made beds and doors impossible to place.
        val second = DoubleBlocks.secondHalf(blockStateId, placeAt.x, placeAt.y, placeAt.z)
        if (second != null) {
            val room = world.getBlockState(second.x, second.y, second.z)
            if (room == null || !BlockShapes.isReplaceable(room)) return ack()
        }
        val changed = world.setBlockState(placeAt.x, placeAt.y, placeAt.z, blockStateId)
        ack()
        if (!changed) return
        second?.let { world.setBlockState(it.x, it.y, it.z, it.stateId) }

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
        second?.let { broadcastBlockUpdate(BlockPos(it.x, it.y, it.z), it.stateId) }
        // Both the block under it (a snow layer put on grass) and the block itself (grass put
        // under snow) can change how they look.
        refreshSnowUnder(placeAt)
        refreshSnowAt(placeAt)
        notifyNeighbours(placeAt)
        if (Signs.isSign(blockStateId)) openNewSign(placeAt, blockStateId)
        currentWorld?.let { loaded ->
            for (block in FluidFlow.onBlockChanged(loaded, placeAt.x, placeAt.y, placeAt.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
            }
        }
    }

    /**
     * Using an item with nothing under the crosshair, which is how a bucket reaches a fluid.
     *
     * The ack comes first and unconditionally: the client already changed its own copy, and the ack
     * is what undoes it when the server disagrees.
     */
    private fun handleUseItem(useItem: ServerboundUseItemPacket) {
        sendCompressed(ClientboundBlockChangedAckPacket.ID) {
            ClientboundBlockChangedAckPacket(useItem.sequence).write(it)
        }
        val session = playerSession ?: return
        if (useItem.hand != ServerboundUseItemOnPacket.Hand.MAIN_HAND) return
        if (session.gamemode == Gamemode.SPECTATOR || session.gamemode == Gamemode.ADVENTURE) return
        val world = currentWorld ?: return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val item = Identifier.parse(held.id)
        if (startEating(session, item, heldSlot)) return
        if (!Buckets.isBucket(item)) return

        val result =
            Buckets.use(
                world = world,
                item = item,
                eyeX = session.x,
                eyeY = session.y + Fluids.eyeHeight(session.sneaking),
                eyeZ = session.z,
                yaw = useItem.yaw,
                pitch = useItem.pitch,
                creative = session.gamemode == Gamemode.CREATIVE,
            ) ?: return

        for (block in result.changes) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        val replacement = result.replacement ?: return
        session.inventory[heldSlot] = held.copy(id = replacement.toString(), count = 1)
        sendInventorySlot(heldSlot)
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
        val match = recipes.matchFor(CraftingInput.of(width, height, cells))
        // parity: the `limited_crafting` game rule, which is what makes `/recipe give` and
        // `/recipe take` mean something: with it on, a player crafts only what they have been
        // taught.
        val limited = currentWorld?.gameRules?.get(GameRuleRegistry.LIMITED_CRAFTING) == true
        val known =
            !limited ||
                match?.first?.let { progress?.knownRecipes(playerSession?.uuid?.toString().orEmpty())?.contains(it) } ==
                true
        craftingResult =
            match
                ?.takeIf { known }
                ?.second
                ?.let { StoredItemStack(it.item.toString(), it.count) }
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
     * The components of a stack the client sent back, as this build stores them.
     *
     * A creative client echoes a slot whenever the inventory screen opens, so anything not rebuilt
     * here is stripped off an item its owner merely looked at. What it cannot rebuild is what this
     * server never sent in the first place, since a client has nothing else to echo.
     */
    private fun componentsOf(wire: WireItemStack): NbtCompound =
        ItemComponentCodecs.toStored(wire.components) { enchantmentNameOf(it) }

    /**
     * Serverbound, id 0x38: creative inventory editing, and the only path a dragged item takes,
     * discarding it as slot `-1` included. Never the window-click packet.
     *
     * Ignored outside creative, as `ServerPlayerGameMode.handleCreativeModeItemAdd` does.
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
                StoredItemStack(id.toString(), wire.count, componentsOf(wire))
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

    /** A system message to every player on the server, whatever world they are in. */
    private fun broadcastToEveryone(message: Component) {
        for (player in playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(SystemChatPacket.ID) {
                SystemChatPacket(message, isActionBar = false).write(it)
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
        val feet = world.getBlockState(floor(x).toInt(), floor(y).toInt(), floor(z).toInt())
        val inWater = feet != null && Fluids.of(feet) == Submersion.WATER
        session.food.addExhaustion(
            FoodData.movementExhaustion(
                x - session.x,
                y - session.y,
                z - session.z,
                session.sprinting,
                onGround,
                inWater,
            ),
        )
        // parity: the server reads a jump off the packets, exactly here: it left the ground moving up.
        if (session.onGround && !onGround && y > session.y) {
            session.food.addExhaustion(FoodData.jumpExhaustion(session.sprinting))
            session.stats.increment(StoredStats.JUMP)
        }
        trackFall(session, y, onGround, feet)
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
            // The rotation above is the body. A head turns on its own, and without this one it only
            // ever tipped up and down while the face stayed pointing the same way.
            other.connection.sendPlayPacket(RotateHeadPacket.ID) {
                RotateHeadPacket(session.entityId, yaw).write(it)
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
        // No flush here: the outbound loop is the only drainer, so the client's requested rate is
        // honored, and the read thread never stalls on a burst of chunk encoding and zlib.
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
     * Driven from this connection's outbound loop and nowhere else. It used to run from the read
     * loop too, on every movement packet: that bypassed the client's own requested chunks-per-tick
     * budget and parked the read thread behind bursts of chunk encoding and zlib. Capped per call
     * so one flush cannot monopolize the socket.
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
                    // parity: vanilla only sends a chunk once it is FULL, which it cannot be until
                    // the eight around it exist: light worked out with a neighbour missing lets the
                    // sky in from nowhere, and the client keeps whatever it is sent. Under water
                    // that is the difference between a dim sea floor and one lit like open air.
                    if (!neighboursLoaded(pos.x, pos.z)) continue
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

    /** Whether the eight chunks around this one are in memory, asking for the ones that are not. */
    private fun neighboursLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean {
        var complete = true
        for (offsetZ in -1..1) {
            for (offsetX in -1..1) {
                if (offsetX == 0 && offsetZ == 0) continue
                val x = chunkX + offsetX
                val z = chunkZ + offsetZ
                if (world.isLoaded(x, z)) continue
                complete = false
                generationQueue.request(x, z, squaredDistanceToCenter(x, z))
            }
        }
        return complete
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

    /** Answers what the client is typing. A reply carries the request's own id or it is discarded. */
    private fun sendSuggestions(request: CommandSuggestionRequestPacket) {
        val session = playerSession ?: return
        val dispatcher = commands ?: return
        val source = PlayerCommandSource(session, dispatcher.environment.groups)
        val result = dispatcher.suggest(source, request.command)
        val packet = CommandSuggestionsPacket(request.id, result.start, result.length, result.suggestions)
        sendPlayPacket(CommandSuggestionsPacket.ID) { packet.write(it) }
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
        syncHeldAttributes()
    }

    /** Sends one effect, or its removal when it has run out. parity: the two mob effect packets. */
    internal fun sendEffect(effect: ActiveEffect) {
        val session = playerSession ?: return
        val id = MobEffects.protocolIdOf(effect.effect) ?: return
        // parity: `AbsorptionMobEffect.onEffectStarted` adds its hearts on top of whatever is left.
        if (effect.effect == MobEffects.ABSORPTION) {
            session.absorption += ABSORPTION_PER_LEVEL * (effect.amplifier + 1)
            syncAbsorption()
        }
        sendPlayPacket(UpdateMobEffectPacket.ID) {
            UpdateMobEffectPacket(
                entityId = session.entityId,
                effect = id,
                amplifier = effect.amplifier,
                durationTicks = effect.remainingTicks,
                ambient = effect.ambient,
                visible = effect.visible,
                showIcon = effect.showIcon,
            ).write(it)
        }
    }

    internal fun sendEffectRemoved(effect: Identifier) {
        val session = playerSession ?: return
        val id = MobEffects.protocolIdOf(effect) ?: return
        // parity: `AbsorptionMobEffect.onEffectRemoved` takes back exactly what it gave, and here
        // nothing else gives any, so the pool empties.
        if (effect == MobEffects.ABSORPTION) {
            session.absorption = 0f
            syncAbsorption()
        }
        sendPlayPacket(RemoveMobEffectPacket.ID) { RemoveMobEffectPacket(session.entityId, id).write(it) }
    }

    internal fun syncHeldAttributes() {
        val session = playerSession ?: return
        val held = session.inventory[PlayerInventory.HOTBAR_START + session.inventory.heldSlot]
        val snapshots = EnchantmentAttributes.snapshotsFor(held) { enchantments?.definitionOf(it) }
        sendPlayPacket(UpdateAttributesPacket.ID) {
            UpdateAttributesPacket(session.entityId, snapshots).write(it)
        }
    }

    /**
     * One slot changed, for when re-sending forty of them would be silly. [slot] is this server's
     * own storage index; it goes out translated to container 0's own numbering (see
     * [PlayerInventory.menuSlotOf]), because the client addresses its window, not our array.
     */
    internal fun sendInventorySlot(slot: Int) {
        val session = playerSession ?: return
        if (slot == PlayerInventory.HOTBAR_START + session.inventory.heldSlot) syncHeldAttributes()
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
        checkAdvancements(session)
        if (session.invulnerableTicks > 0) session.invulnerableTicks--
        if (session.dead) return
        tryPickupNearbyItems()
        tickEating(session)
        if (session.sleepingBed != null) session.sleepTicks++
        tickDigging(session)
        if (session.gamemode == Gamemode.CREATIVE || session.gamemode == Gamemode.SPECTATOR) return

        val healthBefore = session.health
        val foodBefore = session.food.foodLevel
        val saturationBefore = session.food.saturationLevel
        session.food.tick(
            difficulty = difficulty,
            health = session.health,
            maxHealth = PlayerSession.MAX_HEALTH,
            heal = { amount -> session.health = minOf(session.health + amount, PlayerSession.MAX_HEALTH) },
            hurt = { amount -> hurt(amount, DEATH_STARVE, DamageTypes.STARVE) },
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

    /**
     * Fires the inventory trigger once a tick, when something moved.
     *
     * parity: `ServerPlayer.inventoryMenu.broadcastChanges`, which is where vanilla checks it too:
     * once per tick after everything has moved, not once per item, so crafting a stack does not
     * check the same advancement sixty-four times.
     */
    private fun checkAdvancements(session: PlayerSession) {
        val triggers = advancementTriggers ?: return
        if (!session.inventory.changed) return
        session.inventory.clearChanged()
        triggers.onInventoryChanged(session.name, session.uuid.toString(), session.inventory.snapshot().slots)
    }

    /**
     * parity: `LivingEntity.hurt`, minus absorption, which no effect here grants yet.
     *
     * [damageType] is what decides how much of this armour and enchantments stop; a hit that does
     * not name one is taken as going straight through, which is what almost every source in this
     * build does anyway (`bypasses_armor` holds starving, drowning, withering and the rest).
     */
    internal fun hurt(
        amount: Float,
        deathMessageId: String,
        damageType: Identifier? = null,
    ) {
        val session = playerSession ?: return
        if (session.dead || session.invulnerable) return
        // parity: `LivingEntity.hurt`'s cooldown: inside the half second after a hit, only a bigger
        // one lands, and only by the difference. It is what stops lava and burning stacking.
        var incoming = amount
        val cooling = session.invulnerableTicks > COOLDOWN_HALF_TICKS
        if (cooling) {
            if (amount <= session.lastHurt) return
            incoming = amount - session.lastHurt
            session.lastHurt = amount
        } else {
            session.lastHurt = amount
            session.invulnerableTicks = HURT_COOLDOWN_TICKS
        }
        val reduced = reduced(session, incoming, damageType)
        if (reduced <= 0f) return
        if (!cooling) sendDamageEvent(session, damageType)
        // parity: `LivingEntity.hurt` takes it out of absorption first, and only the rest off health.
        val absorbed = minOf(session.absorption, reduced)
        if (absorbed > 0f) {
            session.absorption -= absorbed
            syncAbsorption()
        }
        val damage = reduced - absorbed
        if (damage <= 0f) return
        session.stats.addDamageTaken(damage)
        session.health = maxOf(session.health - damage, 0f)
        if (session.health <= 0f) die(deathMessageId) else syncHealth()
    }

    /** parity: `getDamageAfterArmorAbsorb` then `getDamageAfterMagicAbsorb`, in that order. */
    private fun reduced(
        session: PlayerSession,
        amount: Float,
        damageType: Identifier?,
    ): Float {
        val tags = damageTypeTags
        if (tags == null || damageType == null) return amount
        var damage = amount
        if (!tags.isIn(RealDamageTypeTags.BYPASSES_ARMOUR, damageType)) {
            var armour = 0f
            var toughness = 0f
            for (stack in wornArmour(session)) {
                val piece = stack?.let { ItemArmour.of(Identifier.parse(it.id)) } ?: continue
                armour += piece.armour
                toughness += piece.toughness
            }
            if (armour > 0f) damage = Combat.afterArmour(damage, armour, toughness)
        }
        if (tags.isIn(RealDamageTypeTags.BYPASSES_EFFECTS, damageType)) return damage
        val resistance = session.effects[MobEffects.RESISTANCE]
        if (resistance != null && !tags.isIn(RealDamageTypeTags.BYPASSES_RESISTANCE, damageType)) {
            damage = Combat.afterResistance(damage, resistance.amplifier)
        }
        if (damage <= 0f || tags.isIn(RealDamageTypeTags.BYPASSES_ENCHANTMENTS, damageType)) return damage
        val enchantments = enchantments ?: return damage
        val protection =
            DamageProtection.of(wornArmour(session), damageType, tags) { enchantments.definitionOf(it) }
        return if (protection > 0f) Combat.afterProtection(damage, protection) else damage
    }

    private fun wornArmour(session: PlayerSession): List<StoredItemStack?> =
        (PlayerInventory.ARMOUR_START until PlayerInventory.ARMOUR_START + PlayerInventory.ARMOUR_SLOTS)
            .map { session.inventory[it] }

    /** parity: `LivingEntity.heal`, capped at the maximum and reported like any other change. */
    internal fun heal(amount: Float) {
        val session = playerSession ?: return
        if (session.dead) return
        val healed = minOf(session.health + amount, PlayerSession.MAX_HEALTH)
        if (healed == session.health) return
        session.health = healed
        syncHealth()
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
        // parity: `LivingEntity.die` empties the absorption pool, so respawning starts without it.
        session.absorption = 0f
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
        // parity: `ServerPlayer.findRespawnPositionAndUseSpawnBlock`, minus the bed and anchor
        // checks: what `/spawnpoint` set wins, and the world spawn is the fallback.
        val stored = playerStore?.takeIf { it.isOpen }?.access?.spawnPointOf(session.uuid.toString())
        val respawnWorld = stored?.world?.let { worlds[it] } ?: requireNotNull(currentWorld)
        val (spawnX, spawnY, spawnZ) =
            if (stored != null) Triple(stored.x, stored.y, stored.z) else respawnWorld.spawn
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
        // parity: `PlayerList.sendLevelInfo` again, which is what a respawn goes through too.
        currentWorld?.let { respawnWorld ->
            sendTime(respawnWorld)
            if (respawnWorld.weather.raining) sendWeather(respawnWorld.weather)
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
     * [LoadedWorld.levelKey]: the respawn packet makes it throw away its chunk cache and rebuild,
     * and it only does that when the key differs.
     *
     * What everyone *else* sees is deliberately not sent from here: each connection's [syncPlayers]
     * owns its tracking set, and writing a removal straight to another player's socket left that
     * set out of date, so coming back never spawned this player again.
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

        currentWorld = target
        session.worldName = target.name
        session.x = x
        session.y = y
        session.z = z

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
        // parity: `Entity.teleportTo` clears the fall, so arriving is never a landing.
        fallDistance = 0.0
        // Straight to updatePosition rather than waiting for the client to confirm: the world has
        // to agree the player is there now, so chunks around the destination start generating
        // immediately instead of one round trip later.
        updatePosition(x, y, z, yaw, pitch, onGround = false)
    }

    /**
     * The shared flags byte: everything about how this player is standing that fits in eight bits.
     *
     * parity: `Entity.DATA_SHARED_FLAGS_ID`. One byte for all of them, which is why it is composed
     * in one place: whoever writes only the bit they care about erases the others.
     */
    private fun sharedFlags(session: PlayerSession): Int {
        var flags = 0
        if (session.fireTicks > 0) flags = flags or SetEntityDataPacket.FLAG_ON_FIRE
        if (session.sneaking) flags = flags or SetEntityDataPacket.FLAG_SNEAKING
        if (session.sprinting) flags = flags or SetEntityDataPacket.FLAG_SPRINTING
        return flags
    }

    /**
     * How this player looks to everyone else: crouched, sprinting, burning, and the pose that goes
     * with it. Sent only when it changes, which for a crouch is a handful of times a minute.
     */
    private fun syncPosture() {
        val session = playerSession ?: return
        if (session.sleepingBed != null) return
        val flags = sharedFlags(session)
        // parity: `Player.updatePlayerPose`, of which this build has the two poses it can tell
        // apart: crouching and standing.
        val pose = if (session.sneaking) Pose.CROUCHING else Pose.STANDING
        if (flags == lastSharedFlags && pose == lastPose) return
        lastSharedFlags = flags
        lastPose = pose
        val packet = SetEntityDataPacket.posture(session.entityId, flags, pose)
        sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    /** The air left and whether this player burns, to them and to everyone who can see them. */
    internal fun syncSubmersion() {
        val session = playerSession ?: return
        val packet = SetEntityDataPacket.submersion(session.entityId, session.airSupply, sharedFlags(session))
        sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    internal fun syncHealth() {
        val session = playerSession ?: return
        sendCompressed(SetHealthPacket.ID) {
            SetHealthPacket(session.health, session.food.foodLevel, session.food.saturationLevel).write(it)
        }
    }

    /**
     * Starts eating what is in hand, if it is food and there is room for it.
     *
     * parity: `ServerPlayer.startUsingItem`. The server times the mouthful itself rather than
     * trusting the client to say when it finished, which is what stops one click feeding a player
     * twice.
     */
    private fun startEating(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
    ): Boolean {
        val food = ItemFood.of(item) ?: return false
        if (!food.canAlwaysEat && session.food.foodLevel >= FoodData.MAX_FOOD) return true
        eatingSlot = slot
        eatingTicks = food.consumeTicks
        return true
    }

    private fun stopEating() {
        eatingSlot = -1
        eatingTicks = 0
    }

    /** One tick of the mouthful, and the swallow at the end of it. */
    private fun tickEating(session: PlayerSession) {
        if (eatingSlot < 0) return
        val held = session.inventory[eatingSlot]
        val food = held?.let { ItemFood.of(Identifier.parse(it.id)) }
        // Anything that moved the stack under them (dropping it, a click, swapping slots) stops it.
        if (food == null || eatingSlot != PlayerInventory.HOTBAR_START + session.inventory.heldSlot) {
            stopEating()
            return
        }
        if (--eatingTicks > 0) return
        val slot = eatingSlot
        stopEating()
        session.food.eat(food.nutrition, food.saturation)
        applyConsumeEffects(session, food.effects)
        if (session.gamemode != Gamemode.CREATIVE) {
            val remaining = held.count - 1
            // parity: `use_remainder` replaces what was eaten: the bowl, the bottle, the bucket.
            val leftover = food.remainder?.let { held.copy(id = it.toString(), count = 1) }
            session.inventory[slot] =
                when {
                    remaining > 0 -> held.copy(count = remaining)
                    else -> leftover
                }
            sendInventorySlot(slot)
            // A stack of two bowls of stew leaves the second bowl looking for a slot of its own,
            // and on the floor when there is none.
            if (remaining > 0 && leftover != null && session.inventory.add(leftover) > 0) {
                spawnDroppedItem(
                    item = Identifier.parse(leftover.id),
                    count = 1,
                    x = session.x,
                    y = session.y + THROW_EYE_HEIGHT,
                    z = session.z,
                )
            }
            if (remaining > 0 && leftover != null) sendInventory()
        }
        syncHealth()
    }

    /**
     * How far this player has fallen, and what it costs them when they land.
     *
     * parity: `Entity.checkFallDamage` plus `LivingEntity.calculateFallDamage`, which is
     * `floor(distance - 3)`: the distance is added up from the movement itself, not asked of the
     * client, and anything that cancels a fall in vanilla (water, a ladder, flying, creative)
     * clears it instead.
     */
    private fun trackFall(
        session: PlayerSession,
        y: Double,
        onGround: Boolean,
        feetState: Int?,
    ) {
        val cancelled =
            session.flying ||
                session.gamemode == Gamemode.CREATIVE ||
                session.gamemode == Gamemode.SPECTATOR ||
                (feetState != null && (Fluids.of(feetState) != Submersion.NONE || isClimbable(feetState)))
        if (cancelled) {
            fallDistance = 0.0
            return
        }
        if (!onGround && y < session.y) fallDistance += session.y - y
        if (onGround && !session.onGround) {
            applyFallDamage(session)
            fallDistance = 0.0
        }
        if (onGround) fallDistance = 0.0
    }

    private fun applyFallDamage(session: PlayerSession) {
        if (currentWorld?.gameRules?.get(GameRuleRegistry.FALL_DAMAGE) == false) return
        val damage = Combat.fallDamage(fallDistance)
        if (damage <= 0) return
        session.stats.addFallen(fallDistance)
        hurt(damage.toFloat(), DEATH_FALL, DamageTypes.FALL)
    }

    /** parity: `BlockTags.CLIMBABLE`, which is what stops a fall without stopping the player. */
    private fun isClimbable(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path in CLIMBABLE_BLOCKS

    /**
     * Takes the snow off the block under [pos], or puts it on. parity: `SnowyBlock.updateShape`,
     * which vanilla runs through a block update and this build runs where the block changed.
     */
    private fun refreshSnowUnder(pos: BlockPos) = refreshSnowAt(BlockPos(pos.x, pos.y - 1, pos.z))

    private fun refreshSnowAt(pos: BlockPos) {
        val world = currentWorld ?: return
        val updated = SnowyBlocks.updated(world, pos.x, pos.y, pos.z) ?: return
        if (!world.state.setBlockState(pos.x, pos.y, pos.z, updated)) return
        broadcastBlockUpdate(pos, updated)
    }

    /**
     * The arm swing everyone else sees. parity: `ServerGamePacketListenerImpl.handleAnimate`, which
     * forwards it as `ClientboundAnimatePacket` to whoever is tracking that player.
     *
     * The swinging client animates its own arm, which is why this goes to the others and not back.
     */
    private fun handleSwing(hand: Int) {
        val session = playerSession ?: return
        val action = if (hand == OFFHAND) AnimatePacket.SWING_OFFHAND else AnimatePacket.SWING_MAIN_HAND
        val packet = AnimatePacket(session.entityId, action)
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(AnimatePacket.ID) { packet.write(it) }
        }
    }

    /** Starts drawing cracks on [pos] for everyone else, at the speed this player actually mines. */
    private fun startDigging(pos: BlockPos) {
        val session = playerSession ?: return
        if (session.gamemode != Gamemode.SURVIVAL && session.gamemode != Gamemode.ADVENTURE) return
        diggingAt = pos
        diggingTicks = 0
        diggingStage = -1
    }

    private fun stopDigging() {
        val pos = diggingAt ?: return
        diggingAt = null
        diggingStage = -1
        broadcastDestruction(pos, BlockDestructionPacket.NONE)
    }

    /**
     * One tick of the dig: works out how far along it is and, when the stage changes, tells everyone.
     *
     * The progress comes from the same [ItemTools.destroyProgressPerTick] the break itself uses, so
     * the cracks run at the speed the tool really mines at rather than a guess.
     */
    private fun tickDigging(session: PlayerSession) {
        val pos = diggingAt ?: return
        val state = currentWorld?.state?.getBlockState(pos.x, pos.y, pos.z)
        val block = state?.let(Blocks::byStateId)?.block
        if (block == null || Blocks.isAir(state)) {
            stopDigging()
            return
        }
        diggingTicks++
        val perTick = destroyProgressPerTick(block)
        if (perTick <= 0f) return
        val stage = ((diggingTicks * perTick) * BlockDestructionPacket.STAGES).toInt()
        if (stage >= BlockDestructionPacket.STAGES || stage == diggingStage) return
        diggingStage = stage
        broadcastDestruction(pos, stage)
    }

    private fun broadcastDestruction(
        pos: BlockPos,
        stage: Int,
    ) {
        val session = playerSession ?: return
        val packet = BlockDestructionPacket(session.entityId, pos, stage)
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(BlockDestructionPacket.ID) { packet.write(it) }
        }
    }

    /** parity: `BucketItem.use` with the block the client says it hit, rather than a second raycast. */
    private fun useBucketOn(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ) {
        val world = currentWorld ?: return
        val result =
            Buckets.useOn(
                world = world,
                item = item,
                x = hit.pos.x,
                y = hit.pos.y,
                z = hit.pos.z,
                face = hit.direction,
                creative = session.gamemode == Gamemode.CREATIVE,
            ) ?: return
        for (block in result.changes) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        val replacement = result.replacement ?: return
        session.inventory[slot] = held.copy(id = replacement.toString(), count = 1)
        sendInventorySlot(slot)
    }

    /**
     * The boxes of everything in this world that stops a block being placed inside it.
     *
     * parity: `Entity.blocksBuilding`, which is on for anything alive and off for dropped items and
     * arrows, plus spectators, who are built straight through.
     */
    private fun buildersInTheWay(): List<DoubleArray> {
        val session = playerSession ?: return emptyList()
        val players = playersInSameWorld().toMutableList()
        if (players.none { it.entityId == session.entityId }) players += session
        return players
            .filter { it.gamemode != Gamemode.SPECTATOR }
            .map { BlockPlacement.playerBox(it.x, it.y, it.z, it.sneaking) }
    }

    /** Opening a door, flipping a lever: the change goes to everyone, and so does the sound. */
    private fun toggleBlock(pos: BlockPos) {
        val world = currentWorld ?: return
        val before = world.state.getBlockState(pos.x, pos.y, pos.z)
        val toggled = BlockInteractions.toggle(world, pos.x, pos.y, pos.z) ?: return
        // parity: `ButtonBlock.press` schedules its own release. Everything else here stays put
        // until something touches it again.
        BlockInteractions.pressedTicks(before ?: 0)?.let { delay ->
            world.state.scheduleBlockTick(pos.x, pos.y, pos.z, BUTTON_TICK, delay)
        }
        for (block in toggled.changes) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        val sound =
            SoundPacket(
                sound = toggled.sound,
                source = SoundSource.BLOCKS,
                x = pos.x + 0.5,
                y = pos.y + 0.5,
                z = pos.z + 0.5,
                volume = 1f,
                pitch = 1f,
                seed = Random.nextLong(),
            )
        sendPlayPacket(SoundPacket.ID) { sound.write(it) }
        for (other in playersInSameWorld()) {
            if (other.entityId == playerSession?.entityId) continue
            other.connection.sendPlayPacket(SoundPacket.ID) { sound.write(it) }
        }
    }

    /**
     * Clicking a bed: sleep in it, or say why not.
     *
     * parity: `BedBlock.useWithoutItem` over `ServerPlayer.startSleepInBed`. The spawn point is set
     * before the night check, as vanilla does, so a bed clicked in daylight still becomes home.
     */
    private fun useBed(
        session: PlayerSession,
        pos: BlockPos,
        state: Int,
    ) {
        val world = currentWorld ?: return
        val step = Beds.headOffset(state)
        val head = if (Beds.isHead(state)) pos else BlockPos(pos.x + step[0], pos.y + step[1], pos.z + step[2])
        val headState = world.state.getBlockState(head.x, head.y, head.z) ?: return
        if (!Beds.isBed(headState)) return

        if (Beds.explodesIn(world.declaration.dimension)) {
            for (block in Beds.destroy(world, head.x, head.y, head.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
            }
            sendActionBar(Component.translatable("block.minecraft.bed.not_valid"))
            // parity: `BedBlock.useWithoutItem`, which does not merely refuse: it takes the bed out
            // and sets off an explosion of five where it stood.
            blowUp(world, head.x + 0.5, head.y + 0.5, head.z + 0.5, Explosions.BED_POWER)
            return
        }
        // parity: `ServerPlayer.startSleepInBed` sets the spawn point once the bed is reachable and
        // free, and only then asks whether the night allows sleeping.
        val reachProblem = bedReachProblem(session, head, headState)
        if (reachProblem != null) {
            sendActionBar(Component.translatable(reachProblem.translationKey))
            return
        }
        setSpawnPointAt(session, head)
        if (!Beds.isNight(world.clock.totalTicks) && !world.weather.thundering) {
            sendActionBar(Component.translatable(BedProblem.NOT_POSSIBLE_NOW.translationKey))
            return
        }

        world.state.setBlockState(head.x, head.y, head.z, Beds.withOccupied(headState, occupied = true))
        broadcastBlockUpdate(head, Beds.withOccupied(headState, occupied = true))
        session.sleepingBed = Triple(head.x, head.y, head.z)
        session.sleepTicks = 0
        val packet = SetEntityDataPacket.sleeping(session.entityId, head)
        sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    private fun bedReachProblem(
        session: PlayerSession,
        head: BlockPos,
        headState: Int,
    ): BedProblem? {
        if (Beds.isOccupied(headState)) return BedProblem.OCCUPIED
        val dx = session.x - (head.x + 0.5)
        val dy = session.y - head.y
        val dz = session.z - (head.z + 0.5)
        if (dx * dx + dy * dy + dz * dz > Beds.RANGE * Beds.RANGE) return BedProblem.TOO_FAR_AWAY
        return null
    }

    /** parity: the bed becomes the respawn point whether or not the night lets anyone sleep. */
    private fun setSpawnPointAt(
        session: PlayerSession,
        bed: BlockPos,
    ) {
        val access = playerStore?.takeIf { it.isOpen }?.access ?: return
        access.setSpawnPoint(
            StoredSpawnPoint(
                uuid = session.uuid.toString(),
                world = session.worldName,
                x = bed.x + 0.5,
                y = bed.y.toDouble(),
                z = bed.z + 0.5,
                yaw = session.yaw,
            ),
        )
        sendActionBar(Component.translatable("block.minecraft.set_spawn"))
    }

    /** Out of bed: the flag on the block and the pose. Nobody was moved into it, so nobody moves out. */
    internal fun wakeUp() {
        val session = playerSession ?: return
        val bed = session.sleepingBed ?: return
        session.sleepingBed = null
        session.sleepTicks = 0
        val world = currentWorld
        val state = world?.state?.getBlockState(bed.first, bed.second, bed.third)
        if (world != null && state != null && Beds.isBed(state)) {
            val awake = Beds.withOccupied(state, occupied = false)
            world.state.setBlockState(bed.first, bed.second, bed.third, awake)
            broadcastBlockUpdate(BlockPos(bed.first, bed.second, bed.third), awake)
        }
        val packet = SetEntityDataPacket.sleeping(session.entityId, null)
        sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    private fun sendActionBar(message: Component) {
        sendPlayPacket(SystemChatPacket.ID) { SystemChatPacket(message, isActionBar = true).write(it) }
    }

    /** parity: `ServerGamePacketListenerImpl`'s `SWAP_ITEM_WITH_OFFHAND`: the F key. */
    private fun swapWithOffhand() {
        val session = playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        stopEating()
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        session.inventory[heldSlot] = session.inventory[PlayerInventory.OFFHAND_SLOT]
        session.inventory[PlayerInventory.OFFHAND_SLOT] = held
        sendInventorySlot(heldSlot)
        sendInventorySlot(PlayerInventory.OFFHAND_SLOT)
        syncHeldAttributes()
    }

    /** parity: `Consumable.onConsume`, which is what makes a golden apple worth eating. */
    private fun applyConsumeEffects(
        session: PlayerSession,
        effects: List<ConsumeEffect>,
    ) {
        for (effect in effects) {
            when (effect) {
                is ConsumeEffect.ClearAll -> {
                    for (id in session.effects.keys.toList()) {
                        session.effects.remove(id)
                        sendEffectRemoved(id)
                    }
                }
                is ConsumeEffect.Remove -> {
                    if (session.effects.remove(effect.effect) != null) sendEffectRemoved(effect.effect)
                }
                is ConsumeEffect.Apply -> {
                    if (effect.probability < 1f && Random.nextFloat() >= effect.probability) continue
                    val active =
                        ActiveEffect(
                            effect = effect.effect,
                            amplifier = effect.amplifier,
                            remainingTicks = effect.durationTicks,
                        )
                    session.effects[effect.effect] = active
                    sendEffect(active)
                }
            }
        }
    }

    /** The red flash and the hurt sound, which the client plays off this and nothing else. */
    private fun sendDamageEvent(
        session: PlayerSession,
        damageType: Identifier?,
    ) {
        val registryId = VanillaRegistryData.damageTypes.indexOfFirst { it.first == (damageType?.path ?: "generic") }
        if (registryId < 0) return
        val packet = DamageEventPacket(entityId = session.entityId, damageType = registryId)
        sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        }
    }

    /** parity: absorption rides on the player's own entity data, not on `set_health`. */
    private fun syncAbsorption() {
        val session = playerSession ?: return
        sendCompressed(SetEntityDataPacket.ID) {
            SetEntityDataPacket.playerAbsorption(session.entityId, session.absorption).write(it)
        }
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { buffer ->
                SetEntityDataPacket.playerAbsorption(session.entityId, session.absorption).write(buffer)
            }
        }
    }

    /** This world's clocks, as the join sequence and every `/time` send them. */
    internal fun sendTime(world: LoadedWorld) {
        val packet = world.timePacket()
        sendPlayPacket(SetTimePacket.ID) { packet.write(it) }
    }

    /**
     * parity: `ServerPlayer.setPlayerWeather`/`PlayerList.sendLevelInfo`, which announces rain as a
     * start/stop event *and* the two levels: the events swap the client's weather renderer, the
     * levels set how heavy it is, and a client sent only one of the two shows the wrong thing.
     */
    internal fun sendWeather(weather: WorldWeather) {
        sendPlayPacket(GameEventPacket.ID) {
            GameEventPacket(
                if (weather.raining) GameEventPacket.START_RAINING else GameEventPacket.STOP_RAINING,
                0f,
            ).write(it)
        }
        sendPlayPacket(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.RAIN_LEVEL_CHANGE, weather.rainLevel).write(it)
        }
        sendPlayPacket(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.THUNDER_LEVEL_CHANGE, weather.thunderLevel).write(it)
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
        sendPermissionLevel()
        val tree = dispatcher.treeFor(PlayerCommandSource(session, dispatcher.environment.groups))
        sendCompressed(CommandsPacket.ID) { tree.write(it) }
    }

    /**
     * parity: `PlayerList.sendPlayerPermissionLevel`, which goes right before the command tree.
     *
     * The client parses commands locally against this: below gamemaster it refuses selectors, so
     * without it `/tellraw @a ...` is underlined in red even though the server runs it fine.
     */
    private fun sendPermissionLevel() {
        val session = playerSession ?: return
        val dispatcher = commands ?: return
        val level = PlayerCommandSource(session, dispatcher.environment.groups).permissionLevel
        val packet = EntityEventPacket.permissionLevel(session.entityId, level)
        sendPlayPacket(EntityEventPacket.ID) { packet.write(it) }
    }

    private fun leavePlayIfPresent() {
        val session = playerSession ?: return
        val registry = playerRegistry ?: return
        // Leaving while asleep would otherwise leave the bed occupied for ever.
        wakeUp()
        // Before removing them from the registry, so a save triggered elsewhere cannot race this
        // one into writing a player who is already half gone.
        savePlayerState()
        registry.remove(session.uuid)
        progress?.unload(session.uuid.toString())
        logger.info { "${session.name} left the game" }
        // parity: `ServerGamePacketListenerImpl.onDisconnect`, the other half of the pair above.
        broadcastToEveryone(
            Component
                .translatable("multiplayer.player.left", Component.text(session.name))
                .color(TextColor.YELLOW),
        )
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
        PacketScratch.reset()
        val body = PacketScratch.body
        body.writeVarInt(id)
        writeBody(body)
        val frame = PacketScratch.frame
        PacketFraming.writeUncompressed(frame, body.toByteArray())
        writeToSocket(frame)
    }

    private fun sendCompressed(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        PacketScratch.reset()
        val body = PacketScratch.body
        body.writeVarInt(id)
        writeBody(body)
        val frame = PacketScratch.frame
        PacketFraming.writeCompressed(frame, body.toByteArray(), compressionThreshold, Zlib::compress)
        writeToSocket(frame)
    }

    /**
     * One packet reaches the socket at a time. This connection's own thread is not the only writer:
     * another player's thread broadcasts movement and chat straight into this session, and the tick
     * loop writes keep-alives, so without this two writers interleave their bytes mid-packet and
     * the client desynchronizes on the very next frame length. The cipher is inside the lock
     * because CFB8 is stateful: encrypting two packets concurrently corrupts both even if the
     * writes themselves were ordered.
     *
     * The frame goes to the socket straight from its buffer: it is this thread's own scratch, and
     * the send mutex means nobody grows it mid-write.
     */
    private fun writeToSocket(frame: ByteBuffer) {
        sendMutex.withLock {
            val cipher = encryptCipher
            frame.withReadable { array, offset, length ->
                if (cipher != null) {
                    connection.write(cipher.update(array, offset, length, encrypting = true))
                } else {
                    connection.write(array, offset, length)
                }
            }
        }
    }

    private companion object {
        /** The one enchantment that changes how fast a block comes apart. */
        val EFFICIENCY = Identifier.vanilla("efficiency")

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
        const val DEATH_FALL = "fall"

        val CLIMBABLE_BLOCKS =
            setOf(
                "ladder",
                "vine",
                "scaffolding",
                "twisting_vines",
                "twisting_vines_plant",
                "weeping_vines",
                "weeping_vines_plant",
                "cave_vines",
                "cave_vines_plant",
            )

        /** The largest crafting grid any window here has, so one array serves both. */
        const val MAX_GRID_SIZE = 9

        /** Container ids for opened blocks. 0 is the player's own window and is never reused. */
        const val FIRST_BLOCK_CONTAINER_ID = 1

        /** Wraps well before the varint gets long; vanilla wraps at 100 for the same reason. */
        const val MAX_CONTAINER_ID = 100

        val CRAFTING_TABLE: Identifier = Identifier.vanilla("crafting_table")

        val FLINT_AND_STEEL: Identifier = Identifier.vanilla("flint_and_steel")

        val ENDER_EYE: Identifier = Identifier.vanilla("ender_eye")

        /** Fire's default state: this build has no model for which sides of it are burning. */
        val FIRE_STATE: Int = Blocks.REGISTRY.idOf(Identifier.vanilla("fire")) ?: Blocks.AIR.stateId

        /** The two blocks `IceBlock` covers: packed and blue ice are other blocks and do not melt. */
        val ICE_STATES: Set<Int> =
            listOfNotNull(
                Blocks.ICE.stateId,
                Blocks.REGISTRY.idOf(Identifier.vanilla("frosted_ice")),
            ).toSet()

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

        /**
         * How much ground is generated before the player is let in, in chunks around them.
         *
         * Two is what a client needs to stop showing "loading terrain"; the rest arrives while
         * they are already standing there, which is what vanilla's chunk batches are for.
         */
        const val LOGIN_CHUNK_RADIUS = 2

        /**
         * The slots [syncEquipmentOf] sends, in the order it reads them: hands first, then the
         * armour from the feet up, which is the order `PlayerInventory` keeps them in.
         */
        val EQUIPMENT_SLOTS =
            listOf(
                SetEquipmentPacket.Type.MAINHAND,
                SetEquipmentPacket.Type.OFFHAND,
                SetEquipmentPacket.Type.HEAD,
                SetEquipmentPacket.Type.CHEST,
                SetEquipmentPacket.Type.LEGS,
                SetEquipmentPacket.Type.FEET,
            )

        /** What a scheduled block tick on a button is filed under. */
        val BUTTON_TICK = Identifier.vanilla("button")

        /** `minecraft:move_player_rot` and `minecraft:move_player_status_only`, read directly. */
        const val MOVE_PLAYER_ROT_ID = 0x20
        const val MOVE_PLAYER_STATUS_ONLY_ID = 0x21

        /** `minecraft:swing`, which this build reads directly: its body is one varint, the hand. */
        const val SWING_PACKET_ID = 0x3f
        const val OFFHAND = 1

        /** parity: `AbsorptionMobEffect`, two hearts per level. */
        const val ABSORPTION_PER_LEVEL = 4f

        /** parity: `LivingEntity.invulnerableTime`, and the ten ticks of it that still let a hit in. */
        const val HURT_COOLDOWN_TICKS = 20
        const val COOLDOWN_HALF_TICKS = 10

        const val THROW_EYE_HEIGHT = 1.5
        const val BREAK_DROP_LIFT = 0.2

        /** A block's own drop is meant to be walked into immediately, unlike a deliberate toss. */
        const val BREAK_DROP_PICKUP_DELAY = 10

        /** How far a client is told about dropped items, squared. Well inside the view distance. */
        const val ITEM_VIEW_RADIUS_SQUARED = 64.0 * 64.0

        /** All generated End entity types use vanilla's 10-chunk client tracking range. */
        const val GENERATED_ENTITY_VIEW_RADIUS_SQUARED = 160.0 * 160.0

        /** The same radius in chunks, so the world scan and the distance filter cannot drift apart. */
        val GENERATED_ENTITY_VIEW_RADIUS_CHUNKS: Int =
            ceil(sqrt(GENERATED_ENTITY_VIEW_RADIUS_SQUARED) / 16.0).toInt()

        val GENERATED_ENTITY_TYPE_IDS =
            mapOf(
                "end_crystal" to 45,
                "armor_stand" to 5,
                "camel" to 19,
                "cat" to 21,
                "cow" to 30,
                "horse" to 66,
                "iron_golem" to 70,
                "item_frame" to 73,
                "pig" to 100,
                "sheep" to 111,
                "shulker" to 112,
                "villager" to 140,
                "zombie_villager" to 154,
            )
    }
}

/** [org.basaltmc.wire.protocol.configuration.ConfigurationKeepAlivePacket]'s id, matched here just to no-op it explicitly. */
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

/**
 * Per-thread scratch for building outbound packets, so the thousands of small sends a second stop
 * allocating two buffers each. Thread-local because broadcasts write into a session from other
 * players' threads; safe because nothing sends a packet while serializing another.
 */
@ThreadLocal
private object PacketScratch {
    /** Buffers grown past this by a chunk packet are dropped, not retained per thread forever. */
    private const val RETAIN_CAPACITY = 64 * 1024

    var body: ByteBuffer = ByteBuffer()
        private set

    var frame: ByteBuffer = ByteBuffer()
        private set

    /** Fresh cursors for the next packet, shedding oversized backing arrays. */
    fun reset() {
        if (body.capacity > RETAIN_CAPACITY) body = ByteBuffer() else body.clear()
        if (frame.capacity > RETAIN_CAPACITY) frame = ByteBuffer() else frame.clear()
    }
}
