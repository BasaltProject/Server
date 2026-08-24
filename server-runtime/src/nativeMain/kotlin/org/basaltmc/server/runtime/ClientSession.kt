package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.plugins.PluginPacketBus
import org.basaltmc.server.plugins.PluginSurfaceContent
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.AttributeIds
import org.basaltmc.server.registry.BlockEntityTypes
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.ConsumeEffect
import org.basaltmc.server.registry.CustomStats
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.Fuels
import org.basaltmc.server.registry.ItemArmour
import org.basaltmc.server.registry.ItemBlocks
import org.basaltmc.server.registry.ItemFood
import org.basaltmc.server.registry.ItemStacking
import org.basaltmc.server.registry.ItemTools
import org.basaltmc.server.registry.ItemWeapons
import org.basaltmc.server.registry.Items
import org.basaltmc.server.registry.ParticleTypes
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.PlayerCommandSource
import org.basaltmc.server.runtime.command.commands.BossBarRegistry
import org.basaltmc.server.runtime.command.timePacket
import org.basaltmc.server.runtime.entity.Breeding
import org.basaltmc.server.runtime.entity.DamageSource
import org.basaltmc.server.runtime.entity.EntityCombat
import org.basaltmc.server.runtime.entity.EntityPickResults
import org.basaltmc.server.runtime.entity.Projectiles
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.Taming
import org.basaltmc.server.runtime.entity.Villagers
import org.basaltmc.server.runtime.entity.Wardens
import org.basaltmc.server.runtime.entity.ai.Navigation
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.item.ItemDamageEffects
import org.basaltmc.server.runtime.item.ItemWear
import org.basaltmc.server.runtime.net.SessionOutbox
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
import org.basaltmc.server.runtime.plugins.NpcRegistry
import org.basaltmc.server.runtime.plugins.PacketNames
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.registry.VanillaBiomes
import org.basaltmc.server.runtime.registry.VanillaRegistryData
import org.basaltmc.server.runtime.registry.VanillaTagData
import org.basaltmc.server.runtime.ui.PlayerUi
import org.basaltmc.server.runtime.ui.UiConfigs
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.AdvancementTriggers
import org.basaltmc.server.runtime.world.BedProblem
import org.basaltmc.server.runtime.world.Beds
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.BlockInteractions
import org.basaltmc.server.runtime.world.BlockInventory
import org.basaltmc.server.runtime.world.BlockPlacement
import org.basaltmc.server.runtime.world.Buckets
import org.basaltmc.server.runtime.world.Campfires
import org.basaltmc.server.runtime.world.ChiseledBookshelves
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.server.runtime.world.ContainerOpeners
import org.basaltmc.server.runtime.world.Containers
import org.basaltmc.server.runtime.world.DamageProtection
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.DecoratedPots
import org.basaltmc.server.runtime.world.DetectorRails
import org.basaltmc.server.runtime.world.DoubleBlocks
import org.basaltmc.server.runtime.world.DoubleChests
import org.basaltmc.server.runtime.world.EnchantmentAttributes
import org.basaltmc.server.runtime.world.EndPortals
import org.basaltmc.server.runtime.world.Explosions
import org.basaltmc.server.runtime.world.Fires
import org.basaltmc.server.runtime.world.FluidFlow
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.Furnaces
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.Jukeboxes
import org.basaltmc.server.runtime.world.Lecterns
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.server.runtime.world.MossCarpetTopper
import org.basaltmc.server.runtime.world.NeighbourUpdates
import org.basaltmc.server.runtime.world.NetherPortals
import org.basaltmc.server.runtime.world.NoteBlocks
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.Rails
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
import org.basaltmc.server.runtime.world.Redstone
import org.basaltmc.server.runtime.world.RedstoneWire
import org.basaltmc.server.runtime.world.ScaffoldingPlacement
import org.basaltmc.server.runtime.world.Shelves
import org.basaltmc.server.runtime.world.Shulkers
import org.basaltmc.server.runtime.world.Signs
import org.basaltmc.server.runtime.world.SnowyBlocks
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.Trades
import org.basaltmc.server.runtime.world.Vibrations
import org.basaltmc.server.runtime.world.WallVariants
import org.basaltmc.server.runtime.world.WeaponDamage
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.runtime.world.WorldWeather
import org.basaltmc.server.storage.BanKind
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.server.storage.StoredSpawnPoint
import org.basaltmc.server.storage.StoredStats
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
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
import org.basaltmc.wire.multiversion.ProtocolChain
import org.basaltmc.wire.multiversion.ProtocolGate
import org.basaltmc.wire.multiversion.ProtocolVerdict
import org.basaltmc.wire.multiversion.ProtocolVersions
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
import org.basaltmc.wire.protocol.login.LoginStartPacket
import org.basaltmc.wire.protocol.play.AcceptTeleportationPacket
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.AttackPacket
import org.basaltmc.wire.protocol.play.AwardStatsPacket
import org.basaltmc.wire.protocol.play.BlockDestructionPacket
import org.basaltmc.wire.protocol.play.BlockEntityDataPacket
import org.basaltmc.wire.protocol.play.BlockEventPacket
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
import org.basaltmc.wire.protocol.play.ContainerButtonClickPacket
import org.basaltmc.wire.protocol.play.ContainerClosePacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetDataPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.CooldownPacket
import org.basaltmc.wire.protocol.play.CustomPayloadPacket
import org.basaltmc.wire.protocol.play.DamageEventPacket
import org.basaltmc.wire.protocol.play.Direction
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.EntityPositionSyncPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.wire.protocol.play.GameEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.GlobalPos
import org.basaltmc.wire.protocol.play.Heightmap
import org.basaltmc.wire.protocol.play.HeightmapType
import org.basaltmc.wire.protocol.play.HurtAnimationPacket
import org.basaltmc.wire.protocol.play.InteractPacket
import org.basaltmc.wire.protocol.play.LecternMenuType
import org.basaltmc.wire.protocol.play.LevelEventPacket
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.MerchantCost
import org.basaltmc.wire.protocol.play.MerchantOffer
import org.basaltmc.wire.protocol.play.MerchantOffersPacket
import org.basaltmc.wire.protocol.play.NameTagVisibility
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
import org.basaltmc.wire.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.wire.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEntityMotionPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SetExperiencePacket
import org.basaltmc.wire.protocol.play.SetHealthPacket
import org.basaltmc.wire.protocol.play.SetHeldSlotPacket
import org.basaltmc.wire.protocol.play.SetPassengersPacket
import org.basaltmc.wire.protocol.play.SetPlayerTeamPacket
import org.basaltmc.wire.protocol.play.SetSimulationDistancePacket
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.protocol.play.SignUpdatePacket
import org.basaltmc.wire.protocol.play.SignedChatCommandPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.SpawnInfo
import org.basaltmc.wire.protocol.play.SpawnPositionPacket
import org.basaltmc.wire.protocol.play.StatCounter
import org.basaltmc.wire.protocol.play.StonecutterOption
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import org.basaltmc.wire.protocol.play.UpdateMobEffectPacket
import org.basaltmc.wire.protocol.play.UpdateRecipesPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.status.PingRequestPacket
import org.basaltmc.wire.protocol.status.PongResponsePacket
import org.basaltmc.wire.protocol.status.StatusRequestPacket
import org.basaltmc.wire.protocol.status.StatusResponsePacket
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentNbt
import org.basaltmc.wire.text.LangTable
import org.basaltmc.wire.text.MiniMessage
import org.basaltmc.wire.text.TextColor
import org.basaltmc.wire.text.resolvePlainText
import kotlin.concurrent.AtomicInt
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.roundToLong
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
    /** What a villager offers, out of the jar's own data. `null` means nobody trades. */
    private val villagerTrades: RealVillagerTrades? = null,
    /** The songs a jukebox can play, for the number a comparator reads off one. */
    private val jukeboxes: Jukeboxes? = null,
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
    /** Where plugin events go. Null when there are no plugins, which costs a null check. */
    private val pluginEvents: PluginEventBus? = null,
    /**
     * Whichever plugin is drawing this player's sidebar, asked by uuid.
     *
     * Two lambdas rather than the registry itself: this class has no business knowing what a plugin
     * is, and the cheap question (is anybody holding it) has to stay separate from the expensive one
     * (what do they want drawn), which runs their code.
     */
    private val pluginHoldsSidebar: ((String) -> Boolean)? = null,
    // Only the second one runs plugin code, and only when a redraw is due.
    private val pluginSidebar: ((String) -> PluginSurfaceContent?)? = null,
    private val pluginHoldsBossBar: (() -> Boolean)? = null,
    private val pluginBossBar: ((String) -> PluginSurfaceContent?)? = null,
    /** Where a payload on a plugin's channel goes, and the cheap question of whether anybody wants it. */
    private val pluginChannel: ((String, String, ByteArray) -> Unit)? = null,
    private val pluginListensOn: ((String) -> Boolean)? = null,
    /** The NPCs a plugin put in the world, drawn by this connection like any other player. */
    private val npcs: NpcRegistry? = null,
    /** Who is watching packets. Null when nothing is, which is the usual case and costs one check. */
    private val pluginPackets: PluginPacketBus? = null,
    /** Which clients may in. The default is the native version and nothing else. */
    private val protocolGate: ProtocolGate =
        ProtocolGate(
            setOf(ProtocolVersions.NATIVE),
            UnknownProtocolPolicy.WARN,
            UnknownProtocolPolicy.REFUSE,
        ),
) {
    private val logger = Logger("Session")

    /** What the console renders messages with; empty (so keys print as keys) when no language is loaded. */
    private val consoleLang: LangTable
        get() = commands?.environment?.lang ?: LangTable.EMPTY
    private val sendMutex = NativeMutex()

    /**
     * What is waiting to go out. Open from the moment this connection has its own outbound thread:
     * from then on nobody else touches the socket, so the tick thread never parks on a slow client.
     */
    private val outbox = SessionOutbox()
    private val incoming = ByteBuffer()
    private var state = ProtocolState.HANDSHAKE
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
    private val playerUi =
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

    /** How far the last step moved, squared. parity: `getKnownMovement`, which the sweep reads. */
    private var lastStepSquared = 0.0

    /** Where this player is digging, for how long, and which crack stage everyone else has seen. */
    private var diggingAt: BlockPos? = null
    private var diggingTicks = 0
    private var diggingStage = -1

    /** Which slot is being eaten from, and how many ticks of the mouthful are left. */
    private var usingItem = false
    private var eatingSlot = -1
    private var eatingTicks = 0

    /** What cannot be used again yet, by item name and the tick it frees up on. parity: `ItemCooldowns`. */
    private val itemCooldowns = HashMap<String, Long>()

    /** The bow being drawn, and for how long. parity: `LivingEntity.useItem`/`useItemRemaining`. */
    private var drawingSlot = -1
    private var drawingTicks = 0

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

    /** The cooker whose window is open, if the open window belongs to one. */
    private var openBlock: BlockPos? = null

    /**
     * Whether the open screen is this player's own ender chest rather than a block's slots.
     *
     * parity: `PlayerEnderChestContainer` belongs to the player, so the twenty-seven slots come off
     * the session and go back to `players.db`, not to the chest that was clicked.
     */
    private var openEnderChest: Boolean = false

    /**
     * The window a plugin drew, if one is open: its contents and where a click goes.
     *
     * Not a container. Nothing in it can be taken, because a menu is a row of buttons; a plugin that
     * wants somewhere to put things wants a chest, which the world already has.
     */
    private var pluginMenu: Array<StoredItemStack?>? = null
    private var pluginMenuClick: ((Int) -> Unit)? = null

    /**
     * The other half of an open double chest, whose slots follow the first half's.
     *
     * parity: `ChestBlock.DOUBLE_INVENTORY_COMBINER` over a `CompoundContainer`: two block entities,
     * one window, and which of them leads is decided by the state rather than by which was clicked.
     */
    private var openBlockSecond: BlockPos? = null

    /**
     * Where the opener count was taken, which is the block that was clicked.
     *
     * Not the same as [openBlock] since a double chest opens as its *first* half whichever end was
     * clicked: counting the opening on one block and the closing on another left the lid up for
     * good and a trapped chest powered for good.
     */
    private var openedAt: BlockPos? = null

    /** Whether the open screen is a stonecutter, whose two slots are the menu's own and not a block's. */
    private var openStonecutter: Boolean = false

    /** parity: `StonecutterMenu.input`/`resultSlot`, which live in the menu rather than in the block. */
    private var stonecutterInput: StoredItemStack? = null
    private var stonecutterResult: StoredItemStack? = null

    /** Whether the open screen is a smithing table, whose four slots are the menu's own. */
    private var openSmithing: Boolean = false

    /** parity: `SmithingMenu`'s three inputs and its result, none of which belong to the block. */
    private val smithingSlots = arrayOfNulls<StoredItemStack>(4)

    /** What the open cooker last showed, so a bar is only re-sent when it moves. */
    private var openBlockData = IntArray(4) { -1 }

    /** And the same for its three slots, which the fire changes without anybody clicking. */
    private val shownBlockSlots = arrayOfNulls<StoredItemStack>(3)

    /** How many slots the open block has, which is what tells a chest from a cooker. */
    private var openBlockSize = FURNACE_SLOTS

    /**
     * What this player is holding on the cursor inside their open window, kept server-side rather
     * than taken from the client's own claim: trusting theirs is how a stack gets duplicated.
     */
    private var carriedItem: StoredItemStack? = null

    // The click-and-drag gesture in progress, if any. See [handleQuickCraft].
    private var quickCraftActive = false
    private var quickCraftType = 0
    private val quickCraftSlots = LinkedHashSet<Int>()

    /** The lectern this client has open, and the page it is showing. */
    private var openLecternAt: BlockPos? = null
    private var lecternPage: Int = 0

    /** The villager this client is trading with, and what it offered. */
    private var openTradingWith: Int? = null
    private var openOffers: List<Trades.Offer>? = null

    /** Which dropped items this client has been told about. See [syncItemEntities]. */
    private val trackedItems = HashSet<Int>()

    /** The count each tracked item was last sent with, so a merge can correct it. */
    private val trackedItemCounts = HashMap<Int, Int>()

    /** Summoned entities this client has been told about, so each is spawned once. */
    private val trackedEntities = HashSet<Int>()

    /** NPCs this client has been told about, and which version of each: a change re-draws it. */
    private val trackedNpcs = HashMap<Int, Int>()

    /** Whether this client has been told about the team that hides a name tag. */
    private var namelessTeamMade = false

    /** Where each NPC's head was last pointed for this client, so a still head costs nothing. */
    private val lastNpcLook = HashMap<Int, Pair<Float, Float>>()

    /** Where each of them was last reported, so a mob standing still costs nothing. */
    private val lastEntityPositions = HashMap<Int, DoubleArray>()

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

    /** What the client was last told its level was, so a change is a change rather than a resend. */
    private var lastSyncedLevel: Int = -1

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

    /** Whether this player's turn is already waiting on the owner, so it is not asked for twice. */
    private val tickTaskPending = AtomicInt(0)

    /** Set by a kick: this connection's own thread writes what is left and then closes the socket. */
    private val closeWhenDrained = AtomicInt(0)

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
                                    sendBudget = minOf(sendBudget + desiredChunksPerTick, MAX_BURST_CHUNKS)
                                    val allowed = sendBudget.toInt()
                                    if (allowed > 0) {
                                        timings.of("player.chunks").time {
                                            sendBudget -= flushReadyChunks(allowed).toFloat()
                                        }
                                    }
                                    timings.of("player.tick").time { tickPlayer() }
                                    timings.of("player.players").time { syncPlayers() }
                                    timings.of("player.npcs").time { syncNpcs() }
                                    timings.of("player.posture").time { syncPosture() }
                                    timings.of("player.items").time { syncItemEntities() }
                                    timings.of("player.entities").time { syncSummonedEntities() }
                                    timings.of("player.generated_entities").time { syncGeneratedEntities() }
                                    timings.of("player.ui").time { tickUi() }
                                    timings.of("player.container").time {
                                        if (openBlock != null && openBlockSize == FURNACE_SLOTS) syncOpenContainer()
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
            leavePlayIfPresent()
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
        if (!interceptIncoming(packetId, body)) return
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
        val packet = HandshakePacket.read(body)
        handshake = packet
        verdict = protocolGate.verdictFor(packet.protocolVersion)
        translation = JavaTranslators.chainFor(packet.protocolVersion)
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
        // The one gate before a player exists at all: a plugin that says no here means they never
        // join, and what it says is what they read on the disconnect screen.
        val login =
            pluginEvents.emit(PluginEventTypes.PLAYER_LOGIN) { event ->
                event.set(0, loginStart.name)
                event.set(1, loginStart.profileId.toString())
                event.set(2, connection.remoteAddress)
            }
        if (login?.cancelled == true) {
            val reason = login.cancelMessage ?: "<red>You may not join this server"
            sendUncompressed(LoginDisconnectPacket.ID) { LoginDisconnectPacket(MiniMessage.parse(reason)).write(it) }
            connection.close()
            return
        }
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
        // A client older than 1.20.5 has no such packet and would never answer, so it is not
        // asked: the registries go out straight away rather than waiting for a reply that cannot
        // come.
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
        // The registries a real client needs non-empty (see VanillaRegistryData). The entry names
        // matter, not only their presence: a spawn egg resolves each registry's own `DEFAULT` key,
        // and a client looking up `minecraft:temperate` in an empty chicken_variant crashes.
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
        // parity: `PlayerList.placeNewPlayer` sends the recipes right here, between the abilities
        // and the permission level. The stonecutter draws its own buttons from them, so without it
        // that screen opens empty however many recipes the server knows.
        sendStonecutterRecipes()
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

        // parity: `PlayerList.placeNewPlayer`/`sendLevelInfo`, and the order matters: teleport and
        // player info before level data, and `level_chunks_load_start` right before the chunks.
        // Chunks first leaves a real client on "loading terrain" for ever.
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
                when (ClientCommandPacket.read(body).action) {
                    ClientCommandPacket.Action.PERFORM_RESPAWN -> respawn()
                    ClientCommandPacket.Action.REQUEST_STATS -> sendStats()
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
                handlePlayerAction(action)
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
                    syncHeldAttributes()
                }
            }
            ServerboundSetCreativeModeSlotPacket.ID ->
                handleSetCreativeModeSlot(ServerboundSetCreativeModeSlotPacket.read(body))
            ServerboundContainerClickPacket.ID -> handleContainerClick(ServerboundContainerClickPacket.read(body))
            CustomPayloadPacket.SERVERBOUND_ID -> handleCustomPayload(body)
            ContainerClosePacket.SERVERBOUND_ID -> {
                ContainerClosePacket.read(body)
                playerEvent(PluginEventTypes.INVENTORY_CLOSE) { event ->
                    event.set(3, if (openMenu.containerId == 0) "player" else "container")
                }
                playerSession?.let(::closeOpenMenu)
            }
            InteractPacket.ID -> handleInteract(InteractPacket.read(body))
            AttackPacket.ID -> handleAttack(AttackPacket.read(body))
            SelectTradePacket.ID -> selectTrade(SelectTradePacket.read(body).index)
            ContainerButtonClickPacket.ID ->
                clickContainerButton(ContainerButtonClickPacket.read(body).buttonId)
            SignUpdatePacket.ID -> writeSign(SignUpdatePacket.read(body))
            SWING_PACKET_ID -> handleSwing(body.readVarInt())
            ServerboundUseItemOnPacket.ID -> handleUseItemOn(ServerboundUseItemOnPacket.read(body))
            ServerboundUseItemPacket.ID -> handleUseItem(ServerboundUseItemPacket.read(body))
            ServerboundPickItemFromEntityPacket.ID ->
                handlePickItemFromEntity(ServerboundPickItemFromEntityPacket.read(body))
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
            ServerboundPlayerActionPacket.Action.RELEASE_USE_ITEM -> {
                stopEating()
                releaseBow()
            }
            ServerboundPlayerActionPacket.Action.SWAP_ITEM_WITH_OFFHAND -> swapWithOffhand()
            else -> Unit
        }
        // The cracks everyone else sees. The digger draws its own, which is why this never goes
        // back to them. parity: `ServerPlayerGameMode.destroyAndAck` and its `destroyProgress`.
        when (action.action) {
            ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK -> {
                // parity: `NoteBlock.attack`, which plays the note without moving it on.
                currentWorld?.let { loaded ->
                    val struck = NoteBlocks.strike(loaded, action.pos.x, action.pos.y, action.pos.z)
                    if (struck.isNotEmpty()) {
                        val stateId = world.getBlockState(action.pos.x, action.pos.y, action.pos.z) ?: 0
                        broadcastEffects(action.pos, stateId, struck)
                    }
                }
                startDigging(action.pos)
            }
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
        // The question a protection plugin answers, asked where the server would have gone ahead.
        // Cancelling puts the block back on the digger's screen: the client broke it locally the
        // moment it started, so saying nothing would leave a hole only they can see.
        val breakEvent =
            pluginEvents.emit(PluginEventTypes.BLOCK_BREAK) { event ->
                event.set(0, playerSession?.name)
                event.set(1, playerSession?.uuid?.toString())
                event.set(2, currentWorld?.name)
                event.set(3, action.pos.x)
                event.set(4, action.pos.y)
                event.set(5, action.pos.z)
                event.set(6, broken?.let { BlockStateDefinitions.blockOf(it) })
            }
        if (breakEvent?.cancelled == true) {
            sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(action.sequence).write(it)
            }
            sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(action.pos, broken ?: 0).write(it)
            }
            breakEvent.cancelMessage?.let { message -> sendPluginNotice(message) }
            stopDigging()
            return
        }
        // parity: `ShulkerBoxBlock.getDrops`, read before the block goes: breaking it takes the
        // block entity with it, and the contents have to travel in the item.
        val carried =
            currentWorld
                ?.takeIf {
                    Shulkers.isShulkerBox(BlockStateDefinitions.blockOf(broken ?: 0)?.path)
                }?.let { Shulkers.contentsOf(it, action.pos.x, action.pos.y, action.pos.z) }
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
        // parity: `GameEvent.BLOCK_DESTROY`, which a sculk sensor hears as twelve.
        heardBySculk("block_destroy", action.pos)
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
            if (correctTool) dropLootFor(brokenBlock.block, action.pos, carried)
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
        val wearing =
            playerEvent(PluginEventTypes.PLAYER_ITEM_DAMAGE) { event ->
                event.set(3, held.id)
                event.set(4, cost)
                event.set(5, worn == null)
            }
        if (wearing?.cancelled == true) return
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
     * parity: `BellBlock.onHit` with `isProperHit`: a bell only swings when it is struck on a side
     * it can swing towards, and never from a hand above or below it.
     */
    private fun ringBell(hit: BlockHitResult): Boolean {
        val stateId = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return false
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val face = hit.direction
        if (face == Direction.UP || face == Direction.DOWN) return false
        if (hit.clickY > bellHitTop) return false
        val facing = properties["facing"] ?: "north"
        val sameAxis = axisOf(facing) == axisOf(face.name.lowercase())
        val proper =
            when (properties["attachment"]) {
                "floor" -> sameAxis
                "ceiling" -> true
                else -> !sameAxis
            }
        if (!proper) return false
        broadcastEffects(
            hit.pos,
            stateId,
            listOf(
                BlockEffect.Event(b0 = 1, b1 = frameDirectionData(face.name.lowercase())),
                BlockEffect.Sound("block.bell.use", SoundSource.BLOCKS, volume = 2f, pitch = 1f),
            ),
        )
        return true
    }

    private fun axisOf(direction: String): String = if (direction == "north" || direction == "south") "z" else "x"

    /** parity: `BellBlock.isProperHit`, which will not take a click on the very top of the block. */
    private val bellHitTop = 0.8124f

    /** parity: `MinecartItem.useOn`, the sixteenth of a block a cart sits above its rail. */
    private val minecartRestHeight = 0.0625

    /** The other half of a block change: what it asks everyone nearby to hear or see. */
    internal fun broadcastEffects(
        pos: BlockPos,
        stateId: Int,
        effects: List<BlockEffect>,
    ) {
        for (effect in effects) {
            when (effect) {
                is BlockEffect.Sound -> {
                    val sound =
                        SoundPacket(
                            sound = effect.name,
                            source = effect.source,
                            x = pos.x + 0.5,
                            y = pos.y + 0.5,
                            z = pos.z + 0.5,
                            volume = effect.volume,
                            pitch = effect.pitch,
                            seed = Random.nextLong(),
                        )
                    for (player in playersInSameWorld()) {
                        player.connection.sendPlayPacket(SoundPacket.ID) { sound.write(it) }
                    }
                }

                is BlockEffect.Event -> {
                    val blockId = BlockSupport.blockRegistryIdOf(stateId)
                    for (player in playersInSameWorld()) {
                        player.connection.sendPlayPacket(BlockEventPacket.ID) {
                            BlockEventPacket(pos, effect.b0, effect.b1, blockId).write(it)
                        }
                    }
                }

                is BlockEffect.Effect -> {
                    for (player in playersInSameWorld()) {
                        player.connection.sendPlayPacket(LevelEventPacket.ID) {
                            LevelEventPacket(effect.type, pos, effect.data).write(it)
                        }
                    }
                }
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
        val dropped =
            playerEvent(PluginEventTypes.PLAYER_ITEM_DROP) { event ->
                event.set(3, held.id)
                event.set(4, dropCount)
            }
        if (dropped?.cancelled == true) {
            dropped.cancelMessage?.let { sendPluginNotice(it) }
            // The client has already taken it out of its own hand.
            sendInventorySlot(heldSlot)
            return
        }
        val remaining = held.count - dropCount
        session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
        sendInventorySlot(heldSlot)

        // parity: `Player.drop` throws it forward from eye height along the look direction, rather
        // than dropping it on the player's own feet where they would instantly walk back into it.
        val yaw = session.yaw * PI / 180.0
        val pitch = session.pitch * PI / 180.0
        val tossAngle = Random.nextDouble() * 2 * PI
        val tossSpread = TOSS_SPREAD * Random.nextDouble()
        spawnDroppedItem(
            item = Identifier.parse(held.id),
            count = dropCount,
            x = session.x,
            y = session.y + THROW_EYE_HEIGHT,
            z = session.z,
            velocityX = -sin(yaw) * cos(pitch) * THROW_SPEED + cos(tossAngle) * tossSpread,
            velocityY = -sin(pitch) * THROW_SPEED + THROW_LIFT + tossLift(),
            velocityZ = cos(yaw) * cos(pitch) * THROW_SPEED + sin(tossAngle) * tossSpread,
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
        var lines = update.lines
        val signed =
            playerEvent(PluginEventTypes.PLAYER_SIGN_CHANGE) { event ->
                event.set(3, pos.x)
                event.set(4, pos.y)
                event.set(5, pos.z)
                for (line in 0 until 4) event.set(6 + line, lines.getOrNull(line).orEmpty())
            }
        if (signed != null) {
            if (signed.cancelled) {
                signed.cancelMessage?.let { sendPluginNotice(it) }
                return
            }
            lines = List(4) { line -> (signed.get(6 + line) as? String).orEmpty() }
        }
        val data = Signs.withText(existing?.data ?: Signs.emptyData(), update.isFrontText, lines)
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
        cause: String = "explosion",
    ) {
        val stopped =
            pluginEvents.emit(PluginEventTypes.BLOCK_EXPLODE) { event ->
                event.set(0, world.name)
                event.set(1, x)
                event.set(2, y)
                event.set(3, z)
                event.set(4, power.toDouble())
                event.set(5, cause)
                event.set(6, playerSession?.entityId)
            }
        if (stopped?.cancelled == true) return
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
     * Tells the sculk sensors around [pos] that something happened there.
     *
     * parity: `Level.gameEvent`, whose whole audience in this build is the sensors: what they hear
     * is the event's own frequency, and what that lights is ordinary redstone.
     */
    private fun heardBySculk(
        event: String,
        pos: BlockPos,
    ) {
        val world = currentWorld ?: return
        val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        Vibrations.emit(world, event, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, changes)
        for (change in changes) {
            broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
            for (settled in Redstone.settle(world, change.x, change.y, change.z)) {
                broadcastBlockUpdate(BlockPos(settled.x, settled.y, settled.z), settled.stateId)
            }
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
        val effects = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        NeighbourUpdates.afterChange(
            world,
            pos.x,
            pos.y,
            pos.z,
            onReshaped = { stateId, x, y, z -> broadcastBlockUpdate(BlockPos(x, y, z), stateId) },
            effects = effects,
            // parity: `updateNeighbourShapes` reshapes the neighbours and never the block itself.
            reshapeSelf = false,
        ) { stateId, x, y, z ->
            val at = BlockPos(x, y, z)
            broadcastBlockUpdate(at, Blocks.AIR.stateId)
            Blocks.byStateId(stateId)?.let { dropLootFor(it.block, at) }
        }
        for (change in effects) {
            broadcastEffects(BlockPos(change.x, change.y, change.z), change.stateId, change.effects)
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
        carried: NbtCompound? = null,
    ) {
        val table = lootTables?.forBlock(block) ?: return
        val drops = table.generate(LegacyRandom(Random.nextLong()))
        if (drops.isEmpty()) return
        for (stack in drops) {
            // Falls under its own gravity now ([WorldState.tickItems]) instead of being snapped to
            // the ground up front, so a block broken in mid-air drops as it should.
            val components = NbtCompound()
            if (carried != null && stack.item == block) components[Shulkers.CONTAINER] = carried
            // parity: `Block.popResource`, which scatters the stack inside the block it came from
            // and pops it up, rather than dropping every stack on the same point.
            spawnDroppedItem(
                item = stack.item,
                count = stack.count,
                x = pos.x + 0.5 + popJitter(),
                y = pos.y + 0.5 + popJitter() - ITEM_HALF_HEIGHT,
                z = pos.z + 0.5 + popJitter(),
                velocityX = Random.nextDouble() * POP_SPREAD - POP_SPREAD / 2.0,
                velocityY = BREAK_DROP_LIFT,
                velocityZ = Random.nextDouble() * POP_SPREAD - POP_SPREAD / 2.0,
                pickupDelay = BREAK_DROP_PICKUP_DELAY,
                components = components,
            )
        }
    }

    private fun isAirLike(stateId: Int): Boolean =
        stateId == Blocks.AIR.stateId || stateId == Blocks.CAVE_AIR.stateId || stateId == Blocks.VOID_AIR.stateId

    /** parity: `Block.popResource`'s own `Mth.nextDouble(random, -0.25, 0.25)`. */
    private fun popJitter(): Double = Random.nextDouble() * 2 * POP_JITTER - POP_JITTER

    /** parity: `createItemStackToDrop`, whose toss is never quite straight. */
    private fun tossLift(): Double = (Random.nextDouble() - Random.nextDouble()) * TOSS_LIFT_SPREAD

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
        components: NbtCompound = NbtCompound(),
    ) {
        if (count <= 0) return
        if (Items.REGISTRY.byIdentifier(item) == null) return
        val spawning =
            pluginEvents.emit(PluginEventTypes.ITEM_SPAWN) { event ->
                event.set(0, currentWorld?.name)
                event.set(1, x)
                event.set(2, y)
                event.set(3, z)
                event.set(4, item.toString())
                event.set(5, count)
                event.set(6, "drop")
            }
        if (spawning?.cancelled == true) return
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
                components = components,
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
        val me = session.uuid.toString()
        for (entity in world.entities()) {
            if (entity.removed) continue
            // An entity somebody else can see and this player cannot: taken away if they were
            // already shown it, and skipped if they were not.
            val visible = entity.visibleTo?.contains(me) ?: true
            if (!visible) {
                if (trackedEntities.remove(entity.entityId)) {
                    lastEntityPositions.remove(entity.entityId)
                    sendPlayPacket(RemoveEntitiesPacket.ID) {
                        RemoveEntitiesPacket(listOf(entity.entityId)).write(it)
                    }
                }
                continue
            }
            if (trackedEntities.add(entity.entityId)) {
                if (Taming.isTamable(entity.type) && Taming.flagsOf(entity) != 0) {
                    sendPlayPacket(SetEntityDataPacket.ID) {
                        SetEntityDataPacket(
                            entity.entityId,
                            listOf(
                                SetEntityDataPacket.Entry.Bytes(
                                    Taming.FLAGS_INDEX,
                                    Taming.flagsOf(entity).toByte(),
                                ),
                            ),
                        ).write(it)
                    }
                }
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
                        velocityX = entity.velocityX,
                        velocityY = entity.velocityY,
                        velocityZ = entity.velocityZ,
                    ).write(it)
                }
                entity.sourceNbt?.let { data ->
                    val metadata = generatedEntityMetadata(entity.type.path, data)
                    if (metadata.isNotEmpty()) {
                        sendPlayPacket(SetEntityDataPacket.ID) {
                            SetEntityDataPacket(entity.entityId, metadata).write(it)
                        }
                    }
                    val equipment =
                        generatedEntityEquipment(data).mapNotNull { slot ->
                            val item =
                                Items.REGISTRY.byIdentifier(Identifier.parse(slot.item)) ?: return@mapNotNull null
                            SetEquipmentPacket.Slot(slot.slot, WireItemStack(item.protocolId, slot.count))
                        }
                    if (equipment.isNotEmpty()) {
                        sendPlayPacket(SetEquipmentPacket.ID) {
                            SetEquipmentPacket(entity.entityId, equipment).write(it)
                        }
                    }
                }
                entity.displayText?.let { text -> sendDisplayText(entity.entityId, text) }
                continue
            }
            // parity: `ServerEntity.sendChanges`, which sends nothing at all for an entity that has
            // not moved. A pen of animals standing still is free to watch; without this every one of
            // them costs a packet per player per tick.
            val last = lastEntityPositions[entity.entityId]
            val moved =
                last == null ||
                    abs(last[0] - entity.x) > POSITION_EPSILON ||
                    abs(last[1] - entity.y) > POSITION_EPSILON ||
                    abs(last[2] - entity.z) > POSITION_EPSILON ||
                    abs(last[3] - entity.yaw.toDouble()) > ROTATION_EPSILON ||
                    abs(last[4] - entity.headYaw.toDouble()) > ROTATION_EPSILON
            if (!moved) continue
            // parity: `ClientboundRotateHeadPacket`, which is the only thing that turns a mob's head.
            if (last == null || abs(last[4] - entity.headYaw.toDouble()) > ROTATION_EPSILON) {
                sendPlayPacket(RotateHeadPacket.ID) {
                    RotateHeadPacket(entity.entityId, entity.headYaw).write(it)
                }
            }
            lastEntityPositions[entity.entityId] =
                doubleArrayOf(entity.x, entity.y, entity.z, entity.yaw.toDouble(), entity.headYaw.toDouble())
            // parity: the position *sync*, which carries the velocity so the client can interpolate:
            // a teleport every tick snaps, which is what made a falling block judder down.
            sendPlayPacket(EntityPositionSyncPacket.ID) {
                EntityPositionSyncPacket(
                    entityId = entity.entityId,
                    x = entity.x,
                    y = entity.y,
                    z = entity.z,
                    velocityX = entity.velocityX,
                    velocityY = entity.velocityY,
                    velocityZ = entity.velocityZ,
                    yaw = entity.yaw,
                    pitch = entity.pitch,
                    onGround = entity.onGround,
                ).write(it)
            }
        }
        val gone = world.collectRemovedEntities().filter { trackedEntities.remove(it) }
        for (entityId in gone) lastEntityPositions.remove(entityId)
        if (gone.isNotEmpty()) {
            sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
        }
    }

    /**
     * The profile name an NPC travels under, which nobody ever reads.
     *
     * A player entity must have one, and it must be a plain string of sixteen characters, so it
     * cannot be the name a plugin wrote: that is the display floating above. This is a made-up name
     * whose only job is to be the handle the hiding team joins by, so it has to be unique and it has
     * to be exactly what the team is told.
     */
    private fun npcProfileName(entityId: Int): String = "npc_$entityId".take(PROFILE_NAME_LIMIT)

    /**
     * The text on a `text_display`, which is metadata 23 and a component.
     *
     * parity: `Display.TextDisplay.DATA_TEXT_ID`, whose index is Entity's eight fields plus
     * Display's fifteen. Getting that number by counting is exactly why it is written down here
     * once instead of at each call.
     */
    private fun sendDisplayText(
        entityId: Int,
        text: String,
    ) {
        sendPlayPacket(SetEntityDataPacket.ID) {
            SetEntityDataPacket(
                entityId,
                listOf(
                    SetEntityDataPacket.Entry.Text(TEXT_DISPLAY_TEXT_INDEX, MiniMessage.parse(text)),
                    // Turned to face whoever is reading it. A display left as it spawned is drawn
                    // flat in the world, so from the other side it reads backwards.
                    SetEntityDataPacket.Entry.Bytes(DISPLAY_BILLBOARD_INDEX, BILLBOARD_CENTER),
                ),
            ).write(it)
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

    /** Whether the head moved enough to be worth a packet: a degree of jitter is not. */
    private fun lookedAt(
        entityId: Int,
        yaw: Float,
        pitch: Float,
    ): Boolean {
        val last = lastNpcLook[entityId]
        if (last != null && abs(last.first - yaw) < LOOK_STEP && abs(last.second - pitch) < LOOK_STEP) return false
        lastNpcLook[entityId] = yaw to pitch
        return true
    }

    /**
     * Puts one name on the team whose name tags are never drawn.
     *
     * A player entity always has a tag over its head, even with an empty profile name, which is the
     * faint mark left above an NPC. There is no flag for it: a team with `never` is how vanilla
     * itself hides one, so that is what this does. The team is made once and everybody joins it.
     */
    private fun hideNameTagOf(member: String) {
        if (!namelessTeamMade) {
            namelessTeamMade = true
            sendPlayPacket(SetPlayerTeamPacket.ID) {
                SetPlayerTeamPacket(
                    teamName = NAMELESS_TEAM,
                    method = SetPlayerTeamPacket.METHOD_ADD,
                    parameters =
                        SetPlayerTeamPacket.Parameters(
                            displayName = Component.text(NAMELESS_TEAM),
                            nameTagVisibility = NameTagVisibility.NEVER,
                        ),
                ).write(it)
            }
        }
        sendPlayPacket(SetPlayerTeamPacket.ID) {
            SetPlayerTeamPacket(
                teamName = NAMELESS_TEAM,
                method = SetPlayerTeamPacket.METHOD_JOIN,
                members = listOf(member),
            ).write(it)
        }
    }

    /**
     * Forgets everything this client was told to draw, because a respawn makes it forget too.
     *
     * parity: the client throws away every entity when it changes dimension or respawns, so a
     * server that still believes it sent them never sends them again. That is a hologram that was
     * there before you died and is not there after.
     */
    private fun forgetEverythingDrawn() {
        trackedEntities.clear()
        trackedPlayers.clear()
        trackedNpcs.clear()
        namelessTeamMade = false
        lastNpcLook.clear()
        trackedEquipment.clear()
        lastEntityPositions.clear()
    }

    /**
     * Draws the NPCs a plugin made, the same way [syncPlayers] draws real ones.
     *
     * Same shape on purpose: an NPC is a player entity, so it needs the tab-list entry first (that
     * is where the skin lives) and the entity second. `listed = false` keeps it out of the tab list
     * while still giving the client the profile it needs to draw a face.
     */
    private fun syncNpcs() {
        val session = playerSession ?: return
        val registry = npcs ?: return
        val me = session.uuid.toString()
        val here =
            registry.all().filter { it.world == session.worldName && (it.visibleTo?.contains(me) ?: true) }
        for (npc in here) {
            val known = trackedNpcs[npc.entityId]
            if (known == npc.version) continue
            if (known != null) {
                // Something a client was already told changed: taken away and drawn again, which is
                // what the profile requires anyway since a skin cannot be changed in place.
                sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(listOf(npc.entityId)).write(it) }
            }
            trackedNpcs[npc.entityId] = npc.version
            // Before the entity: the tag is hidden by the team this name belongs to, and a tag
            // drawn before the team arrives flashes on screen.
            hideNameTagOf(npcProfileName(npc.entityId))
            val properties =
                npc.skinTexture?.let { texture ->
                    listOf(GameProfileProperty("textures", texture, npc.skinSignature))
                } ?: emptyList()
            sendPlayPacket(PlayerInfoUpdatePacket.ID) { buffer ->
                PlayerInfoUpdatePacket
                    .initializing(
                        listOf(
                            PlayerInfoEntry(
                                npc.uuid,
                                npcProfileName(npc.entityId),
                                listed = false,
                                gamemode = Gamemode.SURVIVAL,
                                properties = properties,
                            ),
                        ),
                    ).write(buffer)
            }
            sendPlayPacket(SpawnEntityPacket.ID) { buffer ->
                SpawnEntityPacket(
                    entityId = npc.entityId,
                    uuid = npc.uuid,
                    entityType = SpawnEntityPacket.PLAYER_ENTITY_TYPE,
                    x = npc.x,
                    y = npc.y,
                    z = npc.z,
                    pitch = npc.pitch,
                    yaw = npc.yaw,
                    headYaw = npc.yaw,
                ).write(buffer)
            }
            sendPlayPacket(RotateHeadPacket.ID) { RotateHeadPacket(npc.entityId, npc.yaw).write(it) }
            // Every layer of the skin on, or an NPC is drawn without its jacket and hat.
            sendPlayPacket(SetEntityDataPacket.ID) { buffer ->
                SetEntityDataPacket.skinParts(npc.entityId, PlayerSession.ALL_SKIN_PARTS).write(buffer)
            }
        }
        // Heads turn toward whoever is looking, which is per connection: each player sees the NPC
        // looking at them, and none of it is state the server has to keep.
        for (npc in here) {
            if (!npc.looksAtPlayers) continue
            val dx: Double = session.x - npc.x
            // Eyes to eyes, so a tall NPC does not stare at your feet.
            val dy: Double = session.y - npc.y
            val dz: Double = session.z - npc.z
            val flat: Double = dx * dx + dz * dz
            if (flat > NPC_LOOK_RANGE * NPC_LOOK_RANGE) continue
            val yaw = (atan2(dz, dx) * RADIANS_TO_DEGREES - 90.0).toFloat()
            val pitch = (-atan2(dy, sqrt(flat)) * RADIANS_TO_DEGREES).toFloat()
            if (!lookedAt(npc.entityId, yaw, pitch)) continue
            sendPlayPacket(RotateHeadPacket.ID) { RotateHeadPacket(npc.entityId, yaw).write(it) }
            sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(npc.entityId, npc.x, npc.y, npc.z, yaw, pitch, onGround = true).write(it)
            }
        }

        val visible = here.map { it.entityId }.toSet()
        val gone = trackedNpcs.keys.filterNot { it in visible }
        if (gone.isEmpty()) return
        for (entityId in gone) trackedNpcs.remove(entityId)
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
                trackedItemCounts[item.entityId] = item.count
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
            } else if (item.countChanged && trackedItemCounts[item.entityId] != item.count) {
                // A merge changed the stack: the client draws the number it was told, so it has to
                // hear the new one or two logs on the floor keep looking like one.
                trackedItemCounts[item.entityId] = item.count
                sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket.item(item.entityId, WireItemStack(protocolId, item.count)).write(it)
                }
            } else if (item.resting && settledItems.add(item.entityId)) {
                // One correction when it stops, and nothing while it falls: the client simulates
                // the item from the velocity it was spawned with, and a teleport every tick fights
                // that simulation on a different clock, which shows up as a stutter.
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
        for (id in gone) trackedItemCounts.remove(id)
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

    /**
     * Placing a block: resolves the held item to a block state by matching identifiers (so only
     * plain blocks whose item shares the block's own name place correctly), then places it in the
     * empty space [ServerboundUseItemOnPacket.blockHit] points at.
     *
     * parity: `ServerPlayerGameMode.useItemOn`, which answers a click on a block before it
     * considers placing anything: a container opens, a lever flips, a bed is slept in. Off-hand
     * placing is a documented gap, since only the main hand is read.
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
        // Asked before the click does anything at all: opening a chest, using a door and placing a
        // block are all this one event, which is why it carries what was clicked and what was held.
        val heldStack = session.inventory[PlayerInventory.HOTBAR_START + session.inventory.heldSlot]
        val clickedState = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
        val interaction =
            playerEvent(PluginEventTypes.PLAYER_INTERACT) { event ->
                event.set(3, "right_click_block")
                event.set(4, hit.pos.x)
                event.set(5, hit.pos.y)
                event.set(6, hit.pos.z)
                event.set(7, clickedState?.let { BlockStateDefinitions.blockOf(it) })
                event.set(8, heldStack?.id)
                event.set(9, "main")
            }
        if (interaction?.cancelled == true) {
            interaction.cancelMessage?.let { sendPluginNotice(it) }
            // The client predicted whatever it was about to do: put the block and the hand back.
            sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(hit.pos, clickedState ?: 0).write(it)
            }
            sendInventory()
            return ack()
        }
        // parity: `ServerPlayerGameMode.useItemOn` runs the block's own use before it considers the
        // item as something to place, and sneaking overrides that and places instead.
        if (!session.sneaking) {
            val clicked = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)?.let(Blocks::byStateId)
            if (clicked?.block == CRAFTING_TABLE) {
                openCraftingTable(session)
                return ack()
            }
            val cookerPath = clicked?.block?.path
            if (cookerPath != null && cookerPath in Furnaces.COOKERS) {
                openFurnace(session, hit.pos, cookerPath)
                return ack()
            }
            if (cookerPath != null && cookerPath in Containers.PLAIN) {
                openChest(session, hit.pos, cookerPath)
                return ack()
            }
            if (cookerPath == "crafter") {
                openCrafter(session, hit.pos)
                return ack()
            }
            // parity: `JukeboxBlock.useItemOn`/`useWithoutItem`: a disc goes in, and clicking a
            // playing one takes it back out.
            if (cookerPath == "jukebox" && useJukebox(session, hit.pos)) return ack()
            // parity: `LecternBlock.useItemOn`, which takes a book, and `useWithoutItem`, which
            // opens the one already on it.
            if (cookerPath == "lectern" && useLectern(session, hit.pos)) return ack()
            // parity: `CampfireBlock.useItemOn`, which takes one piece of food onto the fire and
            // refuses anything a campfire cannot cook.
            if (cookerPath != null && Campfires.isCampfire(cookerPath) && placeOnCampfire(session, hit.pos)) {
                return ack()
            }
            // parity: `SmithingTableBlock.useWithoutItem`, whose four slots belong to the screen.
            if (cookerPath == "smithing_table") {
                openSmithingMenu(session)
                return ack()
            }
            // parity: `StonecutterBlock.useWithoutItem`, whose two slots belong to the screen.
            if (cookerPath == "stonecutter") {
                openStonecutterMenu(session)
                return ack()
            }
            // parity: `EnderChestBlock.useWithoutItem`, the player's own slots and not the block's.
            if (cookerPath == "ender_chest") {
                openEnderChest(session, hit.pos)
                return ack()
            }
            // parity: `ChiseledBookShelfBlock`, which takes a book into the slot that was clicked
            // and gives it back from the same one.
            if (ChiseledBookshelves.isBookshelf(cookerPath)) {
                useBookshelf(session, hit)
                return ack()
            }
            // parity: `ShelfBlock.useItemOn`, one slot by hand and the whole hotbar when powered.
            if (Shelves.isShelf(cookerPath)) {
                useShelf(session, hit)
                return ack()
            }
            // parity: `DecoratedPotBlock.useItemOn`, one item at a time into its single slot, and
            // `useWithoutItem`, which only rocks it.
            if (DecoratedPots.isPot(cookerPath)) {
                usePot(session, hit.pos)
                return ack()
            }
            // parity: `NoteBlock.useWithoutItem`, which moves the note on one and plays it.
            if (cookerPath == "note_block") {
                val loaded = currentWorld ?: return ack()
                NoteBlocks.tune(loaded, hit.pos.x, hit.pos.y, hit.pos.z)?.let { change ->
                    broadcastBlockUpdate(hit.pos, change.stateId)
                    broadcastEffects(hit.pos, change.stateId, change.effects)
                }
                return ack()
            }
            // parity: `BellBlock.useWithoutItem`, which only rings from a side that can swing it.
            if (cookerPath == "bell" && ringBell(hit)) return ack()
            val clickedState = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
            if (clickedState != null && Beds.isBed(clickedState)) {
                useBed(session, hit.pos, clickedState)
                return ack()
            }
            if (clickedState != null && BlockInteractions.isToggleable(clickedState)) {
                toggleBlock(hit.pos)
                return ack()
            }
            // parity: `RepeaterBlock.useWithoutItem` and `ComparatorBlock.useWithoutItem`, the
            // right click that moves the delay on a notch or flips the comparator's mode.
            if (clickedState != null && BlockInteractions.isCycleable(clickedState)) {
                cycleBlock(hit.pos)
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
        // parity: `MinecartItem.useOn`, which only works on a rail and sits the cart on it.
        if (held != null && heldBlock != null && heldBlock.path.endsWith("minecart")) {
            if (placeMinecart(session, heldBlock, heldSlot, held, hit)) return ack()
        }
        // parity: `BoatItem.use`, which puts the boat where the player is looking.
        if (held != null && heldBlock != null && isBoat(heldBlock)) {
            if (placeBoat(session, heldBlock, heldSlot, held, hit)) return ack()
        }
        // parity: `SpawnEggItem.useOn`, which drops the mob on top of what was clicked.
        if (held != null && heldBlock != null && heldBlock.path.endsWith("_spawn_egg")) {
            if (useSpawnEgg(session, heldBlock, heldSlot, held, hit)) return ack()
        }
        if (held != null && heldBlock != null && Buckets.isBucket(heldBlock)) {
            useBucketOn(session, heldBlock, heldSlot, held, hit)
            return ack()
        }
        // parity: `StandingAndWallBlockItem` and friends: a torch, a sign, a banner or a head put
        // against a side goes down as its wall form, not as the standing one refusing to stand.
        val againstAWall =
            hit.direction != org.basaltmc.wire.protocol.play.Direction.UP &&
                hit.direction != org.basaltmc.wire.protocol.play.Direction.DOWN
        // parity: `BlockItem.getBlock`, which is not the item's own name for redstone dust, string
        // or any seed.
        val placedBlock =
            heldBlock?.let { ItemBlocks.of(it) }?.let { if (againstAWall) WallVariants.of(it) ?: it else it }
        val defaultStateId = placedBlock?.let { Blocks.REGISTRY.idOf(it) }
        if (held == null || placedBlock == null || defaultStateId == null) return ack()

        // parity: `BlockPlaceContext`: the clicked block is replaced when it is one of the things
        // that give way (grass, a fern, a snow layer, fire, a fluid), and the block goes next to it
        // otherwise. A flower, a firefly bush and leaves are not replaceable, so those get built on.
        val clickedForPlacing = world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
        val replaceClicked =
            clickedForPlacing != null &&
                BlockShapes.isReplaceable(clickedForPlacing) &&
                BlockStateDefinitions.blockOf(clickedForPlacing) != heldBlock
        // parity: `ScaffoldingBlockItem.updatePlacementContext`, the one placement rule that lives in
        // the item: clicking a scaffolding builds up the tower instead of hanging one off the face.
        val placementWorld = currentWorld
        val scaffoldTarget =
            if (heldBlock.path == "scaffolding" && placementWorld != null) {
                ScaffoldingPlacement.target(
                    placementWorld,
                    hit.pos.x,
                    hit.pos.y,
                    hit.pos.z,
                    hit.direction,
                    session.yaw,
                    session.sneaking,
                    hit.inside,
                )
            } else {
                null
            }
        val placeAt =
            when {
                scaffoldTarget != null -> BlockPos(scaffoldTarget.first, scaffoldTarget.second, scaffoldTarget.third)
                replaceClicked -> hit.pos
                else -> hit.pos.offset(hit.direction)
            }
        val existing = world.getBlockState(placeAt.x, placeAt.y, placeAt.z)
        if (existing == null || !BlockShapes.isReplaceable(existing)) return ack()

        val placedStateId =
            BlockPlacement.stateFor(
                block = placedBlock,
                defaultStateId = defaultStateId,
                face = hit.direction,
                clickY = hit.clickY,
                yaw = session.yaw,
                pitch = session.pitch,
                replacing = existing,
                world = currentWorld,
                x = placeAt.x,
                y = placeAt.y,
                z = placeAt.z,
                clickX = hit.clickX,
                clickZ = hit.clickZ,
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
        // Asked once the server has decided the block would go down: a plugin that says no is
        // answering the same question the game just answered yes to, and the client is told to put
        // its own prediction back.
        val placeEvent =
            pluginEvents.emit(PluginEventTypes.BLOCK_PLACE) { event ->
                event.set(0, playerSession?.name)
                event.set(1, playerSession?.uuid?.toString())
                event.set(2, currentWorld?.name)
                event.set(3, placeAt.x)
                event.set(4, placeAt.y)
                event.set(5, placeAt.z)
                event.set(6, BlockStateDefinitions.blockOf(blockStateId))
            }
        if (placeEvent?.cancelled == true) {
            ack()
            sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(placeAt, world.getBlockState(placeAt.x, placeAt.y, placeAt.z) ?: 0)
                    .write(it)
            }
            // The client took the item out of its own hand when it predicted the placement.
            sendInventory()
            placeEvent.cancelMessage?.let { message -> sendPluginNotice(message) }
            return
        }
        val changed = world.setBlockState(placeAt.x, placeAt.y, placeAt.z, blockStateId)
        ack()
        if (!changed) return
        // parity: `getStateForPlacement` joins a fence, a wall or a staircase to what is already
        // there, and the announcement below has to carry the state it settled on rather than the one
        // it was placed as.
        var settledStateId = blockStateId
        loadedWorld?.let { placedIn ->
            NeighbourUpdates.connect(placedIn, placeAt.x, placeAt.y, placeAt.z, blockStateId)?.let {
                settledStateId = it
            }
            // parity: `BaseRailBlock.onPlace` into `RailState.place`, which bends the new rail and
            // every rail it decided to meet.
            Rails.place(placedIn, placeAt.x, placeAt.y, placeAt.z) { x, y, z, stateId ->
                if (x == placeAt.x && y == placeAt.y && z == placeAt.z) {
                    settledStateId = stateId
                } else {
                    broadcastBlockUpdate(BlockPos(x, y, z), stateId)
                }
            }
            // parity: `RedStoneWireBlock.getStateForPlacement`, which draws the dust as it lands.
            RedstoneWire.reshape(placedIn, placeAt.x, placeAt.y, placeAt.z, settledStateId)?.let {
                settledStateId = it
            }
        }
        second?.let { world.setBlockState(it.x, it.y, it.z, it.stateId) }
        // parity: `AbstractFurnaceBlock.newBlockEntity`: a cooker is nothing without the entity that
        // holds its slots and its fire.
        val placedPath = BlockStateDefinitions.blockOf(blockStateId)?.path
        // parity: `Level.setBlock` gives a block its block entity as it lands. Every block that owns
        // one needs it, not the handful of containers this used to list: a bell without one is a
        // bell whose body the client never draws.
        if (placedBlock.let { BlockEntityTypes.ofBlock(it) } != null) {
            loadedWorld?.state?.chunkIfLoaded(placeAt.x shr 4, placeAt.z shr 4)?.setBlockEntity(
                ChunkBlockEntity(placeAt.x and 15, placeAt.y, placeAt.z and 15, placedBlock, NbtCompound()),
            )
        }
        // parity: `BlockItem.updateCustomBlockEntityTag`: a shulker box placed from an item that
        // carries contents gets them back, which is the other half of breaking one.
        if (Shulkers.isShulkerBox(placedPath)) {
            (held.components[Shulkers.CONTAINER] as? NbtCompound)?.let { carried ->
                loadedWorld?.let { Shulkers.restore(it, placeAt.x, placeAt.y, placeAt.z, carried) }
            }
        }
        // parity: `MossyCarpetBlock.setPlacedBy`, which grows a second carpet over the one placed.
        if (BlockStateDefinitions.blockOf(blockStateId)?.path == "pale_moss_carpet") {
            loadedWorld?.let { placedIn ->
                MossCarpetTopper
                    .above(placedIn, placeAt.x, placeAt.y, placeAt.z) { Random.nextBoolean() }
                    ?.let { topper -> world.setBlockState(placeAt.x, placeAt.y + 1, placeAt.z, topper) }
            }
        }

        if (session.gamemode == Gamemode.SURVIVAL) {
            val remaining = held.count - 1
            session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
            sendInventorySlot(heldSlot)
        }

        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(placeAt, settledStateId).write(it)
            }
        }
        second?.let { broadcastBlockUpdate(BlockPos(it.x, it.y, it.z), it.stateId) }
        // Both the block under it (a snow layer put on grass) and the block itself (grass put
        // under snow) can change how they look.
        refreshSnowUnder(placeAt)
        refreshSnowAt(placeAt)
        notifyNeighbours(placeAt)
        // parity: `GameEvent.BLOCK_PLACE`, thirteen: a block going down is louder than one coming up.
        heardBySculk("block_place", placeAt)
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
        // parity: `ItemCooldowns.isOnCooldown`, which refuses the use outright rather than
        // letting it through and greying the slot afterwards.
        if (onCooldown(item)) return
        if (startEating(session, item, heldSlot)) return
        if (startDrawing(session, item, heldSlot)) return
        if (throwEnderPearl(session, item, heldSlot)) return
        if (!Buckets.isBucket(item)) return

        val bucketing =
            playerEvent(PluginEventTypes.PLAYER_BUCKET) { event ->
                event.set(3, session.x.toInt())
                event.set(4, session.y.toInt())
                event.set(5, session.z.toInt())
                event.set(6, item.toString())
                event.set(7, item.path == "bucket")
            }
        if (bucketing?.cancelled == true) {
            bucketing.cancelMessage?.let { sendPluginNotice(it) }
            sendInventory()
            return
        }
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
        // parity: `BucketItem.emptyContents` calls `setBlock`, so what was around it hears about it:
        // grass that had a snow layer over it until the water took its place stops wearing snow.
        for (block in result.changes) notifyNeighbours(BlockPos(block[0], block[1], block[2]))
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
        val opening = playerEvent(PluginEventTypes.INVENTORY_OPEN) { event -> event.set(3, "crafting") }
        if (opening?.cancelled == true) {
            opening.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
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

    /**
     * Opens a furnace, a blast furnace or a smoker.
     *
     * parity: `AbstractFurnaceBlock.use`, which opens the block's own menu. The three slots are the
     * block's, so nothing is copied into the session: a click reads and writes the block entity.
     */
    private fun openFurnace(
        session: PlayerSession,
        pos: BlockPos,
        cooker: String,
    ) {
        val opening =
            playerEvent(PluginEventTypes.INVENTORY_OPEN) { event ->
                event.set(3, cooker)
                event.set(4, pos.x)
                event.set(5, pos.y)
                event.set(6, pos.z)
            }
        if (opening?.cancelled == true) {
            opening.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.furnace(nextContainerId)
        openBlock = pos
        openBlockSize = FURNACE_SLOTS
        openBlockData = IntArray(4) { -1 }
        val menuType =
            when (cooker) {
                "blast_furnace" -> OpenScreenPacket.BLAST_FURNACE_MENU
                "smoker" -> OpenScreenPacket.SMOKER_MENU
                else -> OpenScreenPacket.FURNACE_MENU
            }
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = menuType,
                title = Component.translatable("container.$cooker"),
            ).write(it)
        }
        sendInventory()
        sendFurnaceBars()
    }

    /**
     * parity: `EnderChestBlock.useWithoutItem`: the same twenty-seven slots wherever the chest is,
     * and nothing at all when something solid sits on top of it, exactly like a chest.
     */
    private fun openEnderChest(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        val world = currentWorld ?: return
        val above = world.state.getBlockState(pos.x, pos.y + 1, pos.z) ?: return
        if (BlockSupport.conductsRedstone(above)) return
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.chest(nextContainerId)
        openBlock = null
        openBlockSecond = null
        openEnderChest = true
        openBlockSize = StoredPlayer.ENDER_CHEST_SLOTS
        openBlockData = IntArray(4) { -1 }
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.CHEST_MENU,
                title = Component.translatable("container.enderchest"),
            ).write(it)
        }
        sendInventory()
        broadcastEffects(
            pos,
            world.state.getBlockState(pos.x, pos.y, pos.z) ?: 0,
            listOf(BlockEffect.Sound("block.ender_chest.open", SoundSource.BLOCKS, 0.5f, 1f)),
        )
    }

    /**
     * What a container's window is called.
     *
     * parity: the name comes from the *block entity*, not the block, so all ten chests are
     * `container.chest` and every shulker box is `container.shulkerBox`. Naming them after the block
     * left a copper chest showing `container.waxed_copper_chest` in raw text.
     */
    private fun containerTitleKey(
        block: String,
        paired: Boolean,
    ): String =
        when {
            Shulkers.isShulkerBox(block) -> "container.shulkerBox"
            paired -> "container.chestDouble"
            block in Containers.CHESTS -> "container.chest"
            else -> "container.$block"
        }

    /**
     * Opens a chest, a trapped chest, a barrel or a shulker box.
     *
     * parity: `ChestBlock.use`, which opens the block's own container. Double chests are not paired
     * yet, so each half opens as its own three rows.
     */
    private fun openChest(
        session: PlayerSession,
        pos: BlockPos,
        block: String,
    ) {
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        // parity: `ChestBlock.getMenuProvider`, which hands both halves over as one container.
        val halves = DoubleChests.halves(currentWorld, pos.x, pos.y, pos.z)
        openEnderChest = false
        openBlockSecond = halves?.second?.let { BlockPos(it[0], it[1], it[2]) }
        openBlock = halves?.first?.let { BlockPos(it[0], it[1], it[2]) } ?: pos
        val paired = openBlockSecond != null
        openMenu = if (paired) Menu.doubleChest(nextContainerId) else Menu.chest(nextContainerId)
        openBlockSize = if (paired) Menu.DOUBLE_CHEST_SIZE else Menu.CHEST_SIZE
        openBlockData = IntArray(4) { -1 }
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = if (paired) OpenScreenPacket.DOUBLE_CHEST_MENU else OpenScreenPacket.CHEST_MENU,
                title = Component.translatable(containerTitleKey(block, paired)),
            ).write(it)
        }
        sendInventory()
        openedAt = pos
        announceOpeners(pos, block, ContainerOpeners.open(session.worldName, pos.x, pos.y, pos.z))
        // parity: `ChestBlock`'s lid is per block entity, so both halves of a double chest open.
        openBlockSecond?.let { other ->
            val at = if (openBlock == pos) other else checkNotNull(openBlock)
            announceOpeners(at, block, ContainerOpeners.open(session.worldName, at.x, at.y, at.z))
        }
    }

    /**
     * What everyone else sees when somebody opens or closes a container: the lid, and the sound.
     *
     * parity: `ContainerOpenersCounter`, which only calls `onOpen` for the first person in and
     * `onClose` for the last one out, plus `ChestBlockEntity.signalOpenCount`'s block event on every
     * change. A barrel has no lid to animate: it carries `open` in its own state instead.
     */
    private fun announceOpeners(
        pos: BlockPos,
        block: String,
        openers: Int,
    ) {
        val world = currentWorld ?: return
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        if (block == "barrel") {
            val opened =
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("barrel"),
                    mapOf("open" to (openers > 0).toString()),
                    stateId,
                )
            if (opened != null && opened != stateId && world.state.setBlockState(pos.x, pos.y, pos.z, opened)) {
                broadcastBlockUpdate(pos, opened)
            }
        } else {
            val blockId = BlockSupport.blockRegistryIdOf(stateId)
            for (player in playersInSameWorld()) {
                player.connection.sendPlayPacket(BlockEventPacket.ID) {
                    BlockEventPacket(pos, b0 = 1, b1 = openers, blockId = blockId).write(it)
                }
            }
        }
        // Only the first in and the last out make a sound, which is why a busy chest is not a rattle.
        val family =
            when {
                block == "barrel" -> "barrel"
                Shulkers.isShulkerBox(block) -> "shulker_box"
                else -> "chest"
            }
        val sound =
            when (openers) {
                1 -> "block.$family.open"
                0 -> "block.$family.close"
                else -> return
            }
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(SoundPacket.ID) {
                SoundPacket(
                    sound = sound,
                    source = SoundSource.BLOCKS,
                    x = pos.x + 0.5,
                    y = pos.y + 0.5,
                    z = pos.z + 0.5,
                    volume = 0.5f,
                    // parity: `ChestBlockEntity.playSound`, a tenth of a tone of randomness.
                    pitch = Random.nextFloat() * 0.1f + 0.9f,
                    seed = Random.nextLong(),
                ).write(it)
            }
        }
    }

    /** Tells everyone who can see it that something happened to an entity. */
    private fun broadcastEntityEvent(
        entityId: Int,
        event: Int,
    ) {
        val packet = EntityEventPacket(entityId, event.toByte())
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(EntityEventPacket.ID) { packet.write(it) }
        }
    }

    /** The red flash of a hit on something that is not a player, and who is blamed for it. */
    private fun broadcastDamageEvent(
        entity: ServerEntity,
        damageType: Identifier,
        causeEntityId: Int,
    ) {
        val registryId = VanillaRegistryData.damageTypes.indexOfFirst { it.first == damageType.path }
        if (registryId < 0) return
        val packet =
            DamageEventPacket(
                entityId = entity.entityId,
                damageType = registryId,
                causeEntityId = causeEntityId,
                directEntityId = causeEntityId,
            )
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        }
    }

    /** How fast an entity is going now, which is the only way a knocked-back mob moves on a client. */
    private fun broadcastMotion(entity: ServerEntity) {
        val packet =
            SetEntityMotionPacket(entity.entityId, entity.velocityX, entity.velocityY, entity.velocityZ)
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(SetEntityMotionPacket.ID) { packet.write(it) }
        }
    }

    /** A sound heard where it happened, by everyone in the world. */
    private fun broadcastSound(
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

    /**
     * What a killed entity leaves behind.
     *
     * parity: `LivingEntity.die`. The dying animation is the client's own half, off the entity
     * event; the loot is rolled by the tick loop, which is what [ServerEntity.killed] is for.
     */
    private fun killEntity(entity: ServerEntity) {
        broadcastSound(EntityCombat.deathSound(entity.type), entity.x, entity.y, entity.z, SoundSource.HOSTILE)
        broadcastEntityEvent(entity.entityId, EntityEventPacket.LIVING_DEATH)
        entity.killed = true
        entity.removed = true
    }

    /**
     * Hitting a minecart.
     *
     * parity: `VehicleEntity.hurtServer` into `destroy`: the cart drops itself as an item, and a
     * creative player's hit discards it without dropping anything at all.
     */
    private fun breakVehicle(
        session: PlayerSession,
        vehicle: ServerEntity,
    ) {
        broadcastDamageEvent(vehicle, DamageTypes.PLAYER_ATTACK, session.entityId)
        if (session.gamemode != Gamemode.CREATIVE) {
            vehicle.vehicleDamage += VEHICLE_DAMAGE_PER_POINT
            if (vehicle.vehicleDamage <= VEHICLE_BREAK_DAMAGE) return
            spawnDroppedItem(vehicle.type, 1, vehicle.x, vehicle.y, vehicle.z)
        }
        for (passenger in vehicle.passengerIds.toList()) {
            playersInSameWorld().firstOrNull { it.entityId == passenger }?.let { it.connection.dismount(it) }
        }
        vehicle.removed = true
    }

    /**
     * Taming an animal, and telling one that is already yours to sit.
     *
     * parity: `Wolf.mobInteract` and `Cat.mobInteract`: the food goes whether it takes or not, a
     * wolf takes one bone in three, and a click with anything else on an animal that is already
     * yours sits it down or stands it up.
     */
    private fun tameOrSit(
        session: PlayerSession,
        entity: ServerEntity,
    ): Boolean {
        if (!Taming.isTamable(entity.type)) return false
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val item = held?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
        val owner = entity.owner
        if (owner != null) {
            if (owner != session.uuid.toString()) return true
            entity.sitting = !entity.sitting
            Navigation.stop(entity)
            broadcastTameFlags(entity)
            return true
        }
        if (item == null || !Taming.tamedBy(entity.type, item)) return false
        // Not through playerEvent: this is an entity event that happens to have somebody doing it,
        // so the entity is first and the player comes after the world.
        val taming =
            pluginEvents.emit(PluginEventTypes.ENTITY_TAME) { event ->
                event.set(0, entity.entityId)
                event.set(1, entity.type.toString())
                event.set(2, currentWorld?.name)
                event.set(3, session.name)
                event.set(4, session.uuid.toString())
            }
        if (taming?.cancelled == true) {
            taming.cancelMessage?.let { sendPluginNotice(it) }
            return true
        }
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
        if (Random.nextInt(Taming.TAME_ODDS) != 0) {
            broadcastEntityEvent(entity.entityId, Taming.EVENT_TAMING_FAILED)
            return true
        }
        entity.owner = session.uuid.toString()
        entity.sitting = true
        entity.targetId = null
        Navigation.stop(entity)
        broadcastEntityEvent(entity.entityId, Taming.EVENT_TAMING_SUCCEEDED)
        broadcastTameFlags(entity)
        return true
    }

    /** parity: `TamableAnimal`'s own flags byte, which is what draws a sitting pet. */
    private fun broadcastTameFlags(entity: ServerEntity) {
        val packet =
            SetEntityDataPacket(
                entity.entityId,
                listOf(SetEntityDataPacket.Entry.Bytes(Taming.FLAGS_INDEX, Taming.flagsOf(entity).toByte())),
            )
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    /** parity: `Entity.startRiding`, which is the passenger list and the packet that carries it. */
    private fun mount(
        session: PlayerSession,
        vehicle: ServerEntity,
    ) {
        if (session.vehicleId != null) return
        val mounting =
            pluginEvents.emit(PluginEventTypes.ENTITY_MOUNT) { event ->
                event.set(0, vehicle.entityId)
                event.set(1, vehicle.type.toString())
                event.set(2, currentWorld?.name)
                event.set(3, session.name)
                event.set(4, session.uuid.toString())
                event.set(5, true)
            }
        if (mounting?.cancelled == true) return
        session.vehicleId = vehicle.entityId
        vehicle.passengerIds += session.entityId
        broadcastPassengers(vehicle)
    }

    /** parity: `Entity.stopRiding`: off the cart, standing where it left them. */
    internal fun dismount(session: PlayerSession) {
        val world = currentWorld ?: return
        val vehicle = session.vehicleId?.let { world.state.entity(it) }
        session.vehicleId = null
        if (vehicle == null) return
        vehicle.passengerIds -= session.entityId
        broadcastPassengers(vehicle)
        // The client puts itself back on the ground, but the server's own copy of where they are
        // came from the cart, so it is told the same thing rather than left a tick behind.
        teleportTo(session.x, session.y + DISMOUNT_LIFT, session.z, session.yaw, session.pitch)
    }

    private fun broadcastPassengers(vehicle: ServerEntity) {
        val packet = SetPassengersPacket(vehicle.entityId, vehicle.passengerIds.toList())
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(SetPassengersPacket.ID) { packet.write(it) }
        }
    }

    /**
     * A player hitting something.
     *
     * parity: `Player.attack`. The damage is the attribute plus whatever is in hand, scaled by how
     * far the swing has recharged (a fifth of it at worst), half again for a critical, and a
     * sprinting hit pushes half as much again.
     */
    private fun handleAttack(attack: AttackPacket) {
        val session = playerSession ?: return
        if (session.dead || session.gamemode == Gamemode.SPECTATOR) return
        val world = currentWorld ?: return
        val target = world.state.entity(attack.entityId) ?: return
        if (target.removed) return
        if (!withinAttackReach(session, target)) return
        // parity: `VehicleEntity.hurtServer`, which is not health at all: a cart takes ten a point
        // and comes apart past forty, or at once for a creative player.
        if (DetectorRails.isMinecart(target.type)) {
            breakVehicle(session, target)
            return
        }
        if (!EntityAttributes.isLiving(target.type)) return

        val weapon = session.inventory.heldItem?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
        val stats = ItemWeapons.of(weapon)
        val strength =
            Combat.attackStrengthScale(
                session.ticksSinceSwing,
                ItemWeapons.BASE_ATTACK_SPEED + (stats?.attackSpeed ?: 0.0),
            )
        session.ticksSinceSwing = 0

        val base = (ItemWeapons.BASE_ATTACK_DAMAGE + (stats?.attackDamage ?: 0.0)).toFloat()
        // parity: what the enchantments add rides on the swing but not on the critical, and is put
        // back on top after the charge and the critical have had the base to themselves.
        val enchanted = WeaponDamage.of(session.inventory.heldItem, base) { enchantments?.definitionOf(it) }
        val boost = strength * (enchanted - base)
        var damage = base * Combat.chargeScale(strength)
        val fullStrength = strength > FULL_STRENGTH
        // parity: `canCriticalAttack`, which is the falling half of a jump and nothing else.
        val critical = fullStrength && fallDistance > 0.0 && !session.onGround && !session.sprinting
        if (critical) damage *= Combat.CRITICAL_MULTIPLIER
        damage += boost
        if (damage <= 0f) return

        val knockbackAttack = session.sprinting && fullStrength
        if (knockbackAttack) {
            broadcastSound(SOUND_ATTACK_KNOCKBACK, session.x, session.y, session.z, SoundSource.PLAYERS)
        }
        val result =
            EntityCombat.hurt(
                target,
                damage,
                session.x,
                session.z,
                events = pluginEvents,
                worldName = currentWorld?.name,
                cause = "attack",
                attackerName = session.name,
            )
        if (!result.landed) {
            broadcastSound(SOUND_ATTACK_NO_DAMAGE, session.x, session.y, session.z, SoundSource.PLAYERS)
            return
        }
        // parity: `causeExtraKnockback`, which is the half a sprinting hit adds and nothing else
        // without enchantments: the 0.4 every hit is worth was already taken inside the hit.
        if (knockbackAttack) {
            EntityCombat.knockback(target, SPRINT_KNOCKBACK, session.x - target.x, session.z - target.z)
        }
        // parity: `isSweepAttack`: a sword, on the ground, walking, and a swing that is neither a
        // critical nor a sprinting one.
        val sweep =
            fullStrength &&
                !critical &&
                !knockbackAttack &&
                session.onGround &&
                lastStepSquared < SWEEP_SPEED_LIMIT * SWEEP_SPEED_LIMIT &&
                weapon?.path?.endsWith(SWORD_SUFFIX) == true
        if (sweep) sweepAround(session, target, world, strength)
        broadcastDamageEvent(target, DamageTypes.PLAYER_ATTACK, session.entityId)
        broadcastMotion(target)
        // parity: `HurtByTargetGoal` and `PanicGoal` both read the last thing that hurt it: one
        // sends a monster after whoever swung, the other sends an animal running.
        target.hurtBy = session.entityId
        // parity: `Warden.hurtServer`, which adds the angry threshold plus twenty to whoever hit it.
        if (Wardens.isWarden(target.type)) Wardens.anger(target, session.entityId, Wardens.HURT)
        target.panicTicks = PANIC_TICKS
        // parity: `OwnerHurtTargetGoal`, which sends a pet after whatever its owner just hit.
        for (pet in world.state.entities()) {
            if (pet.owner == session.uuid.toString()) pet.ownerHurt = target.entityId
        }
        if (critical) {
            broadcastSound(SOUND_ATTACK_CRIT, session.x, session.y, session.z, SoundSource.PLAYERS)
            // parity: `ServerPlayer.crit`, which is an animation on the target rather than particles.
            broadcastAnimation(target.entityId, AnimatePacket.CRITICAL_HIT)
        }
        if (!critical && !sweep) {
            val sound = if (fullStrength) SOUND_ATTACK_STRONG else SOUND_ATTACK_WEAK
            broadcastSound(sound, session.x, session.y, session.z, SoundSource.PLAYERS)
        }
        session.stats.addDamageDealt(result.damage)
        session.food.addExhaustion(ATTACK_EXHAUSTION)
        if (result.died) {
            killEntity(target)
        } else {
            broadcastSound(
                EntityCombat.hurtSound(target.type),
                target.x,
                target.y,
                target.z,
                SoundSource.HOSTILE,
            )
        }
    }

    /**
     * The rest of the swing, for everything else standing around the target.
     *
     * parity: `Player.doSweepAttack`: a point of damage each, the same push, and only what is
     * within a block of the target and three of the player.
     */
    private fun sweepAround(
        session: PlayerSession,
        target: ServerEntity,
        world: LoadedWorld,
        strength: Float,
    ) {
        broadcastSound(SOUND_ATTACK_SWEEP, session.x, session.y, session.z, SoundSource.PLAYERS)
        val damage = SWEEP_DAMAGE * strength
        for (nearby in world.state.entities()) {
            if (nearby.entityId == target.entityId || nearby.removed) continue
            if (!EntityAttributes.isLiving(nearby.type)) continue
            if (abs(nearby.x - target.x) > SWEEP_RANGE + nearby.width / 2.0) continue
            if (abs(nearby.z - target.z) > SWEEP_RANGE + nearby.width / 2.0) continue
            if (nearby.y > target.y + target.height + SWEEP_HEIGHT) continue
            if (nearby.y + nearby.height < target.y - SWEEP_HEIGHT) continue
            val dx = nearby.x - session.x
            val dz = nearby.z - session.z
            if (dx * dx + dz * dz >= SWEEP_PLAYER_RANGE) continue
            val result =
                EntityCombat.hurt(
                    nearby,
                    damage,
                    session.x,
                    session.z,
                    events = pluginEvents,
                    worldName = currentWorld?.name,
                    cause = "sweep",
                    attackerName = session.name,
                )
            if (!result.landed) continue
            broadcastDamageEvent(nearby, DamageTypes.PLAYER_ATTACK, session.entityId)
            broadcastMotion(nearby)
            if (result.died) {
                killEntity(nearby)
            } else {
                nearby.hurtBy = session.entityId
                if (Wardens.isWarden(nearby.type)) Wardens.anger(nearby, session.entityId, Wardens.HURT)
                nearby.panicTicks = PANIC_TICKS
            }
        }
        // parity: the sweep's own particle, a block ahead of where the player is looking.
        val yaw = session.yaw * PI.toFloat() / HALF_CIRCLE
        broadcastParticle(
            PARTICLE_SWEEP,
            session.x - sin(yaw).toDouble(),
            session.y + SWEEP_PARTICLE_HEIGHT,
            session.z + cos(yaw).toDouble(),
            count = 0,
        )
    }

    /** One animation on an entity, for everyone in the world. */
    private fun broadcastAnimation(
        entityId: Int,
        animation: Int,
    ) {
        val packet = AnimatePacket(entityId, animation)
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(AnimatePacket.ID) { packet.write(it) }
        }
    }

    /** One particle effect where it happened, for everyone in the world. */
    private fun broadcastParticle(
        particle: String,
        x: Double,
        y: Double,
        z: Double,
        count: Int,
    ) {
        val id = ParticleTypes[Identifier.vanilla(particle)]?.protocolId ?: return
        val packet = LevelParticlesPacket(particleTypeId = id, x = x, y = y, z = z, count = count)
        for (player in playersInSameWorld()) {
            player.connection.sendPlayPacket(LevelParticlesPacket.ID) { packet.write(it) }
        }
    }

    /**
     * parity: `ServerGamePacketListenerImpl.handleInteract`, which measures three blocks of reach
     * plus three of slack from the eyes to the target's box before it lets anything through.
     */
    private fun withinAttackReach(
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
     * Right-clicking an entity.
     *
     * parity: `Animal.mobInteract`, the half of it this build has: the right food puts an animal in
     * love, and a hand that is not holding it does nothing. What a villager or a boat does with the
     * same click is their own business and waits for them.
     */
    private fun handleInteract(interact: InteractPacket) {
        val session = playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        // An NPC is not in the world's entity table, so it is answered before anything looks there.
        // Only the main hand: an empty-handed right click sends one packet per hand, so answering
        // both is how one click becomes two.
        if (interact.hand == InteractPacket.MAIN_HAND &&
            npcs?.clicked(interact.entityId, session.uuid.toString()) == true
        ) {
            return
        }
        if (npcs?.of(interact.entityId) != null) return
        val world = currentWorld ?: return
        val entity = world.state.entity(interact.entityId) ?: return
        if (entity.removed) return
        val interacted =
            playerEvent(PluginEventTypes.PLAYER_INTERACT_ENTITY) { event ->
                event.set(3, entity.entityId)
                event.set(4, entity.type.toString())
                event.set(5, "interact")
                event.set(6, if (interact.hand == 1) "off" else "main")
            }
        if (interacted?.cancelled == true) {
            interacted.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
        // parity: `Villager.mobInteract` opens its trades whatever is in the player's hand.
        if (entity.type == Villagers.TYPE) {
            openTrades(entity)
            return
        }
        // parity: `AbstractMinecart.interact`, which seats whoever clicked it.
        if (DetectorRails.isMinecart(entity.type)) {
            mount(session, entity)
            return
        }
        if (tameOrSit(session, entity)) return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val item = Identifier.parse(held.id)
        if (!Breeding.isFood(entity.type, item)) return
        if (!Breeding.canFallInLove(entity)) return
        Breeding.feed(entity)
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
    }

    /**
     * A click on a lectern: put the held book on it, or open the one already there.
     *
     * parity: `LecternBlock.tryPlaceBook` and its menu. Turning a page pulses the block for two
     * ticks, which is the whole reason a lectern is a redstone component at all.
     */
    private fun useLectern(
        session: PlayerSession,
        pos: BlockPos,
    ): Boolean {
        val world = currentWorld ?: return false
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return false
        val hasBook = BlockStateDefinitions.propertiesOf(stateId)["has_book"] == "true"
        if (hasBook) {
            openLectern(session, pos)
            return true
        }
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return false
        val item = Identifier.parse(held.id)
        if (!Lecterns.isBook(item)) return false
        val change = Lecterns.placeBook(world, pos.x, pos.y, pos.z, item) ?: return false
        broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
        session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
        sendInventorySlot(heldSlot)
        notifyNeighbours(pos)
        return true
    }

    /** parity: `LecternMenu`, one book slot and the page it is open at. */
    private fun openLectern(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.playerInventory()
        openBlock = pos
        openLecternAt = pos
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = nextContainerId,
                menuType = LecternMenuType.ID,
                title = Component.translatable("block.minecraft.lectern"),
            ).write(it)
        }
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

    /**
     * Opens a window a plugin drew, [rows] rows of nine with [items] in it.
     *
     * Built on the same [Menu] a chest uses, so clicks arrive through the same path and the player's
     * own inventory keeps its usual place at the bottom. What is different is behind the slots:
     * there is no block, the contents are this array, and no click ever moves any of it.
     */
    internal fun openPluginMenu(
        title: String,
        rows: Int,
        items: Map<Int, org.basaltmc.server.plugins.PluginItem>,
        onClick: (Int) -> Unit,
    ) {
        val session = playerSession ?: return
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.rows(nextContainerId, rows)
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        openStonecutter = false
        openSmithing = false
        openBlockSize = rows * Menu.COLUMNS
        openBlockData = IntArray(4) { -1 }
        pluginMenu = arrayOfNulls(rows * Menu.COLUMNS)
        pluginMenuClick = onClick
        fillPluginMenu(items)
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                // `minecraft:generic_9x1` through `9x6` are consecutive from zero.
                menuType = rows - 1,
                title = MiniMessage.parse(title),
            ).write(it)
        }
        sendInventory()
    }

    /**
     * Shuts whatever window is open, telling the client too.
     *
     * The client is told because this closing did not start there: a plugin decided, or was
     * unloaded, and a window nobody takes down stays on the screen doing nothing.
     */
    internal fun closeOpenMenu() {
        val session = playerSession ?: return
        if (openMenu.containerId == 0) return
        sendPlayPacket(ContainerClosePacket.CLIENTBOUND_ID) {
            ContainerClosePacket(openMenu.containerId).write(it)
        }
        closeOpenMenu(session)
    }

    /** Puts new contents in the open menu and redraws it. */
    internal fun refreshPluginMenu(items: Map<Int, org.basaltmc.server.plugins.PluginItem>) {
        if (pluginMenu == null) return
        fillPluginMenu(items)
        sendInventory()
    }

    private fun fillPluginMenu(items: Map<Int, org.basaltmc.server.plugins.PluginItem>) {
        val slots = pluginMenu ?: return
        slots.fill(null)
        for ((slot, item) in items) {
            if (slot !in slots.indices) continue
            slots[slot] = pluginMenuStack(item)
        }
    }

    private fun pluginMenuStack(item: org.basaltmc.server.plugins.PluginItem): StoredItemStack {
        val components = NbtCompound()
        // The name is MiniMessage on the plugin's side and a component on the wire, so it is parsed
        // here rather than stored as the tags a client would show literally.
        item.name?.let { components[ItemComponentCodecs.CUSTOM_NAME] = ComponentNbt.serialize(MiniMessage.parse(it)) }
        return StoredItemStack(item.id, item.count.coerceAtLeast(1), components)
    }

    /**
     * parity: `SmithingTableBlock.useWithoutItem` over `SmithingMenu`: a template, a base and an
     * addition, and whatever the three of them make. Like the stonecutter, the slots are the
     * screen's own, so what is left in them comes back when it closes.
     */
    private fun openSmithingMenu(session: PlayerSession) {
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.smithing(nextContainerId)
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        openStonecutter = false
        openSmithing = true
        smithingSlots.fill(null)
        openBlockSize = smithingSlots.size
        openBlockData = IntArray(4) { -1 }
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.SMITHING_MENU,
                title = Component.translatable("container.upgrade"),
            ).write(it)
        }
        sendInventory()
    }

    /** parity: `SmithingMenu.createResult`, run whenever any of the three inputs changes. */
    private fun recomputeSmithingResult() {
        val recipes = recipes
        val template = smithingSlots[Menu.SMITHING_TEMPLATE]?.let(::craftingStackOf)
        val base = smithingSlots[Menu.SMITHING_BASE]?.let(::craftingStackOf)
        val addition = smithingSlots[Menu.SMITHING_ADDITION]?.let(::craftingStackOf)
        val recipe = recipes?.smithingFor(template, base, addition)
        smithingSlots[Menu.SMITHING_RESULT] =
            if (recipe == null || base == null || addition == null) {
                null
            } else {
                recipe.assemble(base, addition)?.let { StoredItemStack(it.item.toString(), it.count, it.components) }
            }
    }

    /** parity: `SmithingMenu.onTake`, which eats one of each of the three inputs. */
    private fun takeSmithingResult() {
        for (slot in listOf(Menu.SMITHING_TEMPLATE, Menu.SMITHING_BASE, Menu.SMITHING_ADDITION)) {
            val stack = smithingSlots[slot] ?: continue
            smithingSlots[slot] = if (stack.count <= 1) null else stack.copy(count = stack.count - 1)
        }
        recomputeSmithingResult()
    }

    /** A stored stack as the crafting side sees it, components and all. */
    private fun craftingStackOf(stack: StoredItemStack): org.basaltmc.server.world.loot.ItemStack =
        org.basaltmc.server.world.loot
            .ItemStack(Identifier.parse(stack.id), stack.count, stack.components)

    /**
     * parity: `StonecutterBlock.useWithoutItem` over `StonecutterMenu`.
     *
     * Its two slots are the menu's own, not a block's: whatever is left in the input when the screen
     * closes comes back to the player, exactly as vanilla's `removed` does.
     */
    private fun openStonecutterMenu(session: PlayerSession) {
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.stonecutter(nextContainerId)
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        openStonecutter = true
        stonecutterInput = null
        stonecutterResult = null
        openBlockSize = 2
        openBlockData = IntArray(4) { -1 }
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.STONECUTTER_MENU,
                title = Component.translatable("container.stonecutter"),
            ).write(it)
        }
        sendInventory()
    }

    /** parity: `StonecutterMenu.clickMenuButton` into `setupResultSlot`. */
    private fun selectStonecutterRecipe(
        session: PlayerSession,
        button: Int,
    ) {
        val input = stonecutterInput
        val recipes = recipes
        if (input == null || recipes == null) {
            stonecutterResult = null
            sendInventory()
            return
        }
        val options =
            recipes.stonecuttingFor(
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.parse(input.id), input.count),
            )
        val chosen = options.getOrNull(button)
        stonecutterResult = chosen?.let { StoredItemStack(it.result.item.toString(), it.result.count) }
        sendInventory()
    }

    /**
     * parity: `StonecutterMenu.ResultSlot.onTake`: taking the result eats one of the input and
     * leaves the same button selected, so a stack can be cut one at a time without clicking again.
     */
    private fun takeStonecutterResult(session: PlayerSession) {
        val input = stonecutterInput ?: return
        stonecutterInput = if (input.count <= 1) null else input.copy(count = input.count - 1)
        if (stonecutterInput == null) {
            stonecutterResult = null
        }
    }

    /**
     * A button inside an open window: the lectern's pages, or the stonecutter's list.
     *
     * parity: `LecternMenu.clickMenuButton` (one back, two forward, three to take the book, and a
     * hundred plus the page to jump to it) and `StonecutterMenu.clickMenuButton`, whose button is an
     * index into the recipes that fit the input.
     */
    private fun clickContainerButton(button: Int) {
        val session = playerSession ?: return
        val world = currentWorld ?: return
        // parity: `StonecutterMenu.clickMenuButton`, whose button is an index into the recipes that
        // fit whatever is in the input slot, in the order the client was sent them.
        if (openStonecutter) {
            selectStonecutterRecipe(session, button)
            return
        }
        val pos = openLecternAt ?: return
        val page = lecternPage
        val wanted =
            when {
                button >= ContainerButtonClickPacket.LECTERN_JUMP_TO_PAGE ->
                    button - ContainerButtonClickPacket.LECTERN_JUMP_TO_PAGE
                button == ContainerButtonClickPacket.LECTERN_PREVIOUS_PAGE -> page - 1
                button == ContainerButtonClickPacket.LECTERN_NEXT_PAGE -> page + 1
                button == ContainerButtonClickPacket.LECTERN_TAKE_BOOK -> {
                    val taken = Lecterns.takeBook(world, pos.x, pos.y, pos.z) ?: return
                    val (change, book) = taken
                    broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
                    val left = session.inventory.add(StoredItemStack(book.toString(), 1))
                    if (left > 0) spawnDroppedItem(book, left, session.x, session.y, session.z)
                    sendInventory()
                    notifyNeighbours(pos)
                    openLecternAt = null
                    return
                }
                else -> return
            }
        if (wanted < 0) return
        lecternPage = wanted
        val change = Lecterns.turnPage(world, pos.x, pos.y, pos.z, wanted) ?: return
        broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
        notifyNeighbours(pos)
    }

    /**
     * A click on a jukebox: put the held disc in, or take out what is playing.
     *
     * parity: `JukeboxBlock`, whose whole interaction is those two. What a comparator then reads is
     * the song's own number, and while it plays the block itself gives off a full signal.
     */
    private fun useJukebox(
        session: PlayerSession,
        pos: BlockPos,
    ): Boolean {
        val world = currentWorld ?: return false
        val ejected = Jukeboxes.eject(world, pos.x, pos.y, pos.z)
        if (ejected != null) {
            val (change, disc) = ejected
            broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
            spawnDroppedItem(disc, 1, pos.x + 0.5, pos.y + 1.0, pos.z + 0.5)
            notifyNeighbours(pos)
            return true
        }
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return false
        val item = Identifier.parse(held.id)
        if (!Jukeboxes.isDisc(item)) return false
        val song = Jukeboxes.songOf(item)
        val change =
            Jukeboxes.insert(world, pos.x, pos.y, pos.z, item, jukeboxes?.comparatorOutputOf(song) ?: 0)
                ?: return false
        broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
        notifyNeighbours(pos)
        return true
    }

    /**
     * Opens a villager's trades.
     *
     * parity: `Villager.mobInteract` over `MerchantMenu`: the offers go out as they are, and what
     * the player then picks is a `select_trade`. The offers themselves come out of the jar's own
     * data, so a farmer offers what a farmer offers.
     */
    private fun openTrades(villager: org.basaltmc.server.runtime.entity.ServerEntity) {
        val session = playerSession ?: return
        val trades = villagerTrades ?: return
        val offers = Villagers.offersFor(trades, villager.profession, villager.villagerLevel)
        if (offers.isEmpty()) return
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.merchant(nextContainerId)
        openBlock = null
        openBlockSecond = null
        openTradingWith = villager.entityId
        openOffers = offers
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.MERCHANT_MENU,
                title = Component.translatable("entity.minecraft.villager"),
            ).write(it)
        }
        sendOffers()
        sendInventory()
    }

    /** parity: `ClientboundMerchantOffersPacket`, sent when the window opens and after every trade. */
    private fun sendOffers() {
        val offers = openOffers ?: return
        val villager = openTradingWith?.let { currentWorld?.state?.entity(it) } ?: return
        val wire =
            offers.mapNotNull { offer ->
                val costA = Items.REGISTRY.byIdentifier(offer.costA.item)?.protocolId ?: return@mapNotNull null
                val result = Items.REGISTRY.byIdentifier(offer.result.item)?.protocolId ?: return@mapNotNull null
                val costB =
                    offer.costB?.let { second ->
                        Items.REGISTRY.byIdentifier(second.item)?.protocolId?.let { id ->
                            MerchantCost(id, Trades.priceOf(offer, second))
                        }
                    }
                MerchantOffer(
                    costA = MerchantCost(costA, Trades.priceOf(offer, offer.costA)),
                    result = WireItemStack(result, offer.result.count),
                    costB = costB,
                    outOfStock = offer.outOfStock,
                    uses = offer.uses,
                    maxUses = offer.maxUses,
                    experience = offer.experience,
                    specialPrice = offer.specialPrice,
                    priceMultiplier = offer.priceMultiplier,
                    demand = offer.demand,
                )
            }
        sendPlayPacket(MerchantOffersPacket.ID) {
            MerchantOffersPacket(
                containerId = openMenu.containerId,
                offers = wire,
                level = villager.villagerLevel,
                experience = villager.villagerExperience,
                showProgress = true,
                canRestock = true,
            ).write(it)
        }
    }

    /**
     * Takes the trade the player picked.
     *
     * parity: `MerchantMenu.clickMenuButton`: the offer is checked against what they are carrying,
     * the cost comes out of the inventory, the result goes in, and the offer's own use is counted.
     */
    private fun selectTrade(index: Int) {
        val trading =
            playerEvent(PluginEventTypes.INVENTORY_TRADE) { event ->
                event.set(3, openMenu.containerId)
                event.set(4, index)
                event.set(
                    5,
                    openOffers
                        ?.getOrNull(index)
                        ?.result
                        ?.item
                        ?.toString(),
                )
            }
        if (trading?.cancelled == true) {
            trading.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
        val session = playerSession ?: return
        val offers = openOffers ?: return
        val offer = offers.getOrNull(index) ?: return
        if (offer.outOfStock) return
        val (costA, costB) = Trades.currentCost(offer)
        if (session.inventory.countOf(costA.item.toString()) < costA.count) return
        if (costB != null && session.inventory.countOf(costB.item.toString()) < costB.count) return

        session.inventory.take(costA.item.toString(), costA.count)
        costB?.let { session.inventory.take(it.item.toString(), it.count) }
        val given =
            session.inventory.add(StoredItemStack(offer.result.item.toString(), offer.result.count))
        if (given > 0) {
            spawnDroppedItem(offer.result.item, given, session.x, session.y, session.z)
        }
        Trades.take(offer)
        val villager = openTradingWith?.let { currentWorld?.state?.entity(it) }
        villager?.let { it.villagerExperience += offer.experience }
        sendInventory()
        sendOffers()
    }

    /**
     * parity: `MinecartItem.useOn`: a cart only goes down on a rail, a sixteenth of a block above
     * it, and half a block higher again when the rail is a slope.
     */
    private fun placeMinecart(
        session: PlayerSession,
        item: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ): Boolean {
        val world = currentWorld ?: return false
        val rail = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return false
        if (!Rails.isRail(rail)) return false
        val shape = BlockStateDefinitions.propertiesOf(rail)["shape"] ?: return false
        val slope = if (shape.startsWith("ascending")) 0.5 else 0.0
        val type =
            if (item.path ==
                "minecart"
            ) {
                item
            } else {
                Identifier.vanilla(item.path.removeSuffix("_minecart") + "_minecart")
            }
        return spawnFromItem(
            session,
            type,
            heldSlot,
            held,
            hit.pos.x + 0.5,
            hit.pos.y + minecartRestHeight + slope,
            hit.pos.z + 0.5,
        )
    }

    /** parity: `BoatItem.use`, which drops the boat on what the player is looking at. */
    private fun placeBoat(
        session: PlayerSession,
        item: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ): Boolean {
        // parity: `getBoat` puts it at the hit itself, which is the face of the block clicked.
        val at = hit.pos.offset(hit.direction)
        return spawnFromItem(session, boatEntity(item), heldSlot, held, at.x + 0.5, at.y.toDouble(), at.z + 0.5)
    }

    /** parity: the boat items, whose entity is the item without its `_boat`/`_chest_boat` tail. */
    private fun isBoat(item: Identifier): Boolean =
        item.path.endsWith("_boat") || item.path == "bamboo_raft" || item.path == "bamboo_chest_raft"

    private fun boatEntity(item: Identifier): Identifier = item

    private fun spawnFromItem(
        session: PlayerSession,
        type: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val world = currentWorld ?: return false
        val entityType = EntityTypes[type] ?: return false
        val entityId = playerRegistry?.allocateEntityId() ?: return false
        world.state.spawnEntity(
            ServerEntity(
                entityId = entityId,
                uuid = Uuid.random(),
                type = type,
                protocolType = entityType.protocolId,
                x = x,
                y = y,
                z = z,
            ),
        )
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
        return true
    }

    /**
     * parity: `SpawnEggItem.useOn`: the mob lands on the face that was clicked, unless the block
     * itself has no collision, in which case it lands inside it.
     */
    private fun useSpawnEgg(
        session: PlayerSession,
        egg: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ): Boolean {
        val world = currentWorld ?: return false
        val type = Identifier.vanilla(egg.path.removeSuffix("_spawn_egg"))
        val entityType = EntityTypes[type] ?: return false
        val clicked = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return false
        val at =
            if (BlockShapes.isEmptyShape(clicked)) hit.pos else hit.pos.offset(hit.direction)
        val entityId = playerRegistry?.allocateEntityId() ?: return false
        world.state.spawnEntity(
            ServerEntity(
                entityId = entityId,
                uuid = Uuid.random(),
                type = type,
                protocolType = entityType.protocolId,
                x = at.x + 0.5,
                // parity: `EntityType.spawn` puts it on top of the block, not inside it.
                y = at.y.toDouble(),
                z = at.z + 0.5,
            ),
        )
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
        return true
    }

    /**
     * parity: `ChiseledBookShelfBlock.useItemOn` and `useWithoutItem`: a book goes into the slot the
     * click landed on, and an empty hand takes back whatever is in that one.
     */
    private fun useBookshelf(
        session: PlayerSession,
        hit: BlockHitResult,
    ) {
        val world = currentWorld ?: return
        val stateId = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return
        val slot =
            ChiseledBookshelves.slotAt(stateId, hit.direction.name.lowercase(), hit.clickX, hit.clickY, hit.clickZ)
                ?: return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val book = held?.let { Identifier.parse(it.id) }
        val isBook = book != null && itemTags?.contains("minecraft:bookshelf_books", book) == true
        val interaction =
            if (isBook) {
                ChiseledBookshelves.putBook(world, hit.pos.x, hit.pos.y, hit.pos.z, slot, checkNotNull(book))
            } else {
                ChiseledBookshelves.takeBook(world, hit.pos.x, hit.pos.y, hit.pos.z, slot)
            } ?: return
        broadcastBlockUpdate(hit.pos, interaction.change.stateId)
        broadcastEffects(hit.pos, interaction.change.stateId, interaction.change.effects)
        val taken = interaction.taken
        if (taken != null) {
            giveOrDrop(session, StoredItemStack(taken.toString(), 1), hit.pos)
        } else if (held != null && session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
    }

    /**
     * parity: `ShelfBlock.useItemOn`: unpowered it swaps the held item with the slot that was
     * clicked, and powered it swaps the whole hotbar against the shelves in the chain.
     */
    private fun useShelf(
        session: PlayerSession,
        hit: BlockHitResult,
    ) {
        val world = currentWorld ?: return
        val stateId = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return
        val slot = Shelves.slotAt(stateId, hit.direction.name.lowercase(), hit.clickX, hit.clickY, hit.clickZ) ?: return
        if (BlockStateDefinitions.propertiesOf(stateId)["powered"] == "true") {
            swapHotbarWithShelf(session, hit.pos, stateId)
            return
        }
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val placed =
            held?.let {
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.parse(it.id), it.count)
            }
        val result = Shelves.swap(world, hit.pos.x, hit.pos.y, hit.pos.z, slot, placed) ?: return
        session.inventory[heldSlot] =
            result.removed?.let { StoredItemStack(it.item.toString(), it.count) }
        sendInventorySlot(heldSlot)
        broadcastEffects(hit.pos, stateId, result.effects)
    }

    /** parity: `ShelfBlock.swapHotbar`, nine slots against the chain's nine. */
    private fun swapHotbarWithShelf(
        session: PlayerSession,
        pos: BlockPos,
        stateId: Int,
    ) {
        val world = currentWorld ?: return
        val hotbar =
            Array(PlayerInventory.HOTBAR_SIZE) { index ->
                session.inventory[PlayerInventory.HOTBAR_START + index]?.let {
                    org.basaltmc.server.world.loot
                        .ItemStack(Identifier.parse(it.id), it.count)
                }
            }
        if (!Shelves.swapHotbar(world, pos.x, pos.y, pos.z, hotbar)) return
        for (index in hotbar.indices) {
            session.inventory[PlayerInventory.HOTBAR_START + index] =
                hotbar[index]?.let { StoredItemStack(it.item.toString(), it.count) }
        }
        sendInventoryAll()
        broadcastEffects(
            pos,
            stateId,
            listOf(BlockEffect.Sound("block.shelf.multi_swap", SoundSource.BLOCKS, 1f, 1f)),
        )
    }

    /** Into the player's inventory if it fits, and onto the floor if it does not. */
    private fun giveOrDrop(
        session: PlayerSession,
        stack: StoredItemStack,
        pos: BlockPos,
    ) {
        val leftover = session.inventory.add(stack)
        sendInventoryAll()
        if (leftover <= 0) return
        spawnDroppedItem(
            item = Identifier.parse(stack.id),
            count = leftover,
            x = pos.x + 0.5,
            y = pos.y + 0.5,
            z = pos.z + 0.5,
        )
    }

    /**
     * parity: `DecoratedPotBlock.useItemOn` and `useWithoutItem`. An empty hand, a full pot or the
     * wrong item all end the same way: the pot rocks and takes nothing.
     */
    private fun usePot(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        val world = currentWorld ?: return
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val inserted =
            held?.let { DecoratedPots.insert(world, pos.x, pos.y, pos.z, Identifier.parse(it.id)) }
        if (inserted == null) {
            broadcastEffects(pos, stateId, DecoratedPots.rebuff())
            return
        }
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
        broadcastEffects(pos, stateId, inserted)
    }

    /**
     * Puts what the player is holding onto a campfire.
     *
     * parity: `CampfireBlock.useItemOn`: one piece into the first free slot, at the recipe's own
     * cooking time, and nothing at all if the campfire cannot cook it.
     */
    private fun placeOnCampfire(
        session: PlayerSession,
        pos: BlockPos,
    ): Boolean {
        val world = currentWorld ?: return false
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return false
        if (!Campfires.isLitCampfire(stateId)) return false
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return false
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return false
        val entity =
            chunk.blockEntityAt(pos.x and 15, pos.y, pos.z and 15)
                ?: ChunkBlockEntity(pos.x and 15, pos.y, pos.z and 15, Campfires.BLOCK, NbtCompound())
                    .also { chunk.setBlockEntity(it) }
        val state = Campfires.read(entity.data)
        val stack =
            org.basaltmc.server.world.loot
                .ItemStack(Identifier.parse(held.id), 1)
        if (!Campfires.placeFood(state, stack) { food -> recipes?.cookingFor(campfireCooker, food) }) return false
        Campfires.write(state, entity.data)
        chunk.markDirty()
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            sendInventorySlot(heldSlot)
        }
        return true
    }

    /**
     * Opens a crafter.
     *
     * parity: `CrafterBlock.useWithoutItem`. Its nine slots are the block's own, like a chest's, and
     * nothing is taken out by hand: the block pushes what it makes out of its front.
     */
    private fun openCrafter(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        returnCraftingGrid(session)
        nextContainerId = if (nextContainerId >= MAX_CONTAINER_ID) FIRST_BLOCK_CONTAINER_ID else nextContainerId + 1
        openMenu = Menu.crafter(nextContainerId)
        openBlock = pos
        openBlockSize = Menu.CRAFTER_SIZE
        openBlockData = IntArray(4) { -1 }
        sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.CRAFTER_MENU,
                title = Component.translatable("container.crafter"),
            ).write(it)
        }
        sendInventory()
    }

    /** The block's own slots, read straight off its block entity. */
    private fun readBlockSlot(index: Int): StoredItemStack? {
        if (openSmithing) return smithingSlots.getOrNull(index)
        if (openStonecutter) {
            return if (index == Menu.STONECUTTER_INPUT) stonecutterInput else stonecutterResult
        }
        pluginMenu?.let { return it.getOrNull(index) }
        if (openEnderChest) return playerSession?.enderChest?.getOrNull(index)
        val (position, local) = halfFor(index) ?: return null
        val entity = blockEntityAt(position) ?: return null
        val stack = BlockInventory.read(entity.data, halfSize()).getOrNull(local) ?: return null
        return StoredItemStack(stack.item.toString(), stack.count)
    }

    /**
     * Which block a window slot belongs to, and its index within that block.
     *
     * A double chest is two containers of 27 behind one window of 54, so everything past the first
     * half lands in the second block. Anything else is its own block and its own numbering.
     */
    private fun halfFor(index: Int): Pair<BlockPos, Int>? {
        val second = openBlockSecond
        if (second != null && index >= Menu.CHEST_SIZE) return second to (index - Menu.CHEST_SIZE)
        return (openBlock ?: return null) to index
    }

    /** How many slots the block behind a window slot has of its own. */
    private fun halfSize(): Int = if (openBlockSecond != null) Menu.CHEST_SIZE else openBlockSize

    private fun blockEntityAt(position: BlockPos): org.basaltmc.server.world.ChunkBlockEntity? {
        val world = currentWorld ?: return null
        val chunk = world.state.chunkIfLoaded(position.x shr 4, position.z shr 4) ?: return null
        return chunk.blockEntityAt(position.x and 15, position.y, position.z and 15)
    }

    private fun openBlockEntity(): org.basaltmc.server.world.ChunkBlockEntity? = blockEntityAt(openBlock ?: return null)

    private fun writeBlockSlot(
        index: Int,
        stack: StoredItemStack?,
    ) {
        if (openSmithing) {
            if (index !in smithingSlots.indices) return
            if (index == Menu.SMITHING_RESULT) {
                // Taking the result is the only way this slot is written, and it costs the inputs.
                if (stack == null) takeSmithingResult()
                return
            }
            smithingSlots[index] = stack
            recomputeSmithingResult()
            return
        }
        if (openStonecutter) {
            val session = playerSession ?: return
            if (index == Menu.STONECUTTER_INPUT) {
                stonecutterInput = stack
                // parity: `slotsChanged`, which clears the choice whenever the input changes.
                stonecutterResult = null
            } else if (stack == null) {
                takeStonecutterResult(session)
            }
            return
        }
        if (openEnderChest) {
            val slots = playerSession?.enderChest ?: return
            if (index in slots.indices) slots[index] = stack
            return
        }
        val (position, local) = halfFor(index) ?: return
        val world = currentWorld ?: return
        val chunk = world.state.chunkIfLoaded(position.x shr 4, position.z shr 4) ?: return
        // A block that never got its block entity (one from worldgen, or one written by a command)
        // used to swallow whatever was put into it: the cursor was already empty and the write had
        // nowhere to land. It gets one here instead.
        val entity =
            chunk.blockEntityAt(position.x and 15, position.y, position.z and 15)
                ?: BlockStateDefinitions
                    .blockOf(world.state.getBlockState(position.x, position.y, position.z) ?: return)
                    ?.let {
                        ChunkBlockEntity(position.x and 15, position.y, position.z and 15, it, NbtCompound())
                            .also { entity -> chunk.setBlockEntity(entity) }
                    } ?: return
        // Every slot of the block, not the three a cooker has: writing back fewer than it holds is
        // what would empty a chest the moment somebody moved its first item.
        val slots = BlockInventory.read(entity.data, halfSize())
        if (local !in slots.indices) {
            logger.warn { "dropped a write to slot $local of a container that has ${slots.size}" }
            return
        }
        slots[local] =
            stack?.let {
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.parse(it.id), it.count)
            }
        BlockInventory.write(entity.data, slots)
        // parity: `AbstractFurnaceBlockEntity.setItem`, which recomputes the cook time when the
        // thing being cooked changes: a new item does not inherit the last one's progress.
        val cooking = BlockStateDefinitions.blockOf(world.state.getBlockState(position.x, position.y, position.z) ?: 0)
        if (local == Furnaces.INPUT_SLOT && cooking?.path in Furnaces.COOKERS) {
            val state = Furnaces.read(entity.data)
            state.cookingTimer = 0
            state.cookingTotalTime = Furnaces.cookTimeFor(state) { source -> cookingFor(position, source) }
            Furnaces.write(state, entity.data)
        }
        chunk.markDirty()
    }

    private fun furnaceState(): Furnaces.State? {
        val position = openBlock ?: return null
        val world = currentWorld ?: return null
        val chunk = world.state.chunkIfLoaded(position.x shr 4, position.z shr 4) ?: return null
        val entity = chunk.blockEntityAt(position.x and 15, position.y, position.z and 15) ?: return null
        return Furnaces.read(entity.data)
    }

    private fun cookingFor(
        position: BlockPos,
        stack: org.basaltmc.server.world.loot.ItemStack,
    ): org.basaltmc.server.world.crafting.Recipe.Cooking? {
        val world = currentWorld ?: return null
        val block = BlockStateDefinitions.blockOf(world.state.getBlockState(position.x, position.y, position.z) ?: 0)
        val cooker = Furnaces.COOKERS[block?.path] ?: return null
        return recipes?.cookingFor(cooker, stack)
    }

    /** parity: `RecipeType.CAMPFIRE_COOKING`, the only cooker that is not a block with a window. */
    private val campfireCooker = org.basaltmc.server.world.crafting.Recipe.Cooker.CAMPFIRE

    /** Keeps an open cooker's window in step: its bars, and its slots as the fire eats them. */
    private fun syncOpenContainer() {
        sendFurnaceBars()
        val session = playerSession ?: return
        val state = furnaceState() ?: return
        for (index in state.items.indices) {
            val menuSlot = openMenu.menuSlotOf(MenuSlot.Block(index)) ?: continue
            val shown = shownBlockSlots.getOrNull(index)
            val now = state.items[index]?.let { StoredItemStack(it.item.toString(), it.count) }
            if (shown == now) continue
            if (index < shownBlockSlots.size) shownBlockSlots[index] = now
            sendPlayPacket(ContainerSetSlotPacket.ID) {
                ContainerSetSlotPacket(
                    containerId = openMenu.containerId,
                    stateId = ++containerStateId,
                    slot = menuSlot,
                    item = session.inventory.toWire(now),
                ).write(it)
            }
        }
    }

    /**
     * The two bars of an open cooker, sent only when a number moves.
     *
     * parity: `AbstractContainerMenu.broadcastChanges`, which does the same for its data slots.
     */
    public fun sendFurnaceBars() {
        val state = furnaceState() ?: return
        val values =
            intArrayOf(state.litTimeRemaining, state.litTotalTime, state.cookingTimer, state.cookingTotalTime)
        for (index in values.indices) {
            if (values[index] == openBlockData[index]) continue
            openBlockData[index] = values[index]
            val value = values[index]
            sendPlayPacket(ContainerSetDataPacket.ID) {
                ContainerSetDataPacket(openMenu.containerId, index, value).write(it)
            }
        }
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
        val closing = openedAt
        val closingSecond = openBlockSecond?.let { other -> if (openBlock == closing) other else openBlock }
        val closingBlock = closing?.let { openContainerBlock(it) }
        openMenu = Menu.playerInventory()
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        pluginMenu = null
        pluginMenuClick = null
        // parity: `SmithingMenu.removed`, which hands the three inputs back.
        if (openSmithing) {
            for (slot in listOf(Menu.SMITHING_TEMPLATE, Menu.SMITHING_BASE, Menu.SMITHING_ADDITION)) {
                smithingSlots[slot]?.let { leftover ->
                    val left = session.inventory.add(leftover)
                    if (left > 0) dropStack(session, leftover.copy(count = left))
                }
            }
            smithingSlots.fill(null)
            openSmithing = false
        }
        // parity: `StonecutterMenu.removed`, which hands whatever was in the input back.
        if (openStonecutter) {
            stonecutterInput?.let { leftover ->
                val left = session.inventory.add(leftover)
                if (left > 0) dropStack(session, leftover.copy(count = left))
            }
            stonecutterInput = null
            stonecutterResult = null
            openStonecutter = false
        }
        if (closing != null && closingBlock != null) {
            announceOpeners(
                closing,
                closingBlock,
                ContainerOpeners.close(session.worldName, closing.x, closing.y, closing.z),
            )
            closingSecond?.let { other ->
                announceOpeners(
                    other,
                    closingBlock,
                    ContainerOpeners.close(session.worldName, other.x, other.y, other.z),
                )
            }
        }
        openedAt = null
        recomputeCraftingResult()
        sendInventory()
    }

    /** Which container is open, by name, so closing one knows what lid it is shutting. */
    private fun openContainerBlock(pos: BlockPos): String? {
        val world = currentWorld ?: return null
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        return path.takeIf { it in Containers.PLAIN }
    }

    /** Reads one slot of the open window, whatever it refers to. */
    private fun readSlot(
        session: PlayerSession,
        slot: MenuSlot,
    ): StoredItemStack? =
        when (slot) {
            is MenuSlot.Inventory -> session.inventory[slot.index]
            is MenuSlot.Grid -> craftingGrid.getOrNull(slot.index)
            is MenuSlot.Block -> readBlockSlot(slot.index)
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
            is MenuSlot.Block -> writeBlockSlot(slot.index, stack)
            MenuSlot.Result -> Unit
        }
    }

    /**
     * Whether a stack may be put into this slot at all.
     *
     * parity: `FurnaceResultSlot`, which takes nothing: the result slot of a window only ever gives,
     * and so does the cooked one.
     */
    private fun canPlaceInto(slot: MenuSlot): Boolean =
        when {
            slot == MenuSlot.Result -> false
            // parity: `FurnaceResultSlot.mayPlace`, which is a *furnace's* third slot and nobody
            // else's: a chest's third slot took nothing until this stopped being unconditional.
            slot is MenuSlot.Block && openBlockSize == FURNACE_SLOTS && slot.index == Furnaces.RESULT_SLOT -> false
            // parity: `StonecutterMenu.ResultSlot.mayPlace`, which is always false.
            slot is MenuSlot.Block && openStonecutter && slot.index == Menu.STONECUTTER_RESULT -> false
            // parity: `SmithingMenu`'s result slot, the same.
            slot is MenuSlot.Block && openSmithing && slot.index == Menu.SMITHING_RESULT -> false
            else -> true
        }

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
                // The components go into the grid too: a dye, a copy or a repair is defined by what
                // it does to them, so a recipe that cannot see them cannot exist.
                craftingGrid.getOrNull(index)?.let {
                    LootItemStack(Identifier.parse(it.id), it.count, it.components)
                }
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
                ?.let { StoredItemStack(it.item.toString(), it.count, it.components) }
    }

    /** What each grid cell is left holding, from the recipe that is about to be taken. */
    private fun craftingRemainders(): List<StoredItemStack?> {
        val recipes = recipes ?: return emptyList()
        val width = openMenu.gridWidth
        val height = openMenu.gridHeight
        val cells =
            List(width * height) { index ->
                craftingGrid.getOrNull(index)?.let {
                    LootItemStack(Identifier.parse(it.id), it.count, it.components)
                }
            }
        val input = CraftingInput.of(width, height, cells)
        val recipe = recipes.recipeMatching(input)?.second ?: return emptyList()
        // The grid is trimmed to its occupied box, so the recipe answers in that box's order: walk
        // the same cells in the same order to line the two up.
        val answered = recipe.remainingItems(input).iterator()
        return List(width * height) { index ->
            if (craftingGrid.getOrNull(index) == null) {
                null
            } else if (answered.hasNext()) {
                answered.next()?.let { StoredItemStack(it.item.toString(), it.count, it.components) }
            } else {
                null
            }
        }
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
        // parity: `Recipe.getRemainingItems`: a cell whose item leaves something behind (a bucket, a
        // bottle) keeps it instead of emptying, and the book being copied stays where it was.
        val remaining = craftingRemainders()
        for (index in 0 until openMenu.gridSize) {
            val cell = craftingGrid.getOrNull(index) ?: continue
            val left = cell.count - 1
            val leftover = remaining.getOrNull(index)
            craftingGrid[index] =
                when {
                    left > 0 -> cell.copy(count = left)
                    leftover != null -> leftover
                    else -> null
                }
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
        val clicked =
            playerEvent(PluginEventTypes.INVENTORY_CLICK) { event ->
                event.set(3, packet.slot)
                event.set(4, packet.button)
                event.set(5, packet.mode.name.lowercase())
                // What is in the slot the click is about, when it is one this build models.
                event.set(
                    6,
                    (openMenu.slotAt(packet.slot) as? MenuSlot.Inventory)?.let { session.inventory[it.index]?.id },
                )
            }
        if (clicked?.cancelled == true) {
            clicked.cancelMessage?.let { sendPluginNotice(it) }
            // The client moved the item on its own screen the moment it clicked.
            sendInventory()
            return
        }

        // A plugin's window: the click is a button press, so nothing moves and the plugin is told
        // which of *its* slots was meant, never the player's own.
        val menu = pluginMenu
        if (menu != null) {
            (openMenu.slotAt(packet.slot) as? MenuSlot.Block)?.let { pluginMenuClick?.invoke(it.index) }
            sendInventory()
            return
        }

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
        // parity: `AbstractContainerMenu.quickMoveStack`, whose whole job is picking the other side:
        // with a block open, the inventory sends to the block and the block sends to the inventory.
        // Without a block open the two halves of the inventory swap, hotbar to storage and back.
        val intoBlock = slot is MenuSlot.Inventory && hasBlockSlots()
        val targets: List<MenuSlot> =
            when {
                intoBlock -> (0 until openBlockSize).map { MenuSlot.Block(it) }
                slot !is MenuSlot.Inventory ->
                    (
                        (PlayerInventory.HOTBAR_START until PlayerInventory.MAIN_SLOTS) +
                            (0 until PlayerInventory.HOTBAR_START)
                    ).map { MenuSlot.Inventory(it) }
                slot.index < PlayerInventory.HOTBAR_START ->
                    (PlayerInventory.HOTBAR_START until PlayerInventory.MAIN_SLOTS).map { MenuSlot.Inventory(it) }
                else -> (0 until PlayerInventory.HOTBAR_START).map { MenuSlot.Inventory(it) }
            }

        var remaining = stack.count
        // Stacking onto what is already there comes first, exactly as vanilla's `moveItemStackTo`.
        for (target in targets) {
            if (remaining <= 0) break
            if (!accepts(target, stack)) continue
            val existing = readSlot(session, target) ?: continue
            if (existing.id != stack.id || existing.components != stack.components) continue
            val room = PlayerInventory.MAX_STACK - existing.count
            if (room <= 0) continue
            val moved = minOf(room, remaining)
            writeSlot(session, target, existing.copy(count = existing.count + moved))
            remaining -= moved
        }
        for (target in targets) {
            if (remaining <= 0) break
            if (!accepts(target, stack)) continue
            if (readSlot(session, target) != null) continue
            writeSlot(session, target, stack.copy(count = remaining))
            remaining = 0
        }
        writeSlot(session, slot, if (remaining <= 0) null else stack.copy(count = remaining))
    }

    /** Whether the open screen has slots of its own to shift-click into. */
    private fun hasBlockSlots(): Boolean = openBlock != null || openEnderChest || openStonecutter || openSmithing

    /**
     * parity: `Slot.mayPlace`, which is why a shift-clicked log lands in a furnace's fuel slot and
     * not on top of what is being smelted.
     */
    private fun accepts(
        target: MenuSlot,
        stack: StoredItemStack,
    ): Boolean {
        if (target !is MenuSlot.Block) return true
        // parity: `StonecutterMenu.quickMoveStack` only ever moves into the input.
        if (openStonecutter) return target.index == Menu.STONECUTTER_INPUT
        // parity: `SmithingMenu.quickMoveStack`, which tries the three inputs and never the result.
        if (openSmithing) return target.index != Menu.SMITHING_RESULT
        if (openBlockSize != FURNACE_SLOTS) return true
        val item = runCatching { Identifier.parse(stack.id) }.getOrNull() ?: return false
        return when (target.index) {
            Furnaces.INPUT_SLOT -> true
            Furnaces.FUEL_SLOT -> Fuels.isFuel(item)
            // Nothing goes into the slot the cooked thing comes out of.
            else -> false
        }
    }

    /** Throws a stack into the world from this player, the same arc `Q` uses. */
    private fun dropStack(
        session: PlayerSession,
        stack: StoredItemStack,
    ) {
        val yaw = session.yaw * PI / 180.0
        val pitch = session.pitch * PI / 180.0
        val tossAngle = Random.nextDouble() * 2 * PI
        val tossSpread = TOSS_SPREAD * Random.nextDouble()
        spawnDroppedItem(
            item = Identifier.parse(stack.id),
            count = stack.count,
            x = session.x,
            y = session.y + THROW_EYE_HEIGHT,
            z = session.z,
            velocityX = -sin(yaw) * cos(pitch) * THROW_SPEED + cos(tossAngle) * tossSpread,
            velocityY = -sin(pitch) * THROW_SPEED + THROW_LIFT + tossLift(),
            velocityZ = cos(yaw) * cos(pitch) * THROW_SPEED + sin(tossAngle) * tossSpread,
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
     * nothing. [ServerboundPickItemFromBlockPacket.includeData], the control-click that copies a
     * sign's text or a container's contents onto the picked stack, is a documented gap.
     */
    private fun handlePickItemFromBlock(packet: ServerboundPickItemFromBlockPacket) {
        val session = playerSession ?: return
        val stateId = world.getBlockState(packet.pos.x, packet.pos.y, packet.pos.z) ?: return
        if (isAirLike(stateId)) return
        val block = Blocks.byStateId(stateId) ?: return
        // parity: `Block.asItem`, which is not the block's own name for dust, crops or cocoa.
        val item = Items.REGISTRY.byIdentifier(ItemBlocks.itemOf(block.block)) ?: return
        val picking =
            playerEvent(PluginEventTypes.PLAYER_PICK_ITEM) { event ->
                event.set(3, packet.pos.x)
                event.set(4, packet.pos.y)
                event.set(5, packet.pos.z)
                event.set(6, item.id.toString())
            }
        if (picking?.cancelled == true) {
            picking.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
        pickItem(session, item.id.toString())
    }

    /**
     * Middle-click "pick block" on an entity.
     *
     * parity: `handlePickItemFromEntity` over `Entity.getPickResult`: a mob answers with its own
     * spawn egg, a boat or a minecart with the item it was placed from, and everything else with
     * nothing at all. The control-click that copies the entity's data is ignored, as it is for a
     * block: there are no item data components here yet.
     */
    private fun handlePickItemFromEntity(packet: ServerboundPickItemFromEntityPacket) {
        val session = playerSession ?: return
        val entity = currentWorld?.state?.entity(packet.entityId) ?: return
        if (entity.removed) return
        if (!withinAttackReach(session, entity)) return
        val item = EntityPickResults.itemFor(entity.type) ?: return
        pickItem(session, item.toString())
    }

    /**
     * Puts [id] in the player's hand, however vanilla gets it there.
     *
     * parity: `tryPickItem`: already in the hotbar means only the selection moves, elsewhere in the
     * inventory means a swap rather than a copy, and conjuring one from nothing is creative only.
     */
    private fun pickItem(
        session: PlayerSession,
        id: String,
    ) {
        val inventory = session.inventory

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
        trackFall(session, y, onGround, feet)
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
     * Fires a player event, filling in the three fields every one of them starts with.
     *
     * Inline, so a type nobody listens to costs the same null check as anywhere else: the values
     * after these three are only worked out inside [fill], which never runs without a listener.
     */
    private inline fun playerEvent(
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
    private fun sendPluginNotice(message: String) {
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

    /** parity: `LivingEntity.addEffect`, for the effects a mob puts on somebody rather than a drink. */
    internal fun addEffect(
        effect: Identifier,
        durationTicks: Int,
        amplifier: Int,
    ) {
        val session = playerSession ?: return
        val given =
            playerEvent(PluginEventTypes.PLAYER_EFFECT) { event ->
                event.set(3, effect.toString())
                event.set(4, amplifier)
                event.set(5, durationTicks)
            }
        if (given?.cancelled == true) return
        val active = ActiveEffect(effect = effect, amplifier = amplifier, remainingTicks = durationTicks)
        session.effects[effect] = active
        sendEffect(active)
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
        // Whatever a plugin set goes out beside them, and wins where both name the same attribute:
        // the base a plugin chose is the base, and the enchantments are modifiers on top.
        val fromPlugins =
            session.attributeBases.mapNotNull { (name, base) ->
                AttributeIds.protocolId(name)?.let { id ->
                    UpdateAttributesPacket.AttributeSnapshot(attribute = id, base = base)
                }
            }
        val all =
            snapshots.filterNot { snapshot -> fromPlugins.any { it.attribute == snapshot.attribute } } +
                fromPlugins
        sendPlayPacket(UpdateAttributesPacket.ID) {
            UpdateAttributesPacket(session.entityId, all).write(it)
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
        val session = playerSession ?: return
        // A plugin holding a surface is reason enough to draw, even with all four files switched off.
        if (!ui().anyEnabled &&
            pluginHoldsSidebar?.invoke(session.uuid.toString()) != true &&
            pluginHoldsBossBar?.invoke() != true
        ) {
            return
        }
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
        // parity: `Player.tick`, which counts this up for as long as the player is not swinging.
        session.ticksSinceSwing++
        if (session.dead) return
        tryPickupNearbyItems()
        tickEating(session)
        tickDrawing(session)
        if (session.sleepingBed != null) session.sleepTicks++
        tickDigging(session)
        if (session.gamemode == Gamemode.CREATIVE || session.gamemode == Gamemode.SPECTATOR) return

        val healthBefore = session.health
        val foodBefore = session.food.foodLevel
        val saturationBefore = session.food.saturationLevel
        session.food.tick(
            difficulty = difficulty,
            health = session.health,
            maxHealth = session.maxHealth,
            heal = { amount -> session.health = minOf(session.health + amount, session.maxHealth) },
            hurt = { amount -> hurt(amount, DEATH_STARVE, DamageTypes.STARVE) },
        )
        // Only when something actually moved: `set_health` every tick is 20 packets a second per
        // player for a bar that changes a few times a minute.
        if (session.food.foodLevel != foodBefore) {
            val change =
                playerEvent(PluginEventTypes.PLAYER_FOOD_CHANGE) { event ->
                    event.set(3, session.food.foodLevel)
                    event.set(4, foodBefore)
                }
            if (change != null) {
                val wanted =
                    if (change.cancelled) {
                        foodBefore
                    } else {
                        (
                            (change.get("food") as? Long)?.toInt()
                                ?: session.food.foodLevel
                        )
                    }
                if (wanted != session.food.foodLevel) session.food.set(wanted, session.food.saturationLevel)
            }
        }
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
        source: DamageSource? = null,
    ) {
        val session = playerSession ?: return
        if (session.dead || session.invulnerable) return
        // A player is an entity too, so it is the same event; what changes is that the choke point
        // for a player is here rather than in EntityCombat.
        var amount = amount
        if (pluginEvents != null && pluginEvents.listening(PluginEventTypes.ENTITY_DAMAGE)) {
            val event = pluginEvents.event(PluginEventTypes.ENTITY_DAMAGE)
            event.set(0, session.entityId)
            event.set(1, "minecraft:player")
            event.set(2, currentWorld?.name)
            event.set(3, damageType?.path ?: deathMessageId)
            event.set(4, amount.toDouble())
            event.set(5, source?.entityId?.toString())
            if (!pluginEvents.fire(event)) return
            amount = ((event.get("amount") as? Double) ?: amount.toDouble()).toFloat()
            if (amount <= 0f) return
        }
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
        if (!cooling) {
            sendDamageEvent(session, damageType, source?.entityId ?: -1)
            if (source != null) pushAway(session, source)
        }
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
        val healing =
            playerSession?.let { session ->
                pluginEvents.emit(PluginEventTypes.ENTITY_HEAL) { event ->
                    event.set(0, session.entityId)
                    event.set(1, "minecraft:player")
                    event.set(2, currentWorld?.name)
                    event.set(3, amount.toDouble())
                    event.set(4, "regen")
                }
            }
        if (healing?.cancelled == true) return
        @Suppress("NAME_SHADOWING")
        val amount = ((healing?.get("amount") as? Double) ?: amount.toDouble()).toFloat()
        val session = playerSession ?: return
        if (session.dead) return
        val healed = minOf(session.health + amount, session.maxHealth)
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
        playerSession?.let { session ->
            pluginEvents.emit(PluginEventTypes.PLAYER_DEATH) { event ->
                event.set(0, session.name)
                event.set(1, session.uuid.toString())
                event.set(2, currentWorld?.name)
                event.set(3, session.x)
                event.set(4, session.y)
                event.set(5, session.z)
                event.set(6, deathMessageId)
            }
        }
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
        pluginEvents.emit(PluginEventTypes.PLAYER_RESPAWN) { event ->
            event.set(0, session.name)
            event.set(1, session.uuid.toString())
            event.set(2, respawnWorld.name)
            event.set(3, spawnX)
            event.set(4, spawnY)
            event.set(5, spawnZ)
        }

        forgetEverythingDrawn()
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
        val change =
            playerEvent(PluginEventTypes.PLAYER_GAMEMODE_CHANGE) { event ->
                event.set(3, mode.name.lowercase())
                event.set(4, session.gamemode.name.lowercase())
            }
        if (change?.cancelled == true) {
            change.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
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
        val from = currentWorld?.name
        if (from != target.name) {
            pluginEvents.emit(PluginEventTypes.PLAYER_WORLD_CHANGE) { event ->
                event.set(0, session.name)
                event.set(1, session.uuid.toString())
                event.set(2, target.name)
                event.set(3, from)
            }
        }
        val previous = currentWorld
        if (previous == null || previous.name == target.name) return false

        currentWorld = target
        session.worldName = target.name
        session.x = x
        session.y = y
        session.z = z

        forgetEverythingDrawn()
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
        /** What asked for it, for a plugin that treats a command differently from a portal. */
        cause: String = "command",
    ) {
        val session = playerSession
        val teleport =
            playerEvent(PluginEventTypes.PLAYER_TELEPORT) { event ->
                event.set(3, session?.x)
                event.set(4, session?.y)
                event.set(5, session?.z)
                event.set(6, x)
                event.set(7, y)
                event.set(8, z)
                event.set(9, cause)
            }
        if (teleport?.cancelled == true) {
            teleport.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
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
        // A click on a block and the click that follows it are one mouthful, not two: the client
        // sends `use_item_on` and then `use_item` for the same press, and restarting the timer on
        // the second one meant a held button never finished a single one. parity: vanilla starts the
        // use once because its `useItemOn` never falls through to `use` on the same packet.
        if (eatingSlot == slot) return true
        val eating = playerEvent(PluginEventTypes.PLAYER_ITEM_CONSUME) { event -> event.set(3, item.toString()) }
        if (eating?.cancelled == true) {
            eating.cancelMessage?.let { sendPluginNotice(it) }
            sendInventory()
            return false
        }
        if (!food.canAlwaysEat && session.food.foodLevel >= FoodData.MAX_FOOD) {
            // parity: `ServerPlayerGameMode.useItem` sends the whole inventory back whenever the use
            // never started. Without it a client that had already predicted the mouthful kept the
            // animation and the missing item for good, which is what a full player saw here.
            sendInventoryAll()
            syncHealth()
            return true
        }
        eatingSlot = slot
        eatingTicks = food.consumeTicks
        setUsingItem(true)
        return true
    }

    /**
     * Starts drawing a bow, if there is anything to shoot with it.
     *
     * parity: `BowItem.use`, which refuses without an arrow unless the player has infinite
     * materials, and otherwise just starts the draw: the shot happens on release.
     */
    private fun startDrawing(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
    ): Boolean {
        if (item.path != BOW) return false
        if (session.gamemode != Gamemode.CREATIVE && session.inventory.indexOf(ARROW_ITEM) == null) return true
        drawingSlot = slot
        drawingTicks = 0
        setUsingItem(true)
        return true
    }

    private fun stopDrawing() {
        drawingSlot = -1
        drawingTicks = 0
        setUsingItem(false)
    }

    /**
     * Lets a drawn bow go.
     *
     * parity: `BowItem.releaseUsing`: the power is the square of the draw, a shot under a tenth is
     * no shot at all, and a full draw flies at three times that and counts as a critical.
     */
    private fun releaseBow() {
        val session = playerSession ?: return
        if (drawingSlot < 0) return
        val held = session.inventory[drawingSlot]
        val drawn = drawingTicks
        stopDrawing()
        if (held == null || Identifier.parse(held.id).path != BOW) return
        val power = bowPower(drawn)
        if (power < MIN_BOW_POWER) return
        val creative = session.gamemode == Gamemode.CREATIVE
        val arrowSlot = session.inventory.indexOf(ARROW_ITEM)
        if (!creative && arrowSlot == null) return
        val world = currentWorld ?: return
        val entityId = playerRegistry?.allocateEntityId() ?: return
        val protocolType = EntityTypes[Projectiles.ARROW.type]?.protocolId ?: return
        val shooting =
            pluginEvents.emit(PluginEventTypes.ENTITY_SHOOT) { event ->
                event.set(0, session.entityId)
                event.set(1, "minecraft:player")
                event.set(2, currentWorld?.name)
                event.set(3, Projectiles.ARROW.type.toString())
                event.set(4, power.toDouble())
            }
        if (shooting?.cancelled == true) return

        val pitch = session.pitch * PI.toFloat() / HALF_CIRCLE
        val yaw = session.yaw * PI.toFloat() / HALF_CIRCLE
        val arrow =
            Projectiles.spawn(
                world = world.state,
                kind = Projectiles.ARROW,
                entityId = entityId,
                protocolType = protocolType,
                x = session.x,
                y = session.y + THROW_EYE_HEIGHT,
                z = session.z,
                dirX = (-sin(yaw) * cos(pitch)).toDouble(),
                dirY = (-sin(pitch)).toDouble(),
                dirZ = (cos(yaw) * cos(pitch)).toDouble(),
                power = power * BOW_POWER_SCALE,
                uncertainty = BOW_UNCERTAINTY,
                random = LegacyRandom(Random.nextLong()),
            )
        arrow.ownerId = session.entityId
        arrow.critical = power >= 1f
        broadcastSound(
            SOUND_ARROW_SHOOT,
            session.x,
            session.y,
            session.z,
            SoundSource.PLAYERS,
            pitch = 1f / (Random.nextFloat() * 0.4f + 1.2f) + power * 0.5f,
        )
        if (creative || arrowSlot == null) return
        val stack = session.inventory[arrowSlot] ?: return
        session.inventory[arrowSlot] = if (stack.count <= 1) null else stack.copy(count = stack.count - 1)
        sendInventorySlot(arrowSlot)
    }

    /** One tick of the draw. Letting go of the bow, or of the slot it is in, gives it up. */
    private fun tickDrawing(session: PlayerSession) {
        if (drawingSlot < 0) return
        val held = session.inventory[drawingSlot]
        if (held == null ||
            Identifier.parse(held.id).path != BOW ||
            drawingSlot != PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        ) {
            stopDrawing()
            return
        }
        drawingTicks++
    }

    /**
     * parity: `EnderpearlItem.use`: the pearl flies at one and a half, with a degree of spread, and
     * one leaves the hand however far it gets.
     */
    private fun throwEnderPearl(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
    ): Boolean {
        if (item.path != ENDER_PEARL) return false
        val world = currentWorld ?: return true
        val entityId = playerRegistry?.allocateEntityId() ?: return true
        val protocolType = EntityTypes[Projectiles.ENDER_PEARL.type]?.protocolId ?: return true
        val pitch = session.pitch * PI.toFloat() / HALF_CIRCLE
        val yaw = session.yaw * PI.toFloat() / HALF_CIRCLE
        val pearl =
            Projectiles.spawn(
                world = world.state,
                kind = Projectiles.ENDER_PEARL,
                entityId = entityId,
                protocolType = protocolType,
                x = session.x,
                y = session.y + THROW_EYE_HEIGHT,
                z = session.z,
                dirX = (-sin(yaw) * cos(pitch)).toDouble(),
                dirY = (-sin(pitch)).toDouble(),
                dirZ = (cos(yaw) * cos(pitch)).toDouble(),
                power = PEARL_POWER,
                uncertainty = PEARL_UNCERTAINTY,
                random = LegacyRandom(Random.nextLong()),
            )
        pearl.ownerId = session.entityId
        startCooldown(item)
        broadcastSound(
            SOUND_PEARL_THROW,
            session.x,
            session.y,
            session.z,
            SoundSource.NEUTRAL,
            volume = 0.5f,
            pitch = 0.4f / (Random.nextFloat() * 0.4f + 0.8f),
        )
        if (session.gamemode == Gamemode.CREATIVE) return true
        val held = session.inventory[slot] ?: return true
        session.inventory[slot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
        sendInventorySlot(slot)
        return true
    }

    /** Whether this item is still greyed out. parity: `ItemCooldowns.isOnCooldown`. */
    private fun onCooldown(item: Identifier): Boolean {
        val until = itemCooldowns[item.path] ?: return false
        if (until > playTimeTicks) return true
        itemCooldowns.remove(item.path)
        return false
    }

    /**
     * Starts the wait an item's own `use_cooldown` component asks for, and tells the client so the
     * slot greys out. parity: `UseCooldown.apply`.
     */
    private fun startCooldown(item: Identifier) {
        val ticks = ItemStacking.cooldownTicks(item)
        if (ticks <= 0) return
        itemCooldowns[item.path] = playTimeTicks + ticks
        sendPlayPacket(CooldownPacket.ID) { CooldownPacket(item.toString(), ticks).write(it) }
    }

    /**
     * The "this one is using an item" flag, sent to everybody including its owner.
     *
     * parity: `LivingEntity.setLivingEntityFlag(1, ...)`. The owner's own client needs it as much as
     * the onlookers do: `LocalPlayer.isUsingItem` is its own flag, and the only thing that clears it
     * is this metadata coming back saying the server is no longer using anything. Without it the
     * client thought it was still eating for ever, so holding the button never started a second
     * mouthful and the animation stayed up.
     */
    private fun setUsingItem(using: Boolean) {
        if (usingItem == using) return
        usingItem = using
        val session = playerSession ?: return
        val flags = if (using) LIVING_FLAG_USING_ITEM else 0
        val packet =
            SetEntityDataPacket(
                session.entityId,
                listOf(SetEntityDataPacket.Entry.Bytes(SetEntityDataPacket.LIVING_FLAGS_INDEX, flags.toByte())),
            )
        sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    /** parity: `BowItem.getPowerForTime`. */
    private fun bowPower(ticks: Int): Float {
        val drawn = ticks / BOW_DRAW_TICKS
        return minOf((drawn * drawn + drawn * 2f) / 3f, 1f)
    }

    private fun stopEating() {
        eatingSlot = -1
        eatingTicks = 0
        setUsingItem(false)
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
        val digging =
            playerEvent(PluginEventTypes.BLOCK_DAMAGE) { event ->
                event.set(3, pos.x)
                event.set(4, pos.y)
                event.set(5, pos.z)
                event.set(6, world.getBlockState(pos.x, pos.y, pos.z)?.let { BlockStateDefinitions.blockOf(it) })
            }
        if (digging?.cancelled == true) {
            digging.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
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
        // parity: `LeverBlock.updateNeighbours` and `ButtonBlock.updateNeighbours`. Without this the
        // lever moved and powered nothing at all, which is most of what a lever is for.
        for (block in toggled.changes) notifyNeighbours(BlockPos(block[0], block[1], block[2]))
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

    /** Moving a repeater's delay on, or a comparator's mode over: the change and its click. */
    private fun cycleBlock(pos: BlockPos) {
        val world = currentWorld ?: return
        val cycled = BlockInteractions.cycle(world, pos.x, pos.y, pos.z) ?: return
        broadcastBlockUpdate(pos, cycled.stateId)
        // parity: both of them are components, so the change has to reach the dust and the diodes
        // around them: a repeater set to four notches re-books its own turn from here.
        notifyNeighbours(pos)
        val sound = cycled.sound ?: return
        val packet =
            SoundPacket(
                sound = sound,
                source = SoundSource.BLOCKS,
                x = pos.x + 0.5,
                y = pos.y + 0.5,
                z = pos.z + 0.5,
                volume = cycled.volume,
                pitch = cycled.pitch,
                seed = Random.nextLong(),
            )
        sendPlayPacket(SoundPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.entityId == playerSession?.entityId) continue
            other.connection.sendPlayPacket(SoundPacket.ID) { packet.write(it) }
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
            blowUp(world, head.x + 0.5, head.y + 0.5, head.z + 0.5, Explosions.BED_POWER, "bed")
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
        val sleeping =
            playerEvent(PluginEventTypes.PLAYER_BED) { event ->
                event.set(3, head.x)
                event.set(4, head.y)
                event.set(5, head.z)
                event.set(6, true)
            }
        if (sleeping?.cancelled == true) {
            sleeping.cancelMessage?.let { sendPluginNotice(it) }
            return
        }
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
        causeEntityId: Int = -1,
    ) {
        val registryId = VanillaRegistryData.damageTypes.indexOfFirst { it.first == (damageType?.path ?: "generic") }
        if (registryId < 0) return
        val packet =
            DamageEventPacket(
                entityId = session.entityId,
                damageType = registryId,
                causeEntityId = causeEntityId,
                directEntityId = causeEntityId,
            )
        sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        for (other in playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(DamageEventPacket.ID) { packet.write(it) }
        }
    }

    /**
     * The shove a hit is worth, and the lean the client puts on the screen.
     *
     * parity: `LivingEntity.dealDefaultKnockback` and `ServerPlayer.indicateDamage`. A client moves
     * its own player, so the push only happens because the motion packet says so; the server has no
     * copy of what the player was already doing, so the push starts from a standstill.
     */
    private fun pushAway(
        session: PlayerSession,
        source: DamageSource,
    ) {
        val towardsX = source.x - session.x
        val towardsZ = source.z - session.z
        val moved =
            Combat.knockback(
                power = EntityCombat.DEFAULT_KNOCKBACK,
                towardsX = towardsX,
                towardsZ = towardsZ,
                velocityX = 0.0,
                velocityY = 0.0,
                velocityZ = 0.0,
                onGround = session.onGround,
            )
        sendPlayPacket(SetEntityMotionPacket.ID) {
            SetEntityMotionPacket(session.entityId, moved[0], moved[1], moved[2]).write(it)
        }
        val angle = (atan2(towardsZ, towardsX) * RADIANS_TO_DEGREES).toFloat() - session.yaw
        sendPlayPacket(HurtAnimationPacket.ID) { HurtAnimationPacket(session.entityId, angle).write(it) }
    }

    /**
     * The statistics screen's contents.
     *
     * parity: `ServerStatsCounter.sendStats`, which answers the screen's own request. Only the
     * counters this build keeps go out: the screen shows a zero for everything else, which is what
     * vanilla shows for a counter nobody has moved either.
     */
    private fun sendStats() {
        val session = playerSession ?: return
        val counters = session.stats.snapshot(playTimeTicks).counters
        val stats = mutableListOf<StatCounter>()
        for ((key, value) in counters) {
            val count = value.coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
            val counter =
                when {
                    key.startsWith(CUSTOM_STAT_PREFIX) ->
                        CustomStats.idOf(key.removePrefix(CUSTOM_STAT_PREFIX))?.let {
                            StatCounter(AwardStatsPacket.TYPE_CUSTOM, it, count)
                        }
                    key.startsWith(MINED_STAT_PREFIX) ->
                        Blocks.REGISTRY
                            .idOf(Identifier.parse(key.removePrefix(MINED_STAT_PREFIX)))
                            ?.let { StatCounter(AwardStatsPacket.TYPE_MINED, it, count) }
                    else -> null
                }
            counter?.let { stats += it }
        }
        sendCompressed(AwardStatsPacket.ID) { AwardStatsPacket(stats).write(it) }
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
        if (session.experience.level != lastSyncedLevel) {
            val was = lastSyncedLevel
            lastSyncedLevel = session.experience.level
            playerEvent(PluginEventTypes.PLAYER_LEVEL_CHANGE) { event ->
                event.set(3, session.experience.level)
                event.set(4, was)
            }
        }
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
        // And leaving with a chest open would leave its lid up and a trapped chest powered.
        closeOpenMenu(session)
        // Before removing them from the registry, so a save triggered elsewhere cannot race this
        // one into writing a player who is already half gone.
        savePlayerState()
        registry.remove(session.uuid)
        progress?.unload(session.uuid.toString())
        logger.info { "${session.name} left the game" }
        pluginEvents.emit(PluginEventTypes.PLAYER_QUIT) { event ->
            event.set(0, session.name)
            event.set(1, session.uuid.toString())
            event.set(2, currentWorld?.name)
        }
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

    private fun sendCompressed(
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
        bytes = interceptOutgoing(id, bytes, idBytes) ?: return
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
     * Shows an incoming packet to whoever is watching it, and answers whether to handle it.
     *
     * The bytes are read without consuming them, so a listener seeing a packet costs a copy and the
     * server still decodes it exactly as it would have. Replacing them puts the new body in the same
     * buffer, which is what makes changing one on the way in possible at all before the fields
     * themselves are named.
     */
    private fun interceptIncoming(
        packetId: Int,
        body: ByteBuffer,
    ): Boolean {
        val bus = pluginPackets ?: return true
        val name = PacketNames.of(state, serverbound = true, id = packetId)
        if (!bus.listening(name)) return true
        val session = playerSession
        val packet = bus.packet()
        packet.name = name
        packet.serverbound = true
        packet.playerName = session?.name.orEmpty()
        packet.playerUuid = session?.uuid?.toString().orEmpty()
        packet.bytes = body.toByteArray()
        if (!bus.fire(packet)) return false
        packet.replacement?.let {
            body.clear()
            body.writeBytes(it)
        }
        return true
    }

    /**
     * Shows an outgoing packet to whoever is watching it, and answers with what to send.
     *
     * Null means a plugin cancelled it. The first line is the whole performance story: with nobody
     * listening to that name this is one map lookup and the bytes are never copied.
     */
    private fun interceptOutgoing(
        id: Int,
        bytes: ByteArray,
        idBytes: Int,
    ): ByteArray? {
        val bus = pluginPackets ?: return bytes
        val name = PacketNames.of(state, serverbound = false, id = id)
        if (!bus.listening(name)) return bytes
        val session = playerSession
        val packet = bus.packet()
        packet.name = name
        packet.serverbound = false
        packet.playerName = session?.name.orEmpty()
        packet.playerUuid = session?.uuid?.toString().orEmpty()
        packet.bytes = bytes.copyOfRange(idBytes, bytes.size)
        if (!bus.fire(packet)) return null
        val replaced = packet.replacement ?: return bytes
        return bytes.copyOfRange(0, idBytes) + replaced
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

    private companion object {
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
