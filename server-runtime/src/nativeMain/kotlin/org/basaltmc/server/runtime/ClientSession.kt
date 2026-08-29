package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.plugins.PluginPacketBus
import org.basaltmc.server.plugins.PluginSurfaceContent
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.ConsumeEffect
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.PlayerCommandSource
import org.basaltmc.server.runtime.command.commands.BossBarRegistry
import org.basaltmc.server.runtime.entity.DamageSource
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.net.ProxyLogin
import org.basaltmc.server.runtime.net.SessionOutbox
import org.basaltmc.server.runtime.player.Difficulty
import org.basaltmc.server.runtime.player.FoodData
import org.basaltmc.server.runtime.player.Menu
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerPersistence
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.NpcRegistry
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.registry.VanillaConfigurationRegistries
import org.basaltmc.server.runtime.registry.VanillaTagData
import org.basaltmc.server.runtime.ui.PlayerUi
import org.basaltmc.server.runtime.ui.UiConfigs
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.AdvancementTriggers
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.Jukeboxes
import org.basaltmc.server.runtime.world.LoadedWorld
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
import org.basaltmc.server.runtime.world.RealVillagerTrades
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.runtime.world.WorldWeather
import org.basaltmc.server.storage.BanKind
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.server.storage.StoredStats
import org.basaltmc.wire.auth.Cfb8Cipher
import org.basaltmc.wire.auth.MinecraftCrypt
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.auth.OnlineMode
import org.basaltmc.wire.auth.SessionProfileProperty
import org.basaltmc.wire.auth.forwarding.ForwardingResult
import org.basaltmc.wire.auth.forwarding.ProxyForwarding
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
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
import org.basaltmc.wire.multiversion.ProtocolChain
import org.basaltmc.wire.multiversion.ProtocolGate
import org.basaltmc.wire.multiversion.ProtocolVerdict
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.multiversion.SubstitutionLabels
import org.basaltmc.wire.multiversion.UnknownProtocolPolicy
import org.basaltmc.wire.multiversion.java.JavaTranslators
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.PacketFraming
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.VarInt
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
import org.basaltmc.wire.protocol.login.LoginPluginRequestPacket
import org.basaltmc.wire.protocol.login.LoginPluginResponsePacket
import org.basaltmc.wire.protocol.login.LoginStartPacket
import org.basaltmc.wire.protocol.play.AcceptTeleportationPacket
import org.basaltmc.wire.protocol.play.AttackPacket
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.BossEventPacket
import org.basaltmc.wire.protocol.play.ChatCommandPacket
import org.basaltmc.wire.protocol.play.ChatMessagePacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.ClientCommandPacket
import org.basaltmc.wire.protocol.play.ClientboundTakeItemEntityPacket
import org.basaltmc.wire.protocol.play.CommandSuggestionRequestPacket
import org.basaltmc.wire.protocol.play.CommandSuggestionsPacket
import org.basaltmc.wire.protocol.play.ContainerButtonClickPacket
import org.basaltmc.wire.protocol.play.ContainerClosePacket
import org.basaltmc.wire.protocol.play.CustomPayloadPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.GlobalPos
import org.basaltmc.wire.protocol.play.InteractPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.PlayDisconnectPacket
import org.basaltmc.wire.protocol.play.PlayKeepAlivePacket
import org.basaltmc.wire.protocol.play.PlayerCommandPacket
import org.basaltmc.wire.protocol.play.PlayerInfoEntry
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.play.PlayerLoadedPacket
import org.basaltmc.wire.protocol.play.PlayerPositionPacket
import org.basaltmc.wire.protocol.play.RotateHeadPacket
import org.basaltmc.wire.protocol.play.SelectTradePacket
import org.basaltmc.wire.protocol.play.ServerboundContainerClickPacket
import org.basaltmc.wire.protocol.play.ServerboundPickItemFromBlockPacket
import org.basaltmc.wire.protocol.play.ServerboundPickItemFromEntityPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerAbilitiesPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionLookPacket
import org.basaltmc.wire.protocol.play.ServerboundPositionPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCarriedItemPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCreativeModeSlotPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SignUpdatePacket
import org.basaltmc.wire.protocol.play.SignedChatCommandPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.protocol.play.SpawnInfo
import org.basaltmc.wire.protocol.play.SpawnPositionPacket
import org.basaltmc.wire.protocol.play.StonecutterOption
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.play.UpdateRecipesPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.status.PingRequestPacket
import org.basaltmc.wire.protocol.status.PongResponsePacket
import org.basaltmc.wire.protocol.status.StatusRequestPacket
import org.basaltmc.wire.protocol.status.StatusResponsePacket
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.LangTable
import org.basaltmc.wire.text.MiniMessage
import org.basaltmc.wire.text.TextColor
import org.basaltmc.wire.text.resolvePlainText
import kotlin.concurrent.AtomicInt
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.roundToLong
import kotlin.math.sqrt
import kotlin.native.concurrent.ThreadLocal
import kotlin.random.Random
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Drives one client connection through Handshake, (Status | Login), Configuration and Play.
 *
 * Login is offline-mode when [onlineMode] is `null`; otherwise [HelloPacket]/[KeyPacket] negotiate
 * an AES-128/CFB8 secret and everything from [LoginCompressionPacket] on is enciphered.
 *
 * Play streams chunks through [ChunkStreamer] and wires movement, chat, breaking and placing
 * through [playerRegistry] and [WorldState]'s lock. This connection's thread also runs
 * [PlayerLifecycle.tickPlayer], [tryPickupNearbyItems], commands, death and respawn.
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
    /** Which proxy, if any, is speaking for this connection. Disabled means nobody is. */
    private val forwarding: ProxyForwarding = ProxyForwarding.DISABLED,
    internal val playerRegistry: PlayerRegistry? = null,
    /** Every loaded world, by name: a joining player goes into the one their stored row names. */
    internal val worlds: Map<String, LoadedWorld> = emptyMap(),
    private val defaultWorld: LoadedWorld? = null,
    internal val commands: CommandDispatcher? = null,
    internal val defaultGamemode: Gamemode = Gamemode.SURVIVAL,
    internal val difficulty: Difficulty = Difficulty.EASY,
    internal val playerStore: PlayerStore? = null,
    /** Whether where this connection came from is worth remembering. */
    private val rememberIp: Boolean = false,
    internal val ui: () -> UiConfigs = { UiConfigs() },
    private val placeholders: PlaceholderRegistry = PlaceholderRegistry(),
    /** Where this connection's share of the server's time is recorded. Off unless `/timings` is on. */
    private val timings: Timings = Timings(),
    /** Where a survival block break resolves its drops from. `null` means blocks never drop anything. */
    internal val lootTables: RealLootTables? = null,
    /** Every crafting recipe the server knows. `null` means nothing can be crafted. */
    internal val recipes: RealRecipes? = null,
    /** What a villager offers, out of the jar's own data. `null` means nobody trades. */
    internal val villagerTrades: RealVillagerTrades? = null,
    /** The songs a jukebox can play, for the number a comparator reads off one. */
    internal val jukeboxes: Jukeboxes? = null,
    /** The real block tags, which decide whether the held tool is the right one. */
    private val bossBars: BossBarRegistry? = null,
    /** Whether the whitelist is being enforced right now, which `/whitelist on` can change. */
    private val whitelistEnforced: () -> Boolean = { false },
    internal val enchantments: RealEnchantments? = null,
    private val dialogs: RealDialogs? = null,
    private val timelines: RealTimelines? = null,
    /** What each player has been taught, which `limited_crafting` consults. */
    internal val progress: PlayerProgress? = null,
    /** What turns a picked-up item into an advancement. Null where nothing loaded the definitions. */
    internal val advancementTriggers: AdvancementTriggers? = null,
    internal val blockTags: RealBlockTags? = null,
    /** The real item tags, sent to the client alongside the block ones. */
    internal val itemTags: RealItemTags? = null,
    /** The fluid tags, which are what the client checks before it believes it is under water. */
    private val fluidTags: RealFluidTags? = null,
    private val timelineTags: RealTimelineTags? = null,
    /** What decides how much of a hit armour and enchantments stop. Read by [CombatHandler.reduced]. */
    internal val damageTypeTags: RealDamageTypeTags? = null,
    private val entityTypeTags: RealEntityTypeTags? = null,
    /** Where plugin events go. Null when there are no plugins, which costs a null check. */
    internal val pluginEvents: PluginEventBus? = null,
    /**
     * Whichever plugin is drawing this player's sidebar, asked by uuid.
     *
     * Two lambdas rather than the registry itself: this class has no business knowing what a plugin
     * is, and the cheap question (is anybody holding it) has to stay separate from the expensive one
     * (what do they want drawn), which runs their code.
     */
    internal val pluginHoldsSidebar: ((String) -> Boolean)? = null,
    // Only the second one runs plugin code, and only when a redraw is due.
    private val pluginSidebar: ((String) -> PluginSurfaceContent?)? = null,
    internal val pluginHoldsBossBar: (() -> Boolean)? = null,
    private val pluginBossBar: ((String) -> PluginSurfaceContent?)? = null,
    /** Where a payload on a plugin's channel goes, and the cheap question of whether anybody wants it. */
    private val pluginChannel: ((String, String, ByteArray) -> Unit)? = null,
    private val pluginListensOn: ((String) -> Boolean)? = null,
    /** The NPCs a plugin put in the world, drawn by this connection like any other player. */
    internal val npcs: NpcRegistry? = null,
    /** Who is watching packets. Null when nothing is, which is the usual case and costs one check. */
    internal val pluginPackets: PluginPacketBus? = null,
    /** Which clients may in. The default is the native version and nothing else. */
    private val protocolGate: ProtocolGate =
        ProtocolGate(
            setOf(ProtocolVersions.NATIVE),
            UnknownProtocolPolicy.WARN,
            UnknownProtocolPolicy.REFUSE,
        ),
    /** What to call content this client's version never had. Null leaves the stand-in unnamed. */
    private val substitutionLabels: SubstitutionLabels? = null,
) {
    internal val logger = Logger("Session")

    /** What the console renders messages with; empty (so keys print as keys) when no language is loaded. */
    internal val consoleLang: LangTable
        get() = commands?.environment?.lang ?: LangTable.EMPTY
    private val sendMutex = NativeMutex()

    /**
     * What is waiting to go out. Open from the moment this connection has its own outbound thread:
     * from then on nobody else touches the socket, so the tick thread never parks on a slow client.
     */
    private val outbox = SessionOutbox()
    private val incoming = ByteBuffer()
    internal var state = ProtocolState.HANDSHAKE
    private var lastPacketId = -1

    /** Kept because the status response is built from it, several packets after it arrives. */
    private var handshake: HandshakePacket? = null

    /** What the door said about this client's version, decided the moment it named one. */
    private var verdict: ProtocolVerdict = ProtocolVerdict.Native

    /**
     * The chain of translators this client needs, or null when it speaks what the server speaks.
     *
     * Null is the case that must cost nothing: a native client goes through one null check per
     * packet and everything else works exactly as it did before any of this existed.
     */
    private var translation: ProtocolChain? = null

    /** How an enchanted stack resolves its ids, or null when no enchantment registry went out. */
    internal val enchantmentIds: ((Identifier) -> Int?)?
        get() = enchantments?.takeIf { it.size > 0 }?.let { registry -> { id -> registry.protocolIdOf(id) } }

    /** The same lookup backwards, for a stack coming *from* the client. */
    internal fun enchantmentNameOf(protocolId: Int): Identifier? = enchantments?.entries?.getOrNull(protocolId)?.first

    // Read by this connection's PlayerSession, so the same placeholders answer for a logged-in
    // player and for a pending ping.
    internal val handshakeProtocol: Int get() = handshake?.protocolVersion ?: 0
    internal val handshakeHost: String get() = handshake?.hostName.orEmpty()

    /** What the proxy in front of this server says about this connection, if there is one. */
    private val proxy = ProxyLogin(forwarding, logger)

    /** What reading the handshake host as forwarded data came to, or null when it was not read that way. */
    private var forwardedHandshake: ForwardingResult? = null

    /** The proxy's word for where the player is when one is forwarding, the socket otherwise. */
    internal val remoteAddress: String? get() = proxy.login?.address ?: connection.remoteAddress

    /** Where this connection came from, or null when nothing should be remembered about it. */
    internal val rememberAddress: String? get() = if (rememberIp) remoteAddress else null

    // Written once per session rather than on every flush: it only changes when someone logs in.
    internal var addressRecorded = false

    /**
     * This player's tab list, scoreboard and boss bar. Ticked from the outbound loop below, because
     * everything it does writes to this socket and the shared tick thread must never do that.
     */
    internal val playerUi =
        PlayerUi(
            ui,
            placeholders,
            ::sendPlayPacket,
            pluginHoldsSidebar = {
                val me = playerSession
                me != null && pluginHoldsSidebar?.invoke(me.uuid.toString()) == true
            },
            pluginSidebar = { playerSession?.let { pluginSidebar?.invoke(it.uuid.toString()) } },
            pluginHoldsBossBar = { pluginHoldsBossBar?.invoke() == true },
            pluginBossBar = { playerSession?.let { pluginBossBar?.invoke(it.uuid.toString()) } },
        )
    internal var uiTick = 0L

    // Persisted counters, carried across sessions. Play time is counted by the same 20 Hz loop that
    // ticks hunger, so it is ticks played rather than wall-clock time connected.
    internal var firstSeenEpochSeconds = 0L
    internal var playTimeTicks = 0L
    private var compressionEnabled = false

    /** How far the last step moved, squared. parity: `getKnownMovement`, which the sweep reads. */
    internal var lastStepSquared = 0.0

    /** Eating, drawing a bow and the item cooldown clock. See [ItemUseTimers]. */
    internal val itemUseTimers = ItemUseTimers(this)

    /** Who is watching this connection's packets, and what to do about it. See [PacketInterceptor]. */
    private val packetInterceptor = PacketInterceptor(this)

    /** Health, effects, absorption, experience, abilities and the HUD redraw. See [UiStateSync]. */
    internal val uiState = UiStateSync(this)

    private var pendingUsername: String? = null
    private var challenge: ByteArray? = null
    private var decryptCipher: Cfb8Cipher? = null
    private var encryptCipher: Cfb8Cipher? = null

    private var pendingProfile: GameProfile? = null

    /** The skin layer mask from client settings, which arrive in Configuration before the player exists. */
    private var pendingSkinParts: Byte = PlayerSession.ALL_SKIN_PARTS
    internal var playerSession: PlayerSession? = null

    /** Resolved in [enterPlay] from the player's stored world, falling back to the default. */
    internal var currentWorld: LoadedWorld? = null

    internal val world: WorldState get() = requireNotNull(currentWorld).state
    internal val generationQueue: ChunkGenerationQueue get() = requireNotNull(currentWorld).generationQueue
    internal val worldName: String get() = currentWorld?.name ?: ""

    /**
     * Container, menu, inventory and crafting state: every window this player can have open. See
     * [ContainerMenuController].
     */
    internal val menuController = ContainerMenuController(this)

    /**
     * The window this player has open. Their own inventory unless they opened a block's.
     *
     * A delegate onto [menuController]'s own copy: [VillagerTradeController] reaches into this
     * through [ClientSession] rather than the controller directly, same as it always has.
     */
    internal var openMenu: Menu
        get() = menuController.openMenu
        set(value) {
            menuController.openMenu = value
        }

    /** The last container id handed out, so a freshly opened window never reuses the previous one. */
    internal var nextContainerId: Int
        get() = menuController.nextContainerId
        set(value) {
            menuController.nextContainerId = value
        }

    /** The cooker whose window is open, if the open window belongs to one. */
    internal var openBlock: BlockPos?
        get() = menuController.openBlock
        set(value) {
            menuController.openBlock = value
        }

    /**
     * The other half of an open double chest, whose slots follow the first half's.
     *
     * parity: `ChestBlock.DOUBLE_INVENTORY_COMBINER` over a `CompoundContainer`: two block entities,
     * one window, and which of them leads is decided by the state rather than by which was clicked.
     */
    internal var openBlockSecond: BlockPos?
        get() = menuController.openBlockSecond
        set(value) {
            menuController.openBlockSecond = value
        }

    /**
     * Empties the crafting grid back into the player. Delegates onto [menuController]: reached
     * through [ClientSession] by [VillagerTradeController], same as [openMenu] above.
     */
    internal fun returnCraftingGrid(session: PlayerSession) = menuController.returnCraftingGrid(session)

    /** The whole inventory, and which slot is in hand. Delegates onto [menuController]. */
    internal fun sendInventory() = menuController.sendInventory()

    /** Re-sends the whole window. Delegates onto [menuController]. */
    internal fun sendInventoryAll() = menuController.sendInventoryAll()

    /** One slot changed. Delegates onto [menuController]. */
    internal fun sendInventorySlot(slot: Int) = menuController.sendInventorySlot(slot)

    /**
     * Opens a window a plugin drew. Delegates onto [menuController]: this is the plugin bridge's
     * entry point ([org.basaltmc.server.runtime.plugins.ServerObjectModel]), so the signature stays
     * exact.
     */
    internal fun openPluginMenu(
        title: String,
        rows: Int,
        items: Map<Int, org.basaltmc.server.plugins.PluginItem>,
        onClick: (Int) -> Unit,
    ) = menuController.openPluginMenu(title, rows, items, onClick)

    /** Shuts whatever window is open. Delegates onto [menuController]; also the plugin bridge's exit. */
    internal fun closeOpenMenu() = menuController.closeOpenMenu()

    /** Puts new contents in the open menu and redraws it. Delegates onto [menuController]. */
    internal fun refreshPluginMenu(items: Map<Int, org.basaltmc.server.plugins.PluginItem>) =
        menuController.refreshPluginMenu(items)

    /** The two bars of an open cooker. Delegates onto [menuController]; the furnace tick's entry point. */
    public fun sendFurnaceBars(): Unit = menuController.sendFurnaceBars()

    /**
     * Trading with a villager: offers sent, and the trade the player picks. See
     * [VillagerTradeController]. Reached by [CombatHandler] to open a villager's trades on interact.
     */
    internal val villagerTradeController = VillagerTradeController(this)

    /**
     * Melee combat, mounting, taming, fall damage, and the player-facing half of damage: [hurt],
     * [heal], [killPlayer] and their `fallDistance`. See [CombatHandler].
     */
    internal val combatHandler = CombatHandler(this)

    /**
     * Other players, NPCs, dropped items and summoned/generated entities this client has been told
     * to draw. See [EntityVisibilityTracker].
     */
    internal val entityTracker = EntityVisibilityTracker(this)

    /**
     * View radius, the chunk tracker, the queue of generated-but-unsent chunks and the client's own
     * reported chunks-per-tick budget. See [ChunkStreamer].
     */
    internal val chunkStreamer = ChunkStreamer(this)

    /**
     * Breaking, placing and using a block: digging, drops, signs, beds, jukeboxes and every other
     * block-shaped interaction. See [BlockInteractionHandler].
     */
    internal val blockInteraction = BlockInteractionHandler(this)

    /** One block change, to everyone standing in this world. Delegates onto [blockInteraction]. */
    internal fun broadcastBlockUpdate(
        pos: BlockPos,
        stateId: Int,
    ) = blockInteraction.broadcastBlockUpdate(pos, stateId)

    /** The other half of a block change: what it asks everyone nearby to hear or see. Delegates onto [blockInteraction]. */
    internal fun broadcastEffects(
        pos: BlockPos,
        stateId: Int,
        effects: List<BlockEffect>,
    ) = blockInteraction.broadcastEffects(pos, stateId, effects)

    /** Whoever is around a block being changed, told to relight and re-check it. Delegates onto [blockInteraction]. */
    internal fun notifyNeighbours(pos: BlockPos) = blockInteraction.notifyNeighbours(pos)

    /** Whether a state is air, cave air or void air: the three vanilla treats as nothing there. Delegates onto [blockInteraction]. */
    internal fun isAirLike(stateId: Int): Boolean = blockInteraction.isAirLike(stateId)

    /** parity: `createItemStackToDrop`, whose toss is never quite straight. Delegates onto [blockInteraction]. */
    internal fun tossLift(): Double = blockInteraction.tossLift()

    /** Out of bed: the flag on the block and the pose. Delegates onto [blockInteraction]. */
    internal fun wakeUp() = blockInteraction.wakeUp()

    /** Puts a new item entity into the world and tells everyone who can currently see it. Delegates onto [blockInteraction]. */
    internal fun spawnDroppedItem(
        item: Identifier,
        count: Int,
        x: Double,
        y: Double,
        z: Double,
        velocityX: Double = 0.0,
        velocityY: Double = 0.0,
        velocityZ: Double = 0.0,
        pickupDelay: Int = WorldState.DEFAULT_PICKUP_DELAY,
        components: NbtCompound = NbtCompound(),
    ) = blockInteraction.spawnDroppedItem(
        item,
        count,
        x,
        y,
        z,
        velocityX,
        velocityY,
        velocityZ,
        pickupDelay,
        components,
    )

    /**
     * The player tick, advancements, respawn, game mode, world changes, teleporting, leaving and
     * the player row's persistence. See [PlayerLifecycle].
     */
    internal val playerLifecycle = PlayerLifecycle(this)

    @OptIn(ExperimentalTime::class)
    private var lastKeepAlive: TimeSource.Monotonic.ValueTimeMark = TimeSource.Monotonic.markNow()
    private val streaming = AtomicInt(0)

    /** Whether this player's turn is already waiting on the owner, so it is not asked for twice. */
    private val tickTaskPending = AtomicInt(0)

    /** Set by a kick: this connection's own thread writes what is left and then closes the socket. */
    private val closeWhenDrained = AtomicInt(0)

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
            // From here on this thread owns the socket: everything else queues.
            outbox.open()
            NativeThread.start {
                while (streaming.value == 1) {
                    runCatching {
                        // The player's own turn, on the owner of the world they are in: hunger, the
                        // items they walk over and everything they see of the world are that
                        // owner's state, not this thread's. What they produce is a packet, and a
                        // packet is queued rather than written, so the owner never waits on a
                        // socket. A turn still running is not asked for a second one.
                        if (tickTaskPending.compareAndSet(0, 1)) {
                            onOwner {
                                try {
                                    // A tick's worth of chunk allowance, capped so a client that has
                                    // been idle cannot then be handed an enormous burst at once.
                                    // Reading and serializing a chunk is the world's business, so it
                                    // happens here; writing the bytes is still the socket's thread.
                                    chunkStreamer.sendBudget =
                                        minOf(
                                            chunkStreamer.sendBudget + chunkStreamer.desiredChunksPerTick,
                                            MAX_BURST_CHUNKS,
                                        )
                                    val allowed = chunkStreamer.sendBudget.toInt()
                                    if (allowed > 0) {
                                        timings.of("player.chunks").time {
                                            chunkStreamer.sendBudget -=
                                                chunkStreamer.flushReadyChunks(allowed).toFloat()
                                        }
                                    }
                                    timings.of("player.tick").time { playerLifecycle.tickPlayer() }
                                    timings.of("player.players").time { entityTracker.syncPlayers() }
                                    timings.of("player.npcs").time { entityTracker.syncNpcs() }
                                    timings.of("player.posture").time { uiState.syncPosture() }
                                    timings.of("player.items").time { entityTracker.syncItemEntities() }
                                    timings.of("player.entities").time { entityTracker.syncSummonedEntities() }
                                    timings.of("player.generated_entities").time {
                                        entityTracker.syncGeneratedEntities()
                                    }
                                    timings.of("player.ui").time { uiState.tickUi() }
                                    timings.of("player.container").time {
                                        if (openBlock != null && menuController.openBlockSize == FURNACE_SLOTS) {
                                            menuController.syncOpenContainer()
                                        }
                                    }
                                } finally {
                                    tickTaskPending.value = 0
                                }
                            }
                        }
                        sendKeepAliveIfDue()
                        timings.of("player.flush").time { flushOutbox() }
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
                // Whatever answering those frames queued goes out now: this is a thread of this
                // connection, so it may write, and waiting for the outbound loop would add its
                // whole interval to every reply.
                if (outbox.isOpen) flushOutbox()
            }
        } catch (e: SocketException) {
            // A connection dropping is normal, not a failure worth surfacing: a server-list ping
            // resets the socket the instant it has its answer.
            logger.debug { "Connection closed: ${e.message}" }
        } catch (e: Exception) {
            // One malformed packet costs one connection, not the thread reading it. Logged loudly
            // on purpose: reaching here is a decoder bug or a hostile client, never routine.
            logger.error(e) {
                "Dropping ${playerSession?.name ?: "connection"}: " +
                    "failed to handle packet 0x${lastPacketId.toString(16)} in $state"
            }
        } finally {
            streaming.value = 0
            playerLifecycle.leavePlayIfPresent()
            // Whatever the leaving player's own thread had queued for others is already theirs; what
            // was queued for this socket has nowhere to go.
            outbox.close()
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
        val chain = translation
        if (chain == null) {
            timings.of("network.packet").time { handleFrameBody(packetId, body) }
            return
        }
        // An older client's packet is brought up to what this server reads before anything here
        // looks at it, so nothing past this line knows there is more than one version.
        chain.serverbound(state, packetId, body.readBytes(body.readableBytes)) { id, bytes ->
            lastPacketId = id
            timings.of("network.packet").time { handleFrameBody(id, ByteBuffer.wrap(bytes)) }
        }
    }

    private fun handleFrameBody(
        packetId: Int,
        body: ByteBuffer,
    ) {
        if (!packetInterceptor.interceptIncoming(packetId, body)) return
        when (state) {
            ProtocolState.HANDSHAKE -> handleHandshake(packetId, body)
            ProtocolState.STATUS -> handleStatus(packetId, body)
            ProtocolState.LOGIN -> handleLogin(packetId, body)
            ProtocolState.CONFIGURATION -> handleConfiguration(packetId, body)
            // parity: `PacketUtils.ensureRunningOnSameThread`, which is what vanilla does with every
            // play packet: the network thread reads it and the server thread runs it. Here that is
            // the world's owner, so everything a packet touches is touched by whoever owns it.
            ProtocolState.PLAY ->
                onOwner {
                    try {
                        handlePlay(packetId, body)
                    } catch (e: Exception) {
                        // A decoder that runs off the end of a frame costs this connection and
                        // nothing else, exactly as it did when it ran on the read thread: the world
                        // it was running on carries every other player.
                        logger.debug { "Dropping a connection whose packet 0x${packetId.toString(16)} failed: $e" }
                        connection.close()
                    }
                }
        }
    }

    /**
     * Runs [work] on the owner of the world this player is in, or here when there is none yet.
     *
     * The fallback is not a loophole: before the join finishes there is no world to own anything,
     * and after it there always is.
     */
    private inline fun onOwner(crossinline work: () -> Unit) {
        val world = currentWorld
        if (world == null) {
            work()
            return
        }
        (regionOwner ?: world.owner).submit { work() }
    }

    /**
     * Which owner this player's packets go to, changed only between turns.
     *
     * A player who walks across a border does not change owner mid-tick: two regions can be
     * ticking at once, and handing the session over while one of them is inside it would mean two
     * threads in the same player. [handOverToRegionOf] is called from the global phase, where no
     * region is running, which is the safe point for it.
     */
    private var regionOwner: org.basaltmc.server.runtime.sched.Owner? = null

    /** parity of intent with Folia handing a player to the region they walked into. */
    internal fun handOverToRegionOf(world: LoadedWorld) {
        val session = playerSession ?: return
        if (session.worldName != world.name) return
        regionOwner = world.ownerOf(session.x.toInt(), session.z.toInt())
    }

    private fun handleHandshake(
        packetId: Int,
        body: ByteBuffer,
    ) {
        if (packetId != HandshakePacket.ID) return
        val packet =
            if (proxy.readsHandshake) {
                HandshakePacket.read(body, HandshakePacket.FORWARDED_HOST_LENGTH)
            } else {
                HandshakePacket.read(body)
            }
        handshake = packet
        verdict = protocolGate.verdictFor(packet.protocolVersion)
        translation = JavaTranslators.chainFor(packet.protocolVersion, substitutionLabels)
        state =
            when (packet.intent) {
                ClientIntent.STATUS -> ProtocolState.STATUS
                ClientIntent.LOGIN -> ProtocolState.LOGIN
                ClientIntent.TRANSFER -> return // not supported yet; drop the connection
            }
        // Read now, acted on at login: a refusal there is still a disconnect screen rather than a
        // socket that closes for no stated reason. A ping never carries forwarded data.
        if (proxy.readsHandshake && packet.intent == ClientIntent.LOGIN) {
            forwardedHandshake = proxy.readHandshake(packet.hostName)
            (forwardedHandshake as? ForwardingResult.Accepted)?.let {
                // The host with the forwarded data stripped back off it, which is what the player typed.
                handshake = packet.copy(hostName = it.login.host)
            }
        }
    }

    private fun handleStatus(
        packetId: Int,
        body: ByteBuffer,
    ) {
        when (packetId) {
            StatusRequestPacket.ID -> {
                val protocol = handshake?.protocolVersion ?: 0
                val status = statusProvider(protocol, handshake?.hostName.orEmpty(), connection.remoteAddress)
                sendUncompressed(StatusResponsePacket.ID) { StatusResponsePacket(pingedAs(status, protocol)).write(it) }
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
            LoginPluginResponsePacket.ID -> handleForwardingAnswer(body)
            LoginAcknowledgedPacket.ID -> enterConfiguration()
        }
    }

    /**
     * The version reported to whoever is pinging: theirs when they may in, ours when they may not.
     *
     * A client compares this number with its own and paints the entry red when they differ, so
     * echoing an accepted client's own number is what stops a server that would let them play from
     * looking broken in their list. Refusing one keeps our number, which is the honest hint.
     */
    private fun pingedAs(
        status: org.basaltmc.wire.protocol.status.ServerStatusPayload,
        protocol: Int,
    ): org.basaltmc.wire.protocol.status.ServerStatusPayload {
        val version = status.version ?: return status
        if (protocol == version.protocol || verdict is ProtocolVerdict.Refused) return status
        return status.copy(version = version.copy(protocol = protocol))
    }

    private fun handleLoginStart(body: ByteBuffer) {
        val loginStart = LoginStartPacket.read(body)
        val refusal = verdict as? ProtocolVerdict.Refused
        if (refusal != null) {
            refuseLogin(loginStart.name, refusal)
            return
        }
        (verdict as? ProtocolVerdict.Unknown)?.let { unknown ->
            if (unknown.warn) {
                logger.warn {
                    "${loginStart.name} is on ${ProtocolVersions.describe(unknown.protocol)}, which this build " +
                        "does not know. Letting them in untranslated; anything may happen."
                }
            }
        }
        (verdict as? ProtocolVerdict.Translated)?.let { translated ->
            logger.info { "${loginStart.name} is on ${translated.version}, translated to 26.2." }
        }
        pendingUsername = loginStart.name
        // A proxy has already authenticated this player, so the session exchange never happens:
        // what it says about them is the whole of their identity here.
        if (proxy.enabled) {
            startForwardedLogin(loginStart)
            return
        }
        if (pluginRefusesLogin(loginStart.name, loginStart.profileId.toString())) return

        val online = onlineMode
        if (online == null) {
            finishLogin(OfflineUuid.forUsername(loginStart.name), loginStart.name, emptyList())
            return
        }

        val nonce = Random.nextBytes(4)
        challenge = nonce
        sendUncompressed(HelloPacket.ID) {
            HelloPacket("", online.keyPair.publicKeyDer, nonce, shouldAuthenticate = true).write(it)
        }
    }

    /**
     * The one gate before a player exists at all: a plugin that says no here means they never join,
     * and what it says is what they read on the disconnect screen.
     *
     * Returns whether it said no, in which case this connection is already closed.
     */
    private fun pluginRefusesLogin(
        name: String,
        profileId: String,
    ): Boolean {
        val login =
            pluginEvents.emit(PluginEventTypes.PLAYER_LOGIN) { event ->
                event.set(0, name)
                event.set(1, profileId)
                event.set(2, remoteAddress)
            }
        if (login?.cancelled != true) return false
        val reason = login.cancelMessage ?: "<red>You may not join this server"
        disconnectFromLogin(reason)
        return true
    }

    /**
     * Either finishes the login with what a BungeeCord-family proxy put on the handshake, or asks
     * a Velocity one for it and waits.
     */
    private fun startForwardedLogin(loginStart: LoginStartPacket) {
        if (proxy.asksTheProxy) {
            val transactionId = proxy.beginQuery()
            sendUncompressed(LoginPluginRequestPacket.ID) {
                LoginPluginRequestPacket(transactionId, proxy.channel, proxy.query()).write(it)
            }
            return
        }
        val result =
            forwardedHandshake
                ?: ForwardingResult.Rejected("the handshake was read before forwarding was configured")
        proxy.accept(result, loginStart.name, connection.remoteAddress)?.let { refusal ->
            disconnectFromLogin(refusal)
            return
        }
        val forwarded = proxy.login ?: return
        if (pluginRefusesLogin(loginStart.name, forwarded.profileId.toString())) return
        finishLogin(forwarded.profileId, loginStart.name, forwarded.properties)
    }

    /** Velocity's half: the proxy answers the query with the signed profile, or nothing does. */
    private fun handleForwardingAnswer(body: ByteBuffer) {
        val answer = LoginPluginResponsePacket.read(body)
        val name = pendingUsername
        if (!proxy.asksTheProxy || name == null) {
            connection.close()
            return
        }
        val result = proxy.readAnswer(answer.transactionId, answer.payload)
        proxy.accept(result, name, connection.remoteAddress)?.let { refusal ->
            disconnectFromLogin(refusal)
            return
        }
        val forwarded = proxy.login ?: return
        // The proxy's name for them, not the one they claimed in the login packet.
        val forwardedName = forwarded.name ?: name
        if (pluginRefusesLogin(forwardedName, forwarded.profileId.toString())) return
        finishLogin(forwarded.profileId, forwardedName, forwarded.properties)
    }

    /** Turns a client away while still in login, where a written reason still reaches the screen. */
    private fun disconnectFromLogin(miniMessage: String) {
        sendUncompressed(LoginDisconnectPacket.ID) {
            LoginDisconnectPacket(MiniMessage.parse(miniMessage)).write(it)
        }
        connection.close()
    }

    /** Turns a client away at the door, telling them what is wrong in words they can act on. */
    private fun refuseLogin(
        name: String,
        refusal: ProtocolVerdict.Refused,
    ) {
        val protocol = refusal.protocol
        val message =
            when (refusal.reason) {
                ProtocolVerdict.Refused.Reason.TOO_OLD ->
                    "<red>Your version is too old for this server.</red>\n<gray>It runs Minecraft " +
                        "${BuildInfo.MINECRAFT_VERSION}."
                ProtocolVerdict.Refused.Reason.TOO_NEW, ProtocolVerdict.Refused.Reason.NOT_TRANSLATED ->
                    "<red>This server does not accept your version.</red>\n<gray>It runs Minecraft " +
                        "${BuildInfo.MINECRAFT_VERSION}."
                ProtocolVerdict.Refused.Reason.SNAPSHOT ->
                    "<red>This server does not accept snapshots.</red>\n<gray>It runs Minecraft " +
                        "${BuildInfo.MINECRAFT_VERSION}."
            }
        logger.info {
            "Refused $name on ${ProtocolVersions.describe(protocol)}: ${refusal.reason.name.lowercase()}."
        }
        sendUncompressed(LoginDisconnectPacket.ID) { LoginDisconnectPacket(MiniMessage.parse(message)).write(it) }
        connection.close()
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
        // Before 1.20.2 there is no configuration phase and nothing to acknowledge: the client is
        // already in play. The server still walks its own configuration, whose packets the
        // translation turns into the one thing that client does read, but it walks it by itself
        // rather than waiting to be told to.
        if (handshakeProtocol < ProtocolVersions.V1_20_2) enterConfiguration()
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
        remoteAddress?.let { address ->
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
        // Claims the client's own vanilla data pack rather than an empty list. A client older
        // than 1.20.5 has no such packet and would never answer, so it is not asked.
        if (handshakeProtocol < ProtocolVersions.V1_20_5) {
            sendRegistriesThenFinish()
            return
        }
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
        for (packet in VanillaConfigurationRegistries.packets()) {
            sendCompressed(RegistryDataPacket.ID) { packet.write(it) }
        }
        // The rest of `RegistryDataLoader.SYNCHRONIZED_REGISTRIES`, sent empty: a registry that was
        // never sent does not exist client-side, and tags scoped to it stop resolving. Enchantments
        // are all or nothing, because a partial registry disconnects at login (see RealEnchantments).
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
        // Every tag name the jar ships goes out (see VanillaTagData): the client looks tags up by
        // key whether or not their registry is loaded. Members matter separately, because it runs
        // its own tool rules against them and an empty `mineable/pickaxe` means bare-hand speed.
        sendCompressed(TagsPacket.ID) {
            TagsPacket(
                VanillaTagData.tagsByRegistry.map { (registryId, tagNames) ->
                    TagGroup(registryId, tagNames.map { TagEntry(it, tagMembers(registryId, it)) })
                },
            ).write(it)
        }
        sendCompressed(FinishConfigurationPacket.ID) { }
        // Same again at the other end of it: a client with no configuration phase will not answer
        // that this one is over.
        if (handshakeProtocol < ProtocolVersions.V1_20_2) enterPlay()
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
        uiState.sendAbilities()
        // parity: `PlayerList.placeNewPlayer` sends the recipes right here, between the abilities
        // and the permission level. The stonecutter draws its own buttons from them, so without it
        // that screen opens empty however many recipes the server knows.
        sendStonecutterRecipes()
        uiState.sendCommandTree()

        // Without these, a real client's "loading terrain" screen never dismisses: it has no
        // center to count chunks around and no radius to know when it has enough (confirmed live).
        // Seeds the tracker with exactly the square about to be sent below, so later crossings
        // compute deltas relative to it instead of re-sending everything as "newly loaded" on the
        // first move.
        val initialChunks = chunkStreamer.initialJoin(session.x, session.z, chunkRadius)

        // parity: `PlayerList.placeNewPlayer`/`sendLevelInfo`, and the order matters: teleport and
        // player info before level data, and `level_chunks_load_start` right before the chunks.
        // Chunks first leaves a real client on "loading terrain" for ever.
        uiState.sendTime(world)
        if (world.weather.raining) uiState.sendWeather(world.weather)
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
        // Where this player actually is, which for somebody coming back is not the world spawn.
        logger.info {
            "${session.name} logged in with entity id $entityId at " +
                "(${oneDecimal(session.x)}, ${oneDecimal(session.y)}, ${oneDecimal(session.z)})"
        }
        session.profileProperties = pendingProfile?.properties.orEmpty()
        session.skinParts = pendingSkinParts
        logger.info { "${session.name} joined the game" }
        pluginEvents.emit(PluginEventTypes.PLAYER_JOIN) { event ->
            event.set(0, session.name)
            event.set(1, session.uuid.toString())
            event.set(2, currentWorld?.name)
        }

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
        uiState.syncHealth()
        uiState.syncExperience()
        // parity: `PlayerList.placeNewPlayer` sends the inventory here, after the level info and the
        // player list, not among the chunk-radius packets above. Order matters on join: an earlier
        // version of this code put a packet in the middle of that sequence and left real clients
        // stuck on "loading terrain".
        sendInventory()

        // Awaits and sends the ground the player lands on, then queues the rest of the view
        // distance from [initialChunks] for the outbound loop. See [ChunkStreamer.sendInitialChunks].
        chunkStreamer.sendInitialChunks(initialChunks)

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
        uiState.tickUi()

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
    internal fun spawnInfo(gamemode: Gamemode): SpawnInfo =
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
     *
     * A thin delegate: called on other players' connections directly (`/place`, `/fillbiome` and
     * similar world-editing commands), so the signature stays here even though the work is
     * [ChunkStreamer]'s.
     */
    internal fun resendChunk(
        chunkX: Int,
        chunkZ: Int,
    ) = chunkStreamer.resendChunk(chunkX, chunkZ)

    /**
     * New biomes for columns this client already has, without resending them. A thin delegate, for
     * the same reason as [resendChunk]. See [ChunkStreamer.sendBiomes].
     */
    internal fun sendBiomes(chunks: List<Pair<Int, Int>>) = chunkStreamer.sendBiomes(chunks)

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
                when (ClientCommandPacket.read(body).action) {
                    ClientCommandPacket.Action.PERFORM_RESPAWN -> playerLifecycle.respawn()
                    ClientCommandPacket.Action.REQUEST_STATS -> uiState.sendStats()
                    ClientCommandPacket.Action.REQUEST_GAMERULE_VALUES -> Unit
                }
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
                    PlayerCommandPacket.Action.START_SPRINTING -> {
                        playerEvent(PluginEventTypes.PLAYER_SPRINT) { event -> event.set(3, true) }
                        playerSession?.sprinting = true
                    }

                    PlayerCommandPacket.Action.STOP_SPRINTING -> {
                        playerEvent(PluginEventTypes.PLAYER_SPRINT) { event -> event.set(3, false) }
                        playerSession?.sprinting = false
                    }
                    else -> Unit
                }
            }
            ServerboundPlayerActionPacket.ID -> {
                val action = ServerboundPlayerActionPacket.read(body)
                blockInteraction.handlePlayerAction(action)
            }
            ServerboundSetCarriedItemPacket.ID -> {
                val slot = ServerboundSetCarriedItemPacket.read(body).slot
                if (slot in 0 until ServerboundSetCarriedItemPacket.HOTBAR_SIZE) {
                    val was = playerSession?.inventory?.heldSlot
                    playerEvent(PluginEventTypes.PLAYER_ITEM_HELD) { event ->
                        event.set(3, slot)
                        event.set(4, was)
                    }
                    playerSession?.inventory?.heldSlot = slot
                    uiState.syncHeldAttributes()
                }
            }
            ServerboundSetCreativeModeSlotPacket.ID ->
                menuController.handleSetCreativeModeSlot(ServerboundSetCreativeModeSlotPacket.read(body))
            ServerboundContainerClickPacket.ID ->
                menuController.handleContainerClick(ServerboundContainerClickPacket.read(body))
            CustomPayloadPacket.SERVERBOUND_ID -> handleCustomPayload(body)
            ContainerClosePacket.SERVERBOUND_ID -> {
                ContainerClosePacket.read(body)
                playerEvent(PluginEventTypes.INVENTORY_CLOSE) { event ->
                    event.set(3, if (openMenu.containerId == 0) "player" else "container")
                }
                playerSession?.let { menuController.closeOpenMenu(it) }
            }
            InteractPacket.ID -> combatHandler.handleInteract(InteractPacket.read(body))
            AttackPacket.ID -> combatHandler.handleAttack(AttackPacket.read(body))
            SelectTradePacket.ID -> villagerTradeController.selectTrade(SelectTradePacket.read(body).index)
            ContainerButtonClickPacket.ID ->
                menuController.clickContainerButton(ContainerButtonClickPacket.read(body).buttonId)
            SignUpdatePacket.ID -> blockInteraction.writeSign(SignUpdatePacket.read(body))
            SWING_PACKET_ID -> blockInteraction.handleSwing(body.readVarInt())
            ServerboundUseItemOnPacket.ID -> blockInteraction.handleUseItemOn(ServerboundUseItemOnPacket.read(body))
            ServerboundUseItemPacket.ID -> blockInteraction.handleUseItem(ServerboundUseItemPacket.read(body))
            ServerboundPickItemFromEntityPacket.ID ->
                menuController.handlePickItemFromEntity(ServerboundPickItemFromEntityPacket.read(body))
            ServerboundPickItemFromBlockPacket.ID ->
                menuController.handlePickItemFromBlock(
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
                chunkStreamer.desiredChunksPerTick =
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
                val session = playerSession
                if (session != null) {
                    val nowSneaking = (inputs and SNEAK_INPUT_FLAG) != 0
                    if (nowSneaking != session.sneaking) {
                        playerEvent(PluginEventTypes.PLAYER_SNEAK) { event -> event.set(3, nowSneaking) }
                    }
                    session.sneaking = (inputs and SNEAK_INPUT_FLAG) != 0
                    session.inputForward = (inputs and FORWARD_INPUT_FLAG) != 0
                    session.inputBack = (inputs and BACK_INPUT_FLAG) != 0
                    session.inputLeft = (inputs and LEFT_INPUT_FLAG) != 0
                    session.inputRight = (inputs and RIGHT_INPUT_FLAG) != 0
                    // parity: `Player.wantsToStopRiding`, which is the crouch key and nothing else.
                    if (session.sneaking && session.vehicleId != null) dismount(session)
                }
            }
        }
    }

    /** parity: `BellBlock.isProperHit`, which will not take a click on the very top of the block. */
    internal val bellHitTop = 0.8124f

    /** parity: `MinecartItem.useOn`, the sixteenth of a block a cart sits above its rail. */
    internal val minecartRestHeight = 0.0625

    internal fun frameDirectionData(facing: String): Int =
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
     * ([PlayerLifecycle.tickPlayer], see [startOutboundLoop]'s own KDoc for why): the shared tick
     * loop touches no sockets, and picking something up always ends in one (an inventory update,
     * at least).
     */
    internal fun tryPickupNearbyItems() {
        val session = playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        for (candidate in world.nearbyItems(session.x, session.y, session.z, PICKUP_RADIUS)) {
            // parity: a player's own toss cannot be picked straight back up (`ItemEntity.pickupDelay`).
            if (!candidate.canBePickedUp) continue
            val removed = world.removeItem(candidate.entityId) ?: continue
            val picking =
                playerEvent(PluginEventTypes.PLAYER_ITEM_PICKUP) { event ->
                    event.set(3, removed.item.toString())
                    event.set(4, removed.count)
                    event.set(5, removed.entityId)
                }
            if (picking?.cancelled == true) {
                // Put it back exactly as it was: the client has not been told anything yet.
                world.spawnItem(removed)
                continue
            }
            val leftover =
                session.inventory.add(
                    StoredItemStack(removed.item.toString(), removed.count, removed.components),
                )
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

    /** A sound heard where it happened, by everyone in the world. */
    internal fun broadcastSound(
        name: String,
        x: Double,
        y: Double,
        z: Double,
        source: SoundSource = SoundSource.NEUTRAL,
        volume: Float = 1f,
        pitch: Float = 1f,
    ) {
        val packet = SoundPacket(name, source, x, y, z, volume, pitch, Random.nextLong())
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(SoundPacket.ID) { packet.write(it) }
        }
    }

    /** parity: `Entity.stopRiding`: off the cart, standing where it left them. Delegates onto [combatHandler]. */
    internal fun dismount(session: PlayerSession) = combatHandler.dismount(session)

    /**
     * parity: `ServerGamePacketListenerImpl.handleInteract`, which measures three blocks of reach
     * plus three of slack from the eyes to the target's box before it lets anything through.
     */
    internal fun withinAttackReach(
        session: PlayerSession,
        target: ServerEntity,
    ): Boolean {
        val half = target.width / 2.0
        val dx = maxOf(abs(target.x - session.x) - half, 0.0)
        val dz = maxOf(abs(target.z - session.z) - half, 0.0)
        val eyeY = session.y + THROW_EYE_HEIGHT
        val dy = maxOf(maxOf(target.y - eyeY, eyeY - (target.y + target.height)), 0.0)
        return dx * dx + dy * dy + dz * dz < ATTACK_REACH * ATTACK_REACH
    }

    /**
     * parity: `ClientboundUpdateRecipesPacket`, of which this sends the stonecutter half.
     *
     * The recipe book's own item sets go out empty: they only drive the highlighting of what can be
     * crafted, and drawing a stonecutter's buttons does not depend on them.
     */
    private fun sendStonecutterRecipes() {
        val recipes = recipes ?: return
        val options =
            recipes.stonecutting.mapNotNull { recipe ->
                val result = Items.REGISTRY.byIdentifier(recipe.result.item) ?: return@mapNotNull null
                val inputs = recipe.ingredient.items.mapNotNull { Items.REGISTRY.byIdentifier(it)?.protocolId }
                if (inputs.isEmpty()) return@mapNotNull null
                StonecutterOption(inputs, result.protocolId, recipe.result.count)
            }
        logger.debug { "Sending ${options.size} stonecutter recipe(s)." }
        sendCompressed(UpdateRecipesPacket.ID) { UpdateRecipesPacket(options).write(it) }
    }

    /** parity: `RecipeType.CAMPFIRE_COOKING`, the only cooker that is not a block with a window. */
    internal val campfireCooker = org.basaltmc.server.world.crafting.Recipe.Cooker.CAMPFIRE

    /** A system message to every player on the server, whatever world they are in. */
    internal fun broadcastToEveryone(message: Component) {
        val broadcast =
            pluginEvents.emit(PluginEventTypes.SERVER_BROADCAST) { event ->
                event.set(0, message.resolvePlainText(consoleLang))
            }
        if (broadcast?.cancelled == true) return
        @Suppress("NAME_SHADOWING")
        val message = (broadcast?.get("message") as? String)?.let { MiniMessage.parse(it) } ?: message
        for (player in playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(SystemChatPacket.ID) {
                SystemChatPacket(message, isActionBar = false).write(it)
            }
        }
    }

    /** A coordinate as the log wants it: one decimal, which is as much as anybody reads off it. */
    private fun oneDecimal(value: Double): String {
        // The sign is kept apart from the digits: dividing a negative tenth by ten is zero, so
        // anything between -1 and 0 came out looking positive.
        val tenths = (abs(value) * 10.0).roundToLong()
        val sign = if (value < 0.0 && tenths != 0L) "-" else ""
        return "$sign${tenths / 10}.${tenths % 10}"
    }

    /** Everyone who can actually see this player: the tab list is server-wide, entities are not. */
    internal fun playersInSameWorld(): List<PlayerSession> {
        val name = playerSession?.worldName ?: return emptyList()
        return playerRegistry?.snapshot()?.filter { it.worldName == name }.orEmpty()
    }

    internal fun updatePosition(
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        pitch: Float,
        onGround: Boolean,
    ) {
        val session = playerSession ?: return
        // parity: `ServerGamePacketListenerImpl.handleMovePlayer`, which takes only the rotation
        // from somebody who is riding: where they are is the vehicle's business.
        if (session.vehicleId != null) {
            session.yaw = yaw
            session.pitch = pitch
            return
        }
        // Only on a real move: a client sends its position every tick whether it moved or not, and
        // waking a handler for standing still is what makes this event expensive in Bukkit.
        if (x != session.x || y != session.y || z != session.z) {
            val moved =
                playerEvent(PluginEventTypes.PLAYER_MOVE) { event ->
                    event.set(3, session.x)
                    event.set(4, session.y)
                    event.set(5, session.z)
                    event.set(6, x)
                    event.set(7, y)
                    event.set(8, z)
                }
            if (moved?.cancelled == true) {
                moved.cancelMessage?.let { sendPluginNotice(it) }
                // Put them back where they were, which is what the client is told rather than
                // asked: it has already drawn the step it took.
                sendCompressed(PlayerPositionPacket.ID) {
                    PlayerPositionPacket(
                        0,
                        session.x,
                        session.y,
                        session.z,
                        0.0,
                        0.0,
                        0.0,
                        yaw,
                        pitch,
                        relativeFlags = 0,
                    ).write(it)
                }
                return
            }
        }
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
        combatHandler.trackFall(session, y, onGround, feet)
        // Horizontal only, and only on the ground, which is how vanilla tells walking apart from
        // falling and from being moved by something else.
        if (onGround) {
            val dx = x - session.x
            val dz = z - session.z
            session.stats.addWalked(kotlin.math.sqrt(dx * dx + dz * dz))
            lastStepSquared = dx * dx + dz * dz
        }
        session.x = x
        session.y = y
        session.z = z
        session.yaw = yaw
        session.pitch = pitch
        session.onGround = onGround
        chunkStreamer.streamChunksAround(x, z)
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
     * Fires a player event, filling in the three fields every one of them starts with.
     *
     * Inline, so a type nobody listens to costs the same null check as anywhere else: the values
     * after these three are only worked out inside [fill], which never runs without a listener.
     */
    internal inline fun playerEvent(
        type: org.basaltmc.server.plugins.PluginEventType,
        fill: (org.basaltmc.server.plugins.PluginEvent) -> Unit,
    ): org.basaltmc.server.plugins.PluginEvent? {
        val session = playerSession ?: return null
        return pluginEvents.emit(type) { event ->
            event.set(0, session.name)
            event.set(1, session.uuid.toString())
            event.set(2, currentWorld?.name)
            fill(event)
        }
    }

    /** Tells this player why a plugin stopped what they were doing. MiniMessage, like all plugin text. */
    internal fun sendPluginNotice(message: String) {
        sendPluginText(MiniMessage.parse(message), actionBar = false)
    }

    /** What a plugin writes to this player, in chat or on the action bar. */
    internal fun sendPluginText(
        text: Component,
        actionBar: Boolean,
    ) {
        sendPlayPacket(SystemChatPacket.ID) { SystemChatPacket(text, isActionBar = actionBar).write(it) }
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
        val event =
            playerEvent(PluginEventTypes.PLAYER_CHAT) { event -> event.set(3, message) }
        if (event?.cancelled == true) {
            event.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
        val spoken = (event?.get("message") as? String) ?: message
        val text = Component.text("<${session.name}> $spoken", TextColor.WHITE)
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
        val event =
            playerEvent(PluginEventTypes.PLAYER_COMMAND) { event -> event.set(3, command) }
        if (event?.cancelled == true) {
            event.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
        @Suppress("NAME_SHADOWING")
        val command = (event?.get("command") as? String) ?: command
        logger.info { "${session.name} issued server command: /$command" }
        dispatcher.execute(PlayerCommandSource(session, dispatcher.environment.groups), command)
    }

    // --- Player state ---

    /** parity: `LivingEntity.addEffect`, for the effects a mob puts on somebody rather than a drink. */
    internal fun addEffect(
        effect: Identifier,
        durationTicks: Int,
        amplifier: Int,
    ) = uiState.addEffect(effect, durationTicks, amplifier)

    /** Sends one effect, or its removal when it has run out. parity: the two mob effect packets. */
    internal fun sendEffect(effect: ActiveEffect) = uiState.sendEffect(effect)

    internal fun sendEffectRemoved(effect: Identifier) = uiState.sendEffectRemoved(effect)

    internal fun syncHeldAttributes() = uiState.syncHeldAttributes()

    /**
     * parity: `LivingEntity.hurt`, minus absorption, which no effect here grants yet.
     *
     * A thin delegate: [hurt]'s external call surface is too wide to move with the rest of combat
     * (fire, lava, drowning, starvation, `/kill`... call it directly), so the signature stays here
     * and the work is [CombatHandler]'s. See [CombatHandler]'s class doc.
     */
    internal fun hurt(
        amount: Float,
        deathMessageId: String,
        damageType: Identifier? = null,
        source: DamageSource? = null,
    ) = combatHandler.hurt(amount, deathMessageId, damageType, source)

    /** parity: `LivingEntity.heal`. A thin delegate, for the same reason as [hurt]. */
    internal fun heal(amount: Float) = combatHandler.heal(amount)

    /** parity: `Entity.kill`. A thin delegate, for the same reason as [hurt]. */
    internal fun killPlayer(deathMessageId: String) = combatHandler.killPlayer(deathMessageId)

    /**
     * parity: `ServerPlayer.setGameMode`. A thin delegate: `/gamemode`, the plugin bridge and
     * elsewhere call this by name, so the signature stays here and the work is
     * [PlayerLifecycle]'s.
     */
    internal fun setGamemode(mode: Gamemode) = playerLifecycle.setGamemode(mode)

    /**
     * Moves this player into another world. A thin delegate, for the same reason as [setGamemode]:
     * portals, `/tp`, `/world` and the plugin bridge all call it directly.
     */
    internal fun switchWorld(
        target: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean = playerLifecycle.switchWorld(target, x, y, z)

    /**
     * A thin delegate, for the same reason as [setGamemode]: commands, portals and the plugin
     * bridge all call it directly.
     */
    internal fun teleportTo(
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        pitch: Float,
        /** What asked for it, for a plugin that treats a command differently from a portal. */
        cause: String = "command",
    ) = playerLifecycle.teleportTo(x, y, z, yaw, pitch, cause)

    /** The air left and whether this player burns, to them and to everyone who can see them. */
    internal fun syncSubmersion() = uiState.syncSubmersion()

    internal fun syncHealth() = uiState.syncHealth()

    internal fun sendActionBar(message: Component) {
        sendPlayPacket(SystemChatPacket.ID) { SystemChatPacket(message, isActionBar = true).write(it) }
    }

    /** parity: `ServerGamePacketListenerImpl`'s `SWAP_ITEM_WITH_OFFHAND`: the F key. */
    internal fun swapWithOffhand() {
        val session = playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        itemUseTimers.stopEating()
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val swap =
            playerEvent(PluginEventTypes.PLAYER_SWAP_HANDS) { event ->
                event.set(3, held?.id)
                event.set(4, session.inventory[PlayerInventory.OFFHAND_SLOT]?.id)
            }
        if (swap?.cancelled == true) {
            swap.cancelMessage?.let { sendPluginNotice(it) }
            sendInventorySlot(heldSlot)
            sendInventorySlot(PlayerInventory.OFFHAND_SLOT)
            return
        }
        session.inventory[heldSlot] = session.inventory[PlayerInventory.OFFHAND_SLOT]
        session.inventory[PlayerInventory.OFFHAND_SLOT] = held
        sendInventorySlot(heldSlot)
        sendInventorySlot(PlayerInventory.OFFHAND_SLOT)
        uiState.syncHeldAttributes()
    }

    /** parity: `Consumable.onConsume`, which is what makes a golden apple worth eating. */
    internal fun applyConsumeEffects(
        session: PlayerSession,
        effects: List<ConsumeEffect>,
    ) = uiState.applyConsumeEffects(session, effects)

    /**
     * The statistics screen's contents.
     *
     * parity: `ServerStatsCounter.sendStats`, which answers the screen's own request. Only the
     * counters this build keeps go out: the screen shows a zero for everything else, which is what
     * vanilla shows for a counter nobody has moved either.
     */
    internal fun sendStats() = uiState.sendStats()

    /** This world's clocks, as the join sequence and every `/time` send them. */
    internal fun sendTime(world: LoadedWorld) = uiState.sendTime(world)

    /**
     * parity: `ServerPlayer.setPlayerWeather`/`PlayerList.sendLevelInfo`, which announces rain as a
     * start/stop event *and* the two levels: the events swap the client's weather renderer, the
     * levels set how heavy it is, and a client sent only one of the two shows the wrong thing.
     */
    internal fun sendWeather(weather: WorldWeather) = uiState.sendWeather(weather)

    internal fun syncExperience() = uiState.syncExperience()

    /**
     * Sends the command tree this player may use, which is what makes their client parse commands
     * at all: with no tree, everything after a slash is unknown client-side.
     *
     * Re-sent whenever their permission level changes, because the tree is filtered by it.
     */
    internal fun sendCommandTree() = uiState.sendCommandTree()

    /**
     * This player's row as it stands right now, for the batched periodic flush. A thin delegate:
     * [BasaltServer]'s periodic save calls this by name, so the signature stays here and the work
     * is [PlayerLifecycle]'s.
     */
    internal fun storedSnapshot(): StoredPlayer? = playerLifecycle.storedSnapshot()

    /**
     * Sends to *this* connection from somewhere else: another player's thread, a command, the
     * tick loop. A write failure here is swallowed on purpose: the owning read loop already
     * notices the close and runs [PlayerLifecycle.leavePlayIfPresent], and letting it propagate
     * used to abort a broadcast partway through whenever one recipient had just disconnected.
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
        // Everything queued has to be on the wire before the socket goes, or the client is left
        // guessing why it was dropped. Whoever asked for the kick is often the world's owner, and
        // the owner must not park on a socket, so the closing is left to this connection's own
        // thread: it flushes what is queued and then closes.
        closeWhenDrained.value = 1
        if (!outbox.isOpen) {
            outbox.close()
            connection.close()
        }
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

    internal fun sendCompressed(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        PacketScratch.reset()
        val body = PacketScratch.body
        body.writeVarInt(id)
        writeBody(body)
        var bytes = body.toByteArray()
        // The id is in front of the body, and a plugin is shown the packet without it.
        val idBytes = VarInt.sizeOf(id)
        bytes = packetInterceptor.interceptOutgoing(id, bytes, idBytes) ?: return
        val chain = translation
        if (chain != null) {
            // After the plugins on purpose: a plugin's packet API is this server's version, so it
            // sees and edits the 26.2 shape and the translation happens on the way out of here.
            sendTranslated(chain, id, bytes.copyOfRange(idBytes, bytes.size))
            return
        }
        val frame = PacketScratch.frame
        PacketFraming.writeCompressed(frame, bytes, compressionThreshold, Zlib::compress)
        writeToSocket(frame)
    }

    /**
     * Frames whatever the chain turned one packet into, one packet at a time.
     *
     * Its own buffers rather than the shared scratch: a link may answer with more than one packet,
     * and reusing the scratch would have the second overwrite the first before it is on the wire.
     * A translated client pays for that; a native one never reaches this method.
     */
    private fun sendTranslated(
        chain: ProtocolChain,
        id: Int,
        body: ByteArray,
    ) {
        chain.clientbound(state, id, body) { translatedId, translatedBody ->
            val whole = ByteBuffer()
            whole.writeVarInt(translatedId)
            whole.writeBytes(translatedBody)
            val frame = ByteBuffer()
            PacketFraming.writeCompressed(frame, whole.toByteArray(), compressionThreshold, Zlib::compress)
            writeToSocket(frame)
        }
    }

    /**
     * A message from the client on a named channel.
     *
     * Read only when somebody is listening on that name: a modded client chatters on channels this
     * server has no opinion about, and decoding all of it to throw it away would be a cost every
     * server pays for the few that use one.
     */
    private fun handleCustomPayload(body: ByteBuffer) {
        val deliver = pluginChannel ?: return
        val packet = CustomPayloadPacket.read(body)
        if (pluginListensOn?.invoke(packet.channel) != true) return
        val session = playerSession ?: return
        deliver(packet.channel, session.uuid.toString(), packet.data)
    }

    /**
     * One packet reaches the socket at a time: other threads broadcast into this session too, and
     * without the lock their writes interleave mid-packet. The cipher sits inside the same lock
     * because CFB8 is stateful and concurrent encryption would corrupt both packets.
     */
    private fun writeToSocket(frame: ByteBuffer) {
        // Once this connection has its own outbound thread, every frame goes through the outbox and
        // that thread is the only writer. Before then (login and configuration) there is no other
        // thread to hand it to, so it goes straight out.
        if (outbox.isOpen) {
            outbox.offer(frame.toByteArray())
            return
        }
        sendMutex.withLock { writeFrame(frame.toByteArray()) }
    }

    /** One frame onto the wire, ciphered in send order. Only ever called with [sendMutex] held. */
    private fun writeFrame(frame: ByteArray) {
        val cipher = encryptCipher
        if (cipher != null) {
            connection.write(cipher.update(frame, encrypting = true))
        } else {
            connection.write(frame, 0, frame.size)
        }
    }

    /**
     * Writes everything the outbox has, on this connection's own thread.
     *
     * parity of intent with vanilla's own outbound queue: a client that stops draining delays only
     * itself, and past the outbox's limit it is disconnected rather than left growing a queue
     * nobody will ever read.
     */
    private fun flushOutbox() {
        if (closeWhenDrained.value == 1) {
            sendMutex.withLock {
                for (frame in outbox.drain()) writeFrame(frame)
            }
            outbox.close()
            connection.close()
            return
        }
        if (outbox.hasOverflowed) {
            logger.debug { "Disconnecting a client that stopped draining its socket." }
            connection.close()
            return
        }
        // Taking and writing under the same lock, because two flushers that took their frames first
        // and queued for the socket afterwards would write them in whichever order they woke up.
        sendMutex.withLock {
            for (frame in outbox.drain()) writeFrame(frame)
        }
    }

    internal companion object {
        /** The one enchantment that changes how fast a block comes apart. */
        val EFFICIENCY = Identifier.vanilla("efficiency")

        /**
         * `Display.TextDisplay.DATA_TEXT_ID`: Entity's eight synched fields, then Display's fifteen.
         */
        const val TEXT_DISPLAY_TEXT_INDEX = 23

        /** parity: what a game profile's name may be, which is what the name over a head is. */
        const val PROFILE_NAME_LIMIT = 16

        /** parity: `Display.DATA_BILLBOARD_RENDER_CONSTRAINTS_ID`, and its `CENTER`. */
        const val DISPLAY_BILLBOARD_INDEX = 15
        const val BILLBOARD_CENTER: Byte = 3

        /** The team whose members have no name over their head, which is how an NPC is nameless. */
        const val NAMELESS_TEAM = "basalt_nameless"

        /** How far an NPC notices somebody, and the smallest turn worth a packet. */
        const val NPC_LOOK_RANGE = 10.0
        const val LOOK_STEP = 2f

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

        /** parity: `Player.attack`, where a swing over nine tenths charged counts as a full one. */
        const val FULL_STRENGTH = 0.9f

        /** parity: `Player.attack`'s own `causeFoodExhaustion(0.1)`. */
        const val ATTACK_EXHAUSTION = 0.1f

        /** parity: `LivingEntity.setLivingEntityFlag`'s first bit, the one that says "using". */
        const val LIVING_FLAG_USING_ITEM = 0x01

        /** parity: `Player.entityInteractionRange` plus the slack the packet handler allows. */
        const val ATTACK_REACH = 6.0

        /** parity: the half a sprinting hit adds on top of the 0.4 every hit is worth. */
        const val SPRINT_KNOCKBACK = 0.5

        /** What one radian is worth in degrees, for the angle a hit leans the screen by. */
        const val RADIANS_TO_DEGREES = 57.29577951308232

        const val SOUND_ATTACK_KNOCKBACK = "entity.player.attack.knockback"
        const val SOUND_ATTACK_NO_DAMAGE = "entity.player.attack.nodamage"
        const val SOUND_ATTACK_CRIT = "entity.player.attack.crit"
        const val SOUND_ATTACK_STRONG = "entity.player.attack.strong"
        const val SOUND_ATTACK_WEAK = "entity.player.attack.weak"
        const val SOUND_ATTACK_SWEEP = "entity.player.attack.sweep"

        /** How the stored counters name the two kinds of statistic this build keeps. */
        const val CUSTOM_STAT_PREFIX = "minecraft:custom/minecraft:"
        const val MINED_STAT_PREFIX = "minecraft:mined/"

        /** parity: `isSweepAttack`, whose limit is the walking speed times two and a half. */
        const val SWEEP_SPEED_LIMIT = 0.25

        /** parity: `doSweepAttack`, a point of damage inside a block of the target. */
        const val SWEEP_DAMAGE = 1f
        const val SWEEP_RANGE = 1.0
        const val SWEEP_HEIGHT = 0.25
        const val SWEEP_PLAYER_RANGE = 9.0
        const val SWEEP_PARTICLE_HEIGHT = 0.5

        /** parity: `ItemTags.SWORDS`, which in vanilla is every item whose name ends this way. */
        const val SWORD_SUFFIX = "_sword"

        const val PARTICLE_SWEEP = "sweep_attack"

        const val BOW = "bow"
        const val ARROW_ITEM = "minecraft:arrow"

        /** parity: `BowItem`, whose draw takes a second and whose shot flies at three. */
        const val BOW_DRAW_TICKS = 20f
        const val BOW_POWER_SCALE = 3f
        const val BOW_UNCERTAINTY = 1f
        const val MIN_BOW_POWER = 0.1f
        const val HALF_CIRCLE = 180f
        const val SOUND_ARROW_SHOOT = "entity.arrow.shoot"

        /** Where a rider ends up standing when they get off, clear of the cart's own box. */
        const val DISMOUNT_LIFT = 0.5

        /** parity: `VehicleEntity`, ten per point of damage and gone past forty. */
        const val VEHICLE_DAMAGE_PER_POINT = 10f
        const val VEHICLE_BREAK_DAMAGE = 40f

        /** How long a hit keeps an animal running. parity: `PanicGoal`, which reads the last hit. */
        const val PANIC_TICKS = 40

        /** parity: `EnderpearlItem.use`, which shoots at one and a half with a degree of spread. */
        const val ENDER_PEARL = "ender_pearl"
        const val PEARL_POWER = 1.5f
        const val PEARL_UNCERTAINTY = 1f
        const val SOUND_PEARL_THROW = "entity.ender_pearl.throw"

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

        /** parity: `createItemStackToDrop`, a fiftieth of a block of sideways spread and a tenth up. */
        const val TOSS_SPREAD = 0.02
        const val TOSS_LIFT_SPREAD = 0.1

        /** parity: `Block.popResource`, the quarter block a popped stack lands within. */
        const val POP_JITTER = 0.25
        const val POP_SPREAD = 0.2
        const val ITEM_HALF_HEIGHT = 0.125

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

        /** parity: `AbstractFurnaceBlockEntity`'s three slots, which is how a cooker is told apart. */
        const val FURNACE_SLOTS = 3

        const val MOVE_PLAYER_ROT_ID = 0x20
        const val MOVE_PLAYER_STATUS_ONLY_ID = 0x21

        /** `minecraft:swing`, which this build reads directly: its body is one varint, the hand. */
        const val SWING_PACKET_ID = 0x3f

        /** How far an entity has to move before it is worth a packet, and how far it has to turn. */
        const val POSITION_EPSILON = 0.001
        const val ROTATION_EPSILON = 0.5
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

/** The rest of that byte, in the same order. */
private const val FORWARD_INPUT_FLAG = 1 shl 0
private const val BACK_INPUT_FLAG = 1 shl 1
private const val LEFT_INPUT_FLAG = 1 shl 2
private const val RIGHT_INPUT_FLAG = 1 shl 3

/**
 * The 8 registries in `RegistryDataLoader.SYNCHRONIZED_REGISTRIES` that nothing sent elsewhere in
 * [ClientSession.sendRegistriesThenFinish] populates yet. Sent with zero entries: matches the real
 * 29-registry synced set (see [VanillaRegistryData]'s KDoc) so the client's `RegistryAccess` has an
 * entry for each, which [VanillaTagData]'s tags for them need to resolve at all.
 */
private val EMPTY_SYNCHRONIZED_REGISTRIES =
    listOf(
        "minecraft:chat_type",
        "minecraft:trim_pattern",
        "minecraft:sulfur_cube_archetype",
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
