package org.basaltmc.server.runtime.plugins

import org.basaltmc.server.plugins.PluginEntityRef
import org.basaltmc.server.plugins.PluginHandle
import org.basaltmc.server.plugins.PluginHologramRef
import org.basaltmc.server.plugins.PluginInventory
import org.basaltmc.server.plugins.PluginItem
import org.basaltmc.server.plugins.PluginMenus
import org.basaltmc.server.plugins.PluginNpcRef
import org.basaltmc.server.plugins.PluginPlaceholders
import org.basaltmc.server.plugins.PluginPlayerRef
import org.basaltmc.server.plugins.PluginPlayers
import org.basaltmc.server.plugins.PluginWorldRef
import org.basaltmc.server.plugins.PluginWorlds
import org.basaltmc.server.registry.AttributeIds
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.ParticleTypes
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.placeholder.PlaceholderSlots
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.GameRule
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.storage.StoredInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.common.placeholder.PlaceholderText
import org.basaltmc.wire.common.placeholder.PlaceholderValue
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.SetSubtitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitlesAnimationPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.MiniMessage

/**
 * The server's own objects, seen through the plugin API's handles.
 *
 * Handles, not copies: everything here reads the live thing when it is asked, and a handle to
 * somebody who left answers `online = false` rather than a stale name and a health of 20. The
 * writes go through the same paths a command would, so a plugin setting a gamemode fires the same
 * event and sends the same packets.
 */
internal class SessionPlayerRef(
    private val registry: PlayerRegistry,
    private val uuidText: String,
    private val fallbackName: String,
    private val groups: () -> org.basaltmc.server.runtime.config.GroupsConfig,
    private val worldFor: (String) -> LoadedWorld?,
) : PluginPlayerRef {
    private val session: PlayerSession?
        get() = registry.snapshot().firstOrNull { it.uuid.toString() == uuidText }

    override val name: String get() = session?.name ?: fallbackName
    override val uuid: String get() = uuidText
    override val online: Boolean get() = session != null

    override var health: Double
        get() = session?.health?.toDouble() ?: 0.0
        set(value) {
            val player = session ?: return
            player.health = value.toFloat().coerceIn(0f, player.maxHealth)
            player.connection.syncHealth()
        }

    override var food: Int
        get() = session?.food?.foodLevel ?: 0
        set(value) {
            val player = session ?: return
            player.food.set(value.coerceIn(0, MAX_FOOD), player.food.saturationLevel)
            player.connection.syncHealth()
        }

    override var gamemode: String
        get() = session?.gamemode?.name?.lowercase() ?: "survival"
        set(value) {
            val player = session ?: return
            val mode = Gamemodes.of(value) ?: return
            player.connection.setGamemode(mode)
        }

    override var level: Int
        get() = session?.experience?.level ?: 0
        set(value) {
            val player = session ?: return
            player.experience.setLevels(value)
            player.connection.syncExperience()
        }

    override val world: String get() = session?.worldName.orEmpty()
    override val x: Double get() = session?.x ?: 0.0
    override val y: Double get() = session?.y ?: 0.0
    override val z: Double get() = session?.z ?: 0.0

    // No latency is measured yet, so this answers zero rather than a number that means nothing.
    override val ping: Int get() = 0
    override val sneaking: Boolean get() = session?.sneaking == true

    override val heldItem: String?
        get() {
            val player = session ?: return null
            val slot = org.basaltmc.server.runtime.player.PlayerInventory.HOTBAR_START + player.inventory.heldSlot
            return player.inventory[slot]?.id
        }

    override fun send(message: String) {
        session?.connection?.sendPluginText(MiniMessage.parse(message), actionBar = false)
    }

    override fun actionBar(message: String) {
        session?.connection?.sendPluginText(MiniMessage.parse(message), actionBar = true)
    }

    override fun teleport(
        world: String,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val player = session ?: return false
        // Through the same two paths a command uses, so a world change is a world change and a
        // move within one is a move: `player.teleport` fires either way.
        if (world != player.worldName) {
            val target = worldFor(world) ?: return false
            return player.connection.switchWorld(target, x, y, z)
        }
        player.connection.teleportTo(x, y, z, player.yaw, player.pitch, cause = "plugin")
        return true
    }

    override fun kick(reason: String) {
        session?.connection?.disconnect(MiniMessage.parse(reason))
    }

    // The server's own groups, so a plugin does not carry a second permission system.
    override fun may(permission: String): Boolean {
        val player = session ?: return false
        return groups().resolve(player.group, permission)
    }

    override fun give(
        item: String,
        count: Int,
    ): Boolean {
        val player = session ?: return false
        if (count <= 0) return false
        val left = player.inventory.add(StoredItemStack(item, count))
        player.connection.sendInventoryAll()
        return left == 0
    }

    override val inventory: PluginInventory = SessionInventoryRef { session }

    override fun attribute(name: String): Double? {
        if (!AttributeIds.exists(name)) return null
        val player = session ?: return null
        val qualified = if (':' in name) name else "minecraft:$name"
        player.attributeBases[qualified]?.let { return it }
        // Not set by anybody, so the vanilla default for a player: the only one this build keeps its
        // own number for is health, and saying 0 for the rest would be a lie.
        return if (qualified == MAX_HEALTH_ID) player.maxHealth.toDouble() else null
    }

    override fun setAttribute(
        name: String,
        value: Double,
    ): Boolean {
        if (!AttributeIds.exists(name)) return false
        val player = session ?: return false
        val qualified = if (':' in name) name else "minecraft:$name"
        player.attributeBases[qualified] = value
        // The one this server acts on itself. The rest are sent and the client honours what it can.
        if (qualified == MAX_HEALTH_ID) {
            player.maxHealth = value.toFloat()
            player.connection.syncHealth()
        }
        player.connection.syncHeldAttributes()
        return true
    }

    override fun attributes(): List<String> = AttributeIds.all()

    override fun title(
        title: String,
        subtitle: String,
        fadeIn: Int,
        stay: Int,
        fadeOut: Int,
    ) {
        val player = session ?: return
        // The timings first: sent after the text, the client applies them to the *next* title.
        player.connection.sendPlayPacket(SetTitlesAnimationPacket.ID) {
            SetTitlesAnimationPacket(fadeIn, stay, fadeOut).write(it)
        }
        if (subtitle.isNotEmpty()) {
            player.connection.sendPlayPacket(SetSubtitleTextPacket.ID) {
                SetSubtitleTextPacket(MiniMessage.parse(subtitle)).write(it)
            }
        }
        player.connection.sendPlayPacket(SetTitleTextPacket.ID) {
            SetTitleTextPacket(MiniMessage.parse(title)).write(it)
        }
    }

    override fun sound(
        sound: String,
        volume: Float,
        pitch: Float,
    ) {
        val player = session ?: return
        player.connection.sendPlayPacket(SoundPacket.ID) {
            SoundPacket(
                sound = sound,
                source = SoundSource.MASTER,
                x = player.x,
                y = player.y,
                z = player.z,
                volume = volume,
                pitch = pitch,
                seed = 0L,
            ).write(it)
        }
    }

    override fun particle(
        particle: String,
        x: Double,
        y: Double,
        z: Double,
        count: Int,
        spread: Double,
    ) {
        val player = session ?: return
        val type = ParticleTypes[Identifier.parse(particle)] ?: return
        player.connection.sendPlayPacket(LevelParticlesPacket.ID) {
            LevelParticlesPacket(
                particleTypeId = type.protocolId,
                x = x,
                y = y,
                z = z,
                offsetX = spread.toFloat(),
                offsetY = spread.toFloat(),
                offsetZ = spread.toFloat(),
                count = count,
            ).write(it)
        }
    }

    private companion object {
        const val MAX_FOOD = 20
        const val MAX_HEALTH_ID = "minecraft:max_health"
    }
}

/** Gamemode by the name a plugin writes, since the enum is not the API's to expose. */
internal object Gamemodes {
    fun of(name: String): org.basaltmc.wire.protocol.play.Gamemode? =
        org.basaltmc.wire.protocol.play.Gamemode.entries
            .firstOrNull { it.name.equals(name, ignoreCase = true) }
}

internal class RegistryPlayers(
    private val registry: PlayerRegistry,
    private val groups: () -> org.basaltmc.server.runtime.config.GroupsConfig,
    private val worldFor: (String) -> LoadedWorld?,
) : PluginPlayers {
    private fun refFor(session: PlayerSession) =
        SessionPlayerRef(registry, session.uuid.toString(), session.name, groups, worldFor)

    override fun online(): List<PluginPlayerRef> = registry.snapshot().map(::refFor)

    override fun byName(name: String): PluginPlayerRef? =
        registry.snapshot().firstOrNull { it.name.equals(name, ignoreCase = true) }?.let(::refFor)

    override fun byUuid(uuid: String): PluginPlayerRef? =
        registry.snapshot().firstOrNull { it.uuid.toString() == uuid }?.let(::refFor)
}

internal class WorldEntityRef(
    private val loadedWorld: LoadedWorld,
    private val entityId: Int,
    private val typeName: String,
) : PluginEntityRef {
    private val entity: ServerEntity? get() = loadedWorld.state.entity(entityId)?.takeIf { !it.removed }

    override val id: Int get() = entityId
    override val type: String get() = entity?.type?.toString() ?: typeName
    override val world: String get() = loadedWorld.name
    override val x: Double get() = entity?.x ?: 0.0
    override val y: Double get() = entity?.y ?: 0.0
    override val z: Double get() = entity?.z ?: 0.0
    override val alive: Boolean get() = entity != null

    override var health: Double
        get() = entity?.health ?: 0.0
        set(value) {
            entity?.health = value
        }

    override fun remove() {
        entity?.removed = true
    }

    override fun teleport(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val target = entity ?: return false
        target.x = x
        target.y = y
        target.z = z
        return true
    }
}

internal class LoadedWorldRef(
    private val world: LoadedWorld,
    private val spawnMob: (LoadedWorld, Identifier, Double, Double, Double) -> ServerEntity?,
    /** Where an NPC goes when a plugin makes one. Null before there is a server to put it in. */
    private val npcs: NpcRegistry? = null,
    private val nextEntityId: (() -> Int)? = null,
    /** Who is connected, for an NPC that wants to wear somebody's skin. */
    private val players: () -> List<PlayerSession> = { emptyList() },
) : PluginWorldRef {
    override val name: String get() = world.name
    override val time: Long get() = world.clock.totalTicks
    override val raining: Boolean get() = world.weather.raining
    override val thundering: Boolean get() = world.weather.thundering
    override val difficulty: String get() = world.difficulty

    override fun gameRule(name: String): String? = GameRuleRegistry.byId(name)?.let { world.gameRules.serialized(it) }

    override fun setGameRule(
        name: String,
        value: String,
    ): Boolean {
        val rule = GameRuleRegistry.byId(name) ?: return false
        // Typed on the way in, so a rule cannot be left holding something `/gamerule` would refuse.
        val fits =
            when (rule) {
                is GameRule.Bool -> value.toBooleanStrictOrNull() != null
                is GameRule.Integer -> value.toIntOrNull() != null
            }
        if (!fits) return false
        world.gameRules.set(rule, value)
        return true
    }

    override fun gameRules(): List<String> = GameRuleRegistry.ALL.map { it.id }

    override fun block(
        x: Int,
        y: Int,
        z: Int,
    ): String? = world.state.getBlockState(x, y, z)?.let { BlockStateDefinitions.blockOf(it)?.toString() }

    override fun setBlock(
        x: Int,
        y: Int,
        z: Int,
        block: String,
    ): Boolean {
        val stateId = Blocks.REGISTRY.idOf(Identifier.parse(block)) ?: return false
        return world.state.setBlockState(x, y, z, stateId)
    }

    override fun entities(): List<PluginEntityRef> =
        world.state
            .entities()
            .filter { !it.removed }
            .map { WorldEntityRef(world, it.entityId, it.type.toString()) }

    override fun spawn(
        type: String,
        x: Double,
        y: Double,
        z: Double,
    ): PluginEntityRef? {
        val identifier = Identifier.parse(type)
        if (EntityTypes[identifier] == null) return null
        val spawned = spawnMob(world, identifier, x, y, z) ?: return null
        return WorldEntityRef(world, spawned.entityId, spawned.type.toString())
    }

    @OptIn(kotlin.uuid.ExperimentalUuidApi::class)
    override fun npc(
        name: String,
        x: Double,
        y: Double,
        z: Double,
        onClick: ((String) -> Unit)?,
    ): PluginNpcRef? {
        val registry = npcs ?: return null
        val allocate = nextEntityId ?: return null
        val npc =
            Npc(
                entityId = allocate(),
                uuid =
                    kotlin.uuid.Uuid
                        .random(),
                // Nameless on purpose: what is read over its head is the display made below.
                name = "",
                world = world.name,
                x = x,
                y = y,
                z = z,
            )
        registry.add(npc, onClick)
        val label = name.takeIf { it.isNotEmpty() }?.let { hologram(it, x, y + NpcHandle.LABEL_HEIGHT, z) }
        return NpcHandle(npc, registry, ::texturesOf, label)
    }

    override fun hologram(
        text: String,
        x: Double,
        y: Double,
        z: Double,
    ): PluginHologramRef? {
        val spawned = spawnMob(world, TEXT_DISPLAY, x, y, z) ?: return null
        spawned.displayText = text
        return HologramRef(world, spawned)
    }

    /** The textures of a connected player, which is where an NPC's face comes from. */
    private fun texturesOf(playerName: String): Pair<String, String?>? =
        players()
            .firstOrNull { it.name.equals(playerName, ignoreCase = true) }
            ?.profileProperties
            ?.firstOrNull { it.name == "textures" }
            ?.let { it.value to it.signature }

    private companion object {
        val TEXT_DISPLAY = Identifier.vanilla("text_display")
    }
}

/** Floating text, through the same entity the client draws. */
internal class HologramRef(
    loadedWorld: LoadedWorld,
    private val entity: ServerEntity,
) : PluginHologramRef,
    PluginEntityRef by WorldEntityRef(loadedWorld, entity.entityId, entity.type.toString()) {
    override var text: String
        get() = entity.displayText.orEmpty()
        set(value) {
            entity.displayText = value
        }

    override fun showOnlyTo(uuids: List<String>) {
        entity.visibleTo = uuids.toSet()
    }

    override fun showToAll() {
        entity.visibleTo = null
    }

    override val audience: List<String>? get() = entity.visibleTo?.toList()
}

internal class LoadedWorlds(
    private val worlds: () -> Map<String, LoadedWorld>,
    private val defaultName: () -> String,
    private val spawnMob: (LoadedWorld, Identifier, Double, Double, Double) -> ServerEntity?,
    /** Making one, which only the server can do: it owns the folder, the seed and the scheduler. */
    private val make: (String, WorldDimension, WorldType, Long?) -> LoadedWorld? = { _, _, _, _ -> null },
    private val npcs: NpcRegistry? = null,
    private val nextEntityId: (() -> Int)? = null,
    private val players: () -> List<PlayerSession> = { emptyList() },
) : PluginWorlds {
    private fun refFor(world: LoadedWorld) = LoadedWorldRef(world, spawnMob, npcs, nextEntityId, players)

    override fun all(): List<PluginWorldRef> = worlds().values.map(::refFor)

    override fun byName(name: String): PluginWorldRef? = worlds()[name]?.let(::refFor)

    override val default: PluginWorldRef? get() = byName(defaultName())

    override fun create(
        name: String,
        dimension: String,
        type: String,
        seed: Long?,
    ): PluginWorldRef? {
        // Named rather than numbered, and refused rather than guessed: a typo that quietly made an
        // overworld where a nether was asked for is a world somebody has to delete by hand.
        val theDimension =
            WorldDimension.entries.firstOrNull { it.name.equals(dimension.removePrefix("minecraft:"), true) }
                ?: return null
        val theType = WorldType.entries.firstOrNull { it.name.equals(type, ignoreCase = true) } ?: return null
        return make(name, theDimension, theType, seed)?.let(::refFor)
    }
}

/**
 * A plugin's placeholders, in the server's own registry.
 *
 * Registering here is what makes `%arena_wins%` work in the MOTD and the sidebar without those
 * knowing a plugin exists. The prefix is the plugin's id, so two of them cannot collide by accident
 * and a collision within one is refused rather than silently replacing what was there.
 */
internal class RegistryPlaceholders(
    private val registry: PlaceholderRegistry,
    private val players: PluginPlayers,
) : PluginPlaceholders {
    override fun register(
        pluginId: String,
        name: String,
        value: (PluginPlayerRef?) -> String,
    ): org.basaltmc.server.plugins.PluginHandle {
        // `%arena_wins%`: the plugin's id is the identifier and the rest is the parameter, which is
        // how the registry already splits every other one.
        val identifier = "${pluginId}_$name"
        registry.register(identifier) { _, context ->
            val player = context[PlaceholderSlots.PLAYER]?.let { players.byName(it.name) }
            PlaceholderValue.of(value(player))
        }
        return org.basaltmc.server.plugins
            .PluginHandle { registry.unregister(identifier) }
    }

    override fun fill(
        text: String,
        player: PluginPlayerRef?,
    ): String {
        val session = player?.name?.let { name -> sessions().firstOrNull { it.name == name } }
        val context = PlaceholderContext.EMPTY.with(PlaceholderSlots.PLAYER, session)
        return PlaceholderText.substitute(text, registry, context)
    }

    /** Set by the server: rendering for a player needs the session the slot is typed on. */
    var sessions: () -> List<PlayerSession> = { emptyList() }
}

/** MiniMessage for anything a plugin writes, so the caller never assembles a Component. */
internal fun pluginText(message: String): Component = MiniMessage.parse(message)

/**
 * A plugin's data, in `players.db`, with the plugin's own id on every row.
 *
 * The isolation is the shape of the calls rather than a check inside them: there is no way to ask
 * this for another plugin's namespace, because every query takes the caller's id and filters on it.
 * A plugin never sees a connection, a statement or a table name.
 */
internal class DatabasePluginStorage(
    private val store: () -> org.basaltmc.server.storage.PlayerStore?,
) : org.basaltmc.server.plugins.PluginStorage {
    override fun playerValue(
        pluginId: String,
        uuid: String,
        key: String,
    ): String? = store()?.takeIf { it.isOpen }?.pluginData?.get(pluginId, uuid, key)

    override fun setPlayerValue(
        pluginId: String,
        uuid: String,
        key: String,
        value: String?,
    ) {
        store()?.takeIf { it.isOpen }?.pluginData?.set(pluginId, uuid, key, value)
    }

    override fun playerValues(
        pluginId: String,
        uuid: String,
    ): Map<String, String> =
        store()
            ?.takeIf { it.isOpen }
            ?.pluginData
            ?.all(pluginId, uuid)
            .orEmpty()

    override fun top(
        pluginId: String,
        key: String,
        limit: Int,
    ): List<Pair<String, String>> =
        store()
            ?.takeIf { it.isOpen }
            ?.pluginData
            ?.top(pluginId, key, limit)
            .orEmpty()

    override fun namespaces(): Map<String, Long> =
        store()
            ?.takeIf { it.isOpen }
            ?.pluginData
            ?.namespaces()
            .orEmpty()

    override fun purge(pluginId: String): Long {
        val data = store()?.takeIf { it.isOpen }?.pluginData ?: return 0
        val rows = data.namespaces()[pluginId] ?: return 0
        data.purge(pluginId)
        return rows
    }
}

/**
 * Windows a plugin draws, opened on the session of whoever is looking at one.
 *
 * All it does is find the connection: the window itself is the session's business, because that is
 * where the container machinery and the socket already are.
 */
internal class SessionMenus(
    private val registry: PlayerRegistry,
) : PluginMenus {
    private fun sessionOf(player: PluginPlayerRef): PlayerSession? =
        registry.snapshot().firstOrNull { it.uuid.toString() == player.uuid }

    override fun open(
        player: PluginPlayerRef,
        title: String,
        rows: Int,
        items: Map<Int, PluginItem>,
        onClick: (Int) -> Unit,
    ): PluginHandle {
        val session = sessionOf(player) ?: return PluginHandle { }
        session.connection.openPluginMenu(title, rows, items, onClick)
        return PluginHandle { sessionOf(player)?.connection?.closeOpenMenu() }
    }

    override fun close(player: PluginPlayerRef) {
        sessionOf(player)?.connection?.closeOpenMenu()
    }

    override fun refresh(
        player: PluginPlayerRef,
        items: Map<Int, PluginItem>,
    ) {
        sessionOf(player)?.connection?.refreshPluginMenu(items)
    }
}

/**
 * A player's slots, through the same [PlayerInventory] the client is looking at.
 *
 * Every write re-sends the window, because the client keeps its own copy and a slot changed behind
 * its back is a ghost item until the next click. Offline is not an error here: the handle answers
 * an empty inventory and drops the writes, the same way `health` reads 0 rather than throwing.
 */
internal class SessionInventoryRef(
    private val session: () -> PlayerSession?,
) : PluginInventory {
    override val size: Int get() = StoredInventory.PLAYER_SLOTS

    override val heldSlot: Int get() = session()?.inventory?.heldSlot ?: 0

    override fun get(slot: Int): PluginItem? = session()?.inventory?.get(slot)?.let { itemOf(it) }

    override fun set(
        slot: Int,
        item: PluginItem?,
    ) {
        val player = session() ?: return
        player.inventory[slot] = item?.let { storedOf(it) }
        player.connection.sendInventoryAll()
    }

    override fun add(item: PluginItem): Int {
        val player = session() ?: return item.count
        val left = player.inventory.add(storedOf(item))
        player.connection.sendInventoryAll()
        return left
    }

    override fun remove(
        id: String,
        count: Int,
    ): Int {
        val player = session() ?: return 0
        val taken = player.inventory.take(id, count)
        if (taken > 0) player.connection.sendInventoryAll()
        return taken
    }

    override fun count(id: String): Int = session()?.inventory?.countOf(id) ?: 0

    override fun clear() {
        val player = session() ?: return
        player.inventory.clear()
        player.connection.sendInventoryAll()
    }

    private companion object {
        fun itemOf(stack: StoredItemStack): PluginItem =
            PluginItem(
                id = stack.id,
                count = stack.count,
                name = (stack.components[ItemComponentCodecs.CUSTOM_NAME] as? NbtString)?.value,
                damage = (stack.components[ItemComponentCodecs.DAMAGE] as? NbtInt)?.value ?: 0,
                enchantments =
                    PlayerInventory.enchantmentLevels(stack).entries.associate { it.key.toString() to it.value },
            )

        fun storedOf(item: PluginItem): StoredItemStack {
            val components = NbtCompound()
            item.name?.let { components[ItemComponentCodecs.CUSTOM_NAME] = NbtString(it) }
            if (item.damage > 0) components[ItemComponentCodecs.DAMAGE] = NbtInt(item.damage)
            if (item.enchantments.isNotEmpty()) {
                val levels = NbtCompound()
                for ((id, level) in item.enchantments) levels[id] = NbtInt(level)
                components[PlayerInventory.ENCHANTMENTS_COMPONENT] = levels
            }
            return StoredItemStack(item.id, item.count.coerceAtLeast(1), components)
        }
    }
}
