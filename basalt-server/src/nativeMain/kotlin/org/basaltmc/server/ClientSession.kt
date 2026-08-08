package org.basaltmc.server

import org.basaltmc.auth.Cfb8Cipher
import org.basaltmc.auth.MinecraftCrypt
import org.basaltmc.auth.OfflineUuid
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionProfileProperty
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.ByteBuffer
import org.basaltmc.io.TcpConnection
import org.basaltmc.io.Zlib
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
import org.basaltmc.protocol.play.ChatMessagePacket
import org.basaltmc.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.protocol.play.ChunkBatchStartPacket
import org.basaltmc.protocol.play.ChunkBlockEntityInfo
import org.basaltmc.protocol.play.ClientboundBlockChangedAckPacket
import org.basaltmc.protocol.play.ClientboundBlockUpdatePacket
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
import org.basaltmc.protocol.play.PlayerInfoAddPacket
import org.basaltmc.protocol.play.PlayerInfoEntry
import org.basaltmc.protocol.play.PlayerInfoRemovePacket
import org.basaltmc.protocol.play.PlayerLoadedPacket
import org.basaltmc.protocol.play.PlayerPositionPacket
import org.basaltmc.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.protocol.play.ServerboundPositionLookPacket
import org.basaltmc.protocol.play.ServerboundPositionPacket
import org.basaltmc.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.protocol.play.SetSimulationDistancePacket
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
import org.basaltmc.server.player.PlayerRegistry
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.server.registry.VanillaBiomes
import org.basaltmc.server.registry.VanillaRegistryData
import org.basaltmc.server.registry.VanillaTagData
import org.basaltmc.server.world.WorldState
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkDataCodec
import org.basaltmc.world.ChunkTracker
import kotlin.random.Random
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
 * through [playerRegistry] so other connected players see them. Breaking a block ([handlePlayerAction])
 * is instant-only (creative-style, no hardness/tool timing) and broadcasts to every connected
 * player through [WorldState]'s single lock; placing one is acknowledged but not yet acted on,
 * since it needs a held-item/inventory model that doesn't exist yet.
 */
@OptIn(ExperimentalUuidApi::class)
public class ClientSession(
    private val connection: TcpConnection,
    private val statusProvider: () -> org.basaltmc.protocol.status.ServerStatusPayload,
    private val compressionThreshold: Int = 256,
    private val onlineMode: OnlineMode? = null,
    private val playerRegistry: PlayerRegistry? = null,
    private val world: WorldState = WorldState(emptyList()),
    private val spawn: Triple<Double, Double, Double> = Triple(8.5, 4.0, 8.5),
    private val configuredViewDistance: Int = 10,
) {
    private val logger = Logger("Session")
    private val incoming = ByteBuffer()
    private var state = ProtocolState.HANDSHAKE
    private var compressionEnabled = false

    private var pendingUsername: String? = null
    private var challenge: ByteArray? = null
    private var decryptCipher: Cfb8Cipher? = null
    private var encryptCipher: Cfb8Cipher? = null

    private var pendingProfile: GameProfile? = null
    private var playerSession: PlayerSession? = null

    private val chunkTracker = ChunkTracker()
    private var viewRadius = 0
    private var centerChunkX = 0
    private var centerChunkZ = 0

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
        } finally {
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
            StatusRequestPacket.ID ->
                sendUncompressed(
                    StatusResponsePacket.ID,
                ) { StatusResponsePacket(statusProvider()).write(it) }
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
                VanillaBiomes.ALL.map { entry -> RegistryEntry("minecraft:${entry.path}", with(VanillaBiomes) { entry.toNbt() }) },
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

        val chunkRadius = configuredViewDistance
        val (spawnX, spawnY, spawnZ) = spawn

        val entityId = registry.allocateEntityId()
        sendCompressed(LoginPlayPacket.ID) {
            LoginPlayPacket(
                entityId = entityId,
                isHardcore = false,
                worldNames = listOf("minecraft:overworld"),
                maxPlayers = 20,
                viewDistance = chunkRadius,
                simulationDistance = chunkRadius,
                reducedDebugInfo = false,
                enableRespawnScreen = true,
                doLimitedCrafting = false,
                spawnInfo =
                    SpawnInfo(
                        dimension = 0,
                        dimensionName = "minecraft:overworld",
                        hashedSeed = 0L,
                        // Must match what block breaking actually does (instant, no hardness/tool
                        // timing): reporting SURVIVAL while breaking like CREATIVE would desync the
                        // client's own prediction, which times survival mining client-side before
                        // sending STOP_DESTROY_BLOCK. Switch this back to SURVIVAL only once real
                        // hardness-timed breaking exists.
                        gamemode = Gamemode.CREATIVE,
                        previousGamemode = 255,
                        isDebug = false,
                        isFlat = true,
                        death = null,
                        portalCooldown = 0,
                        seaLevel = 63,
                    ),
                onlineMode = onlineMode != null,
                enforcesSecureChat = false,
            ).write(it)
        }

        // Without these, a real client's "loading terrain" screen never dismisses: it has no
        // center to count chunks around and no radius to know when it has enough (confirmed live).
        centerChunkX = spawnX.toInt() shr 4
        centerChunkZ = spawnZ.toInt() shr 4
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
                GlobalPos("minecraft:overworld", BlockPos(spawnX.toInt(), spawnY.toInt(), spawnZ.toInt())),
                yaw = 0f,
                pitch = 0f,
            ).write(it)
        }
        sendCompressed(PlayerPositionPacket.ID) {
            PlayerPositionPacket(0, spawnX, spawnY, spawnZ, 0.0, 0.0, 0.0, 0f, 0f, relativeFlags = 0).write(it)
        }

        val session = PlayerSession(profile.id, profile.name, entityId, this)
        session.x = spawnX
        session.y = spawnY
        session.z = spawnZ
        playerSession = session

        val others = registry.snapshot()
        registry.add(session)
        logger.info { "${session.name} logged in with entity id $entityId at ($spawnX, $spawnY, $spawnZ)" }
        logger.info { "${session.name} joined the game" }

        sendCompressed(PlayerInfoAddPacket.ID) {
            PlayerInfoAddPacket(
                others.map { PlayerInfoEntry(it.uuid, it.name, listed = true, latencyMillis = 0) } +
                    PlayerInfoEntry(session.uuid, session.name, listed = true, latencyMillis = 0),
            ).write(it)
        }

        // The client only starts tracking/waiting for level chunks once it sees this event: without
        // it (confirmed live) the loading screen never dismisses even after every chunk and a
        // correct chunk-batch wrapper arrive, since the client was never told to start expecting them.
        sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.LEVEL_CHUNKS_LOAD_START, 0f).write(it)
        }
        // The declared view distance above is a promise: generate (in parallel, off this one
        // thread; see WorldState.getOrGenerateChunks's own KDoc) and send every chunk it actually
        // covers, not just whatever BasaltServer happened to pre-generate at startup. Chunks
        // already warm from that startup batch are free hits in WorldState's own cache.
        val loginPositions =
            (-chunkRadius..chunkRadius).flatMap { dx ->
                (-chunkRadius..chunkRadius).map { dz -> (centerChunkX + dx) to (centerChunkZ + dz) }
            }
        val loginChunks = world.getOrGenerateChunks(loginPositions)
        sendCompressed(ChunkBatchStartPacket.ID) { }
        for (chunk in loginChunks) sendChunk(chunk)
        sendCompressed(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(loginChunks.size).write(it) }

        for (other in others) {
            other.connection.sendPlayPacket(PlayerInfoAddPacket.ID) {
                PlayerInfoAddPacket(
                    listOf(PlayerInfoEntry(session.uuid, session.name, listed = true, latencyMillis = 0)),
                ).write(it)
            }
            other.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(session.entityId, session.x, session.y, session.z, 0, 0, onGround = true).write(it)
            }
            sendCompressed(EntityTeleportPacket.ID) {
                EntityTeleportPacket(
                    other.entityId,
                    other.x,
                    other.y,
                    other.z,
                    angleToByte(other.yaw),
                    angleToByte(other.pitch),
                    other.onGround,
                ).write(it)
            }
        }
    }

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
            ServerboundPlayerActionPacket.ID -> {
                val action = ServerboundPlayerActionPacket.read(body)
                handlePlayerAction(action)
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
            AcceptTeleportationPacket.ID,
            PlayKeepAlivePacket.SERVERBOUND_ID,
            ChunkBatchReceivedPacket.ID,
            PlayerLoadedPacket.ID,
            -> Unit
        }
    }

    /**
     * Instant-break only, matching vanilla's creative-mode digging (`ServerPlayerGameMode.
     * destroyBlock` on the first packet, no timing): [ServerboundPlayerActionPacket.Action
     * .ABORT_DESTROY_BLOCK]/[ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK] are no-ops.
     * Survival's hardness/tool-speed-timed breaking needs block hardness data and a real game mode
     * model, neither of which exist yet: documented gap, not a silent shortcut.
     */
    private fun handlePlayerAction(action: ServerboundPlayerActionPacket) {
        if (action.action != ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK) {
            sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(action.sequence).write(it)
            }
            return
        }

        val changed = world.setBlockState(action.pos.x, action.pos.y, action.pos.z, Blocks.AIR.stateId)
        sendCompressed(ClientboundBlockChangedAckPacket.ID) {
            ClientboundBlockChangedAckPacket(action.sequence).write(it)
        }
        if (!changed) return

        for (player in playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(action.pos, Blocks.AIR.stateId).write(it)
            }
        }
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
        session.x = x
        session.y = y
        session.z = z
        session.yaw = yaw
        session.pitch = pitch
        session.onGround = onGround
        streamChunksAround(x, z)
        for (other in playerRegistry?.snapshot().orEmpty()) {
            if (other.uuid == session.uuid) continue
            other.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(
                    session.entityId,
                    x,
                    y,
                    z,
                    angleToByte(yaw),
                    angleToByte(pitch),
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
        centerChunkX = chunkX
        centerChunkZ = chunkZ

        val delta = chunkTracker.update(chunkX, chunkZ, viewRadius)
        if (delta.toLoad.isEmpty() && delta.toUnload.isEmpty()) return

        sendCompressed(SetChunkCacheCenterPacket.ID) { SetChunkCacheCenterPacket(chunkX, chunkZ).write(it) }
        if (delta.toLoad.isNotEmpty()) {
            sendCompressed(ChunkBatchStartPacket.ID) { }
            // Generation itself runs across a worker pool (WorldState.getOrGenerateChunks); only
            // the actual network writes stay serialized here, on this connection's own thread.
            val loaded = world.getOrGenerateChunks(delta.toLoad.map { it.x to it.z })
            for (chunk in loaded) sendChunk(chunk)
            sendCompressed(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(delta.toLoad.size).write(it) }
        }
        for (pos in delta.toUnload) {
            sendCompressed(ForgetLevelChunkPacket.ID) { ForgetLevelChunkPacket(pos.x, pos.z).write(it) }
        }
    }

    private fun broadcastChat(message: String) {
        val session = playerSession ?: return
        val text = Component.text("<${session.name}> $message", TextColor.WHITE)
        for (player in playerRegistry?.snapshot().orEmpty()) {
            player.connection.sendPlayPacket(
                SystemChatPacket.ID,
            ) { SystemChatPacket(text, isActionBar = false).write(it) }
        }
    }

    private fun leavePlayIfPresent() {
        val session = playerSession ?: return
        val registry = playerRegistry ?: return
        registry.remove(session.uuid)
        logger.info { "${session.name} left the game" }
        for (other in registry.snapshot()) {
            other.connection.sendPlayPacket(
                PlayerInfoRemovePacket.ID,
            ) { PlayerInfoRemovePacket(listOf(session.uuid)).write(it) }
        }
    }

    internal fun sendPlayPacket(
        id: Int,
        writeBody: (ByteBuffer) -> Unit,
    ) {
        sendCompressed(id, writeBody)
    }

    internal fun sendKeepAlive() {
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

    private fun writeToSocket(bytes: ByteArray) {
        val cipher = encryptCipher
        connection.write(if (cipher != null) cipher.update(bytes, encrypting = true) else bytes)
    }
}

/** [org.basaltmc.protocol.configuration.ConfigurationKeepAlivePacket]'s id, matched here just to no-op it explicitly. */
private const val CONFIGURATION_KEEP_ALIVE_ID = 4

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
