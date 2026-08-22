package org.basaltmc.server.runtime.plugins

import org.basaltmc.server.plugins.PluginNpcRef
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * A player-shaped entity a plugin put in the world.
 *
 * A real player entity: the client draws it because it was told about a player, with a profile and
 * a skin, not because of any trick. That is the whole reason this is a handful of fields rather than
 * a library: being the server means there is nothing to fake and no client version to detect.
 */
@OptIn(ExperimentalUuidApi::class)
public class Npc(
    public val entityId: Int,
    public val uuid: Uuid,
    public var name: String,
    public val world: String,
    public var x: Double,
    public var y: Double,
    public var z: Double,
) {
    public var yaw: Float = 0f
    public var pitch: Float = 0f

    /** The skin, as the two strings a profile property carries. Null draws the default skin. */
    public var skinTexture: String? = null
    public var skinSignature: String? = null

    /** Who can see it, by uuid, or null for everybody. Same rule as a hologram's. */
    public var visibleTo: Set<String>? = null

    /** Bumped whenever something a client has already been told changes, so the sync re-sends it. */
    public var version: Int = 0
        private set

    public var removed: Boolean = false
        private set

    public fun changed() {
        version++
    }

    public fun remove() {
        removed = true
    }
}

/**
 * Every NPC on the server, and who is allowed to be told about them.
 *
 * One registry rather than one per world because a connection asks it once per tick and a map of
 * maps would only move the filtering around.
 */
public class NpcRegistry {
    private val npcs = LinkedHashMap<Int, Npc>()
    private val clicks = LinkedHashMap<Int, (String) -> Unit>()

    public fun add(
        npc: Npc,
        onClick: ((String) -> Unit)? = null,
    ) {
        npcs[npc.entityId] = npc
        onClick?.let { clicks[npc.entityId] = it }
    }

    public fun remove(entityId: Int) {
        npcs.remove(entityId)
        clicks.remove(entityId)
    }

    public fun all(): List<Npc> = npcs.values.filterNot { it.removed }

    public fun of(entityId: Int): Npc? = npcs[entityId]?.takeIf { !it.removed }

    /** Answers whether anybody was listening, so a click on an NPC is not also a click on nothing. */
    public fun clicked(
        entityId: Int,
        playerUuid: String,
    ): Boolean {
        val handler = clicks[entityId] ?: return npcs.containsKey(entityId)
        handler(playerUuid)
        return true
    }

    /** Forgets the ones that were removed, after the connections have been told. */
    public fun sweep() {
        val gone = npcs.values.filter { it.removed }.map { it.entityId }
        for (entityId in gone) {
            npcs.remove(entityId)
            clicks.remove(entityId)
        }
    }

    public fun removedSince(known: Set<Int>): List<Int> = known.filterNot { npcs[it]?.removed == false }
}

/** What a plugin holds onto after making one. */
@OptIn(ExperimentalUuidApi::class)
internal class NpcHandle(
    private val npc: Npc,
    private val registry: NpcRegistry,
) : PluginNpcRef {
    override val id: Int get() = npc.entityId
    override val uuid: String get() = npc.uuid.toString()
    override val world: String get() = npc.world
    override val x: Double get() = npc.x
    override val y: Double get() = npc.y
    override val z: Double get() = npc.z
    override val alive: Boolean get() = registry.of(npc.entityId) != null

    override var name: String
        get() = npc.name
        set(value) {
            npc.name = value
            npc.changed()
        }

    override fun skin(
        texture: String,
        signature: String?,
    ) {
        npc.skinTexture = texture
        npc.skinSignature = signature
        npc.changed()
    }

    override fun look(
        yaw: Float,
        pitch: Float,
    ) {
        npc.yaw = yaw
        npc.pitch = pitch
        npc.changed()
    }

    override fun teleport(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        npc.x = x
        npc.y = y
        npc.z = z
        npc.changed()
        return true
    }

    override fun showOnlyTo(uuids: List<String>) {
        npc.visibleTo = uuids.toSet()
        npc.changed()
    }

    override fun showToAll() {
        npc.visibleTo = null
        npc.changed()
    }

    override val audience: List<String>? get() = npc.visibleTo?.toList()

    override fun remove() {
        npc.remove()
    }
}
