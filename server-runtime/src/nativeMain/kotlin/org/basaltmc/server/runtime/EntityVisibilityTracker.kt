package org.basaltmc.server.runtime

import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.Taming
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.login.GameProfileProperty
import org.basaltmc.wire.protocol.play.EntityPositionSyncPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.NameTagVisibility
import org.basaltmc.wire.protocol.play.PlayerInfoEntry
import org.basaltmc.wire.protocol.play.PlayerInfoUpdatePacket
import org.basaltmc.wire.protocol.play.Pose
import org.basaltmc.wire.protocol.play.RemoveEntitiesPacket
import org.basaltmc.wire.protocol.play.RotateHeadPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEntityLinkPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SetPassengersPacket
import org.basaltmc.wire.protocol.play.SetPlayerTeamPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.MiniMessage
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.floor
import kotlin.math.sqrt

/**
 * What this client has been told to draw: other players, plugin NPCs, dropped items, summoned
 * entities and generated-chunk entities, each spawned once and only re-sent on a real change.
 *
 * A back-reference to the owning session rather than captured values, same as [PacketInterceptor]
 * and [ItemUseTimers]: every method here reads the current [ClientSession.playerSession] and
 * [ClientSession.currentWorld], and sends through the same outbound packets.
 */
internal class EntityVisibilityTracker(
    private val owner: ClientSession,
) {
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

    /** Baby/size metadata last sent for each live entity, including transitions while stationary. */
    private val lastRuntimeEntityMetadata = HashMap<Int, RuntimeEntityMetadata>()

    /** Passenger ids last sent for each visible vehicle. */
    private val lastPassengers = HashMap<Int, List<Int>>()

    /** Leash holder last sent for each visible mob. */
    private val lastLeashHolders = HashMap<Int, Int>()

    /** Which other players this client currently has as entities, kept by [syncPlayers]. */
    private val trackedPlayers = HashSet<Int>()

    /**
     * The flags and pose this player was last drawn with, so only changes go on the wire.
     *
     * They start at what an entity has by default, so a player who joins standing still and unlit
     * costs no packet at all: the client already believes exactly that.
     */
    internal var lastSharedFlags = 0
    internal var lastPose = Pose.STANDING

    /** What the client was last told its level was, so a change is a change rather than a resend. */
    internal var lastSyncedLevel: Int = -1

    /** What each of them was last seen holding and wearing, so only changes go on the wire. */
    private val trackedEquipment = HashMap<Int, List<StoredItemStack?>>()

    /** Static entities emitted by world generation that this client has been told about. */
    private val trackedGeneratedEntities = HashSet<Int>()

    /** Which of those have already had their final resting position sent. */
    private val settledItems = HashSet<Int>()

    /**
     * Brings this client's view of summoned entities up to date: spawns what it has not seen,
     * moves what has been moved by a command, and removes what was killed.
     *
     * Same shape as [syncItemEntities] and for the same reason: the world owns the entity, each
     * connection reports it on its own thread.
     */
    internal fun syncSummonedEntities() {
        val session = owner.playerSession ?: return
        val me = session.uuid.toString()
        val entities = owner.world.entities()
        for (entity in entities) {
            if (entity.removed) continue
            // An entity somebody else can see and this player cannot: taken away if they were
            // already shown it, and skipped if they were not.
            val visible = entity.visibleTo?.contains(me) ?: true
            if (!visible) {
                if (trackedEntities.remove(entity.entityId)) {
                    lastEntityPositions.remove(entity.entityId)
                    lastRuntimeEntityMetadata.remove(entity.entityId)
                    lastPassengers.remove(entity.entityId)
                    lastLeashHolders.remove(entity.entityId)
                    owner.sendPlayPacket(RemoveEntitiesPacket.ID) {
                        RemoveEntitiesPacket(listOf(entity.entityId)).write(it)
                    }
                }
                continue
            }
            if (trackedEntities.add(entity.entityId)) {
                if (Taming.isTamable(entity.type) && Taming.flagsOf(entity) != 0) {
                    owner.sendPlayPacket(SetEntityDataPacket.ID) {
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
                owner.sendPlayPacket(SpawnEntityPacket.ID) {
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
                val liveMetadata = runtimeEntityMetadata(entity)
                lastRuntimeEntityMetadata[entity.entityId] = liveMetadata
                val metadata =
                    (
                        entity.sourceNbt?.let { generatedEntityMetadata(entity.type.path, it) }.orEmpty() +
                            liveMetadata.changesFrom(null)
                    ).associateBy { it.index }.values.toList()
                if (metadata.isNotEmpty()) {
                    owner.sendPlayPacket(SetEntityDataPacket.ID) {
                        SetEntityDataPacket(entity.entityId, metadata).write(it)
                    }
                }
                entity.sourceNbt?.let { data ->
                    val equipment =
                        generatedEntityEquipment(data).mapNotNull { slot ->
                            val item =
                                Items.REGISTRY.byIdentifier(Identifier.parse(slot.item)) ?: return@mapNotNull null
                            SetEquipmentPacket.Slot(
                                slot.slot,
                                WireItemStack(
                                    item.protocolId,
                                    slot.count,
                                    ItemComponentCodecs.toWire(slot.components, owner.enchantmentIds ?: { null }),
                                ),
                            )
                        }
                    if (equipment.isNotEmpty()) {
                        owner.sendPlayPacket(SetEquipmentPacket.ID) {
                            SetEquipmentPacket(entity.entityId, equipment).write(it)
                        }
                    }
                }
                entity.displayText?.let { text -> sendDisplayText(entity.entityId, text) }
                syncLeash(entity)
                continue
            }
            val liveMetadata = runtimeEntityMetadata(entity)
            val metadataChanges = liveMetadata.changesFrom(lastRuntimeEntityMetadata[entity.entityId])
            lastRuntimeEntityMetadata[entity.entityId] = liveMetadata
            if (metadataChanges.isNotEmpty()) {
                owner.sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket(entity.entityId, metadataChanges).write(it)
                }
            }
            syncLeash(entity)
            // parity: `ServerEntity.sendChanges`, which sends nothing at all for an entity that has
            // not moved. A pen of animals standing still is free to watch; without this every one of
            // them costs a packet per player per tick.
            val last = lastEntityPositions[entity.entityId]
            val moved =
                last == null ||
                    abs(last[0] - entity.x) > ClientSession.POSITION_EPSILON ||
                    abs(last[1] - entity.y) > ClientSession.POSITION_EPSILON ||
                    abs(last[2] - entity.z) > ClientSession.POSITION_EPSILON ||
                    abs(last[3] - entity.yaw.toDouble()) > ClientSession.ROTATION_EPSILON ||
                    abs(last[4] - entity.headYaw.toDouble()) > ClientSession.ROTATION_EPSILON
            if (!moved) continue
            // parity: `ClientboundRotateHeadPacket`, which is the only thing that turns a mob's head.
            if (last == null || abs(last[4] - entity.headYaw.toDouble()) > ClientSession.ROTATION_EPSILON) {
                owner.sendPlayPacket(RotateHeadPacket.ID) {
                    RotateHeadPacket(entity.entityId, entity.headYaw).write(it)
                }
            }
            lastEntityPositions[entity.entityId] =
                doubleArrayOf(entity.x, entity.y, entity.z, entity.yaw.toDouble(), entity.headYaw.toDouble())
            // parity: the position *sync*, which carries the velocity so the client can interpolate:
            // a teleport every tick snaps, which is what made a falling block judder down.
            owner.sendPlayPacket(EntityPositionSyncPacket.ID) {
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
        syncPassengers(entities, me, session.entityId)
        val gone = owner.world.collectRemovedEntities().filter { trackedEntities.remove(it) }
        for (entityId in gone) {
            lastEntityPositions.remove(entityId)
            lastRuntimeEntityMetadata.remove(entityId)
            lastPassengers.remove(entityId)
            lastLeashHolders.remove(entityId)
        }
        if (gone.isNotEmpty()) {
            owner.sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
        }
    }

    private fun syncPassengers(
        entities: List<ServerEntity>,
        viewer: String,
        viewerEntityId: Int,
    ) {
        val visibleEntityIds =
            entities
                .filter { !it.removed && (it.visibleTo?.contains(viewer) ?: true) }
                .mapTo(hashSetOf()) { it.entityId }
        for (vehicle in entities) {
            if (vehicle.entityId !in trackedEntities || vehicle.entityId !in visibleEntityIds) continue
            val passengers =
                vehicle.passengerIds.filter { passengerId ->
                    passengerId == viewerEntityId ||
                        passengerId in trackedPlayers ||
                        (passengerId in trackedEntities && passengerId in visibleEntityIds)
                }
            val previous = lastPassengers[vehicle.entityId]
            if (previous == passengers) continue
            if (previous != null || passengers.isNotEmpty()) {
                owner.sendPlayPacket(SetPassengersPacket.ID) {
                    SetPassengersPacket(vehicle.entityId, passengers).write(it)
                }
            }
            if (passengers.isEmpty()) {
                lastPassengers.remove(vehicle.entityId)
            } else {
                lastPassengers[vehicle.entityId] = passengers
            }
        }
        lastPassengers.keys.removeAll { vehicleId ->
            vehicleId !in trackedEntities || vehicleId !in visibleEntityIds
        }
    }

    private fun syncLeash(entity: ServerEntity) {
        val holder = entity.leashHolderId
        val previous = lastLeashHolders[entity.entityId]
        if (previous == holder) return
        if (previous != null || holder != null) {
            owner.sendPlayPacket(SetEntityLinkPacket.ID) {
                SetEntityLinkPacket(entity.entityId, holder ?: SetEntityLinkPacket.NO_VEHICLE).write(it)
            }
        }
        if (holder == null) lastLeashHolders.remove(entity.entityId) else lastLeashHolders[entity.entityId] = holder
    }

    /**
     * The profile name an NPC travels under, which nobody ever reads.
     *
     * A player entity must have one, and it must be a plain string of sixteen characters, so it
     * cannot be the name a plugin wrote: that is the display floating above. This is a made-up name
     * whose only job is to be the handle the hiding team joins by, so it has to be unique and it has
     * to be exactly what the team is told.
     */
    private fun npcProfileName(entityId: Int): String = "npc_$entityId".take(ClientSession.PROFILE_NAME_LIMIT)

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
        owner.sendPlayPacket(SetEntityDataPacket.ID) {
            SetEntityDataPacket(
                entityId,
                listOf(
                    SetEntityDataPacket.Entry.Text(ClientSession.TEXT_DISPLAY_TEXT_INDEX, MiniMessage.parse(text)),
                    // Turned to face whoever is reading it. A display left as it spawned is drawn
                    // flat in the world, so from the other side it reads backwards.
                    SetEntityDataPacket.Entry.Bytes(
                        ClientSession.DISPLAY_BILLBOARD_INDEX,
                        ClientSession.BILLBOARD_CENTER,
                    ),
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
    internal fun syncPlayers() {
        val session = owner.playerSession ?: return
        val registry = owner.playerRegistry ?: return
        val visible = registry.snapshot().filter { it.uuid != session.uuid && it.worldName == session.worldName }

        for (other in visible) {
            if (!trackedPlayers.add(other.entityId)) continue
            // parity: the tab-list entry always precedes the entity. The client matches the two by
            // profile uuid to find the skin, and an entity that arrives first is drawn bald and
            // grey. Sending it again for someone already listed costs nothing and removes the race.
            owner.sendPlayPacket(PlayerInfoUpdatePacket.ID) { buffer ->
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
            owner.sendPlayPacket(SpawnEntityPacket.ID) { buffer -> spawnPlayerPacket(other).write(buffer) }
            owner.sendPlayPacket(SetEntityDataPacket.ID) { buffer ->
                SetEntityDataPacket.skinParts(other.entityId, other.skinParts).write(buffer)
            }
            owner.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(other.entityId, other.x, other.y, other.z, other.yaw, other.pitch, other.onGround)
                    .write(it)
            }
            owner.sendPlayPacket(RotateHeadPacket.ID) { RotateHeadPacket(other.entityId, other.yaw).write(it) }
        }

        for (other in visible) syncEquipmentOf(other)

        val stillThere = visible.map { it.entityId }.toSet()
        val gone = trackedPlayers.filter { it !in stillThere }
        if (gone.isEmpty()) return
        trackedPlayers.removeAll(gone.toSet())
        for (entityId in gone) trackedEquipment.remove(entityId)
        owner.sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
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

    /** Whether the head moved enough to be worth a packet: a degree of jitter is not. */
    private fun lookedAt(
        entityId: Int,
        yaw: Float,
        pitch: Float,
    ): Boolean {
        val last = lastNpcLook[entityId]
        if (last != null &&
            abs(last.first - yaw) < ClientSession.LOOK_STEP &&
            abs(last.second - pitch) < ClientSession.LOOK_STEP
        ) {
            return false
        }
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
            owner.sendPlayPacket(SetPlayerTeamPacket.ID) {
                SetPlayerTeamPacket(
                    teamName = ClientSession.NAMELESS_TEAM,
                    method = SetPlayerTeamPacket.METHOD_ADD,
                    parameters =
                        SetPlayerTeamPacket.Parameters(
                            displayName = Component.text(ClientSession.NAMELESS_TEAM),
                            nameTagVisibility = NameTagVisibility.NEVER,
                        ),
                ).write(it)
            }
        }
        owner.sendPlayPacket(SetPlayerTeamPacket.ID) {
            SetPlayerTeamPacket(
                teamName = ClientSession.NAMELESS_TEAM,
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
    internal fun forgetEverythingDrawn() {
        trackedEntities.clear()
        trackedPlayers.clear()
        trackedNpcs.clear()
        namelessTeamMade = false
        lastNpcLook.clear()
        trackedEquipment.clear()
        lastEntityPositions.clear()
        lastRuntimeEntityMetadata.clear()
        lastPassengers.clear()
    }

    /**
     * Draws the NPCs a plugin made, the same way [syncPlayers] draws real ones.
     *
     * Same shape on purpose: an NPC is a player entity, so it needs the tab-list entry first (that
     * is where the skin lives) and the entity second. `listed = false` keeps it out of the tab list
     * while still giving the client the profile it needs to draw a face.
     */
    internal fun syncNpcs() {
        val session = owner.playerSession ?: return
        val registry = owner.npcs ?: return
        val me = session.uuid.toString()
        val here =
            registry.all().filter { it.world == session.worldName && (it.visibleTo?.contains(me) ?: true) }
        for (npc in here) {
            val known = trackedNpcs[npc.entityId]
            if (known == npc.version) continue
            if (known != null) {
                // Something a client was already told changed: taken away and drawn again, which is
                // what the profile requires anyway since a skin cannot be changed in place.
                owner.sendPlayPacket(RemoveEntitiesPacket.ID) {
                    RemoveEntitiesPacket(listOf(npc.entityId)).write(it)
                }
            }
            trackedNpcs[npc.entityId] = npc.version
            // Before the entity: the tag is hidden by the team this name belongs to, and a tag
            // drawn before the team arrives flashes on screen.
            hideNameTagOf(npcProfileName(npc.entityId))
            val properties =
                npc.skinTexture?.let { texture ->
                    listOf(GameProfileProperty("textures", texture, npc.skinSignature))
                } ?: emptyList()
            owner.sendPlayPacket(PlayerInfoUpdatePacket.ID) { buffer ->
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
            owner.sendPlayPacket(SpawnEntityPacket.ID) { buffer ->
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
            owner.sendPlayPacket(RotateHeadPacket.ID) { RotateHeadPacket(npc.entityId, npc.yaw).write(it) }
            // Every layer of the skin on, or an NPC is drawn without its jacket and hat.
            owner.sendPlayPacket(SetEntityDataPacket.ID) { buffer ->
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
            if (flat > ClientSession.NPC_LOOK_RANGE * ClientSession.NPC_LOOK_RANGE) continue
            val yaw = (atan2(dz, dx) * ClientSession.RADIANS_TO_DEGREES - 90.0).toFloat()
            val pitch = (-atan2(dy, sqrt(flat)) * ClientSession.RADIANS_TO_DEGREES).toFloat()
            if (!lookedAt(npc.entityId, yaw, pitch)) continue
            owner.sendPlayPacket(RotateHeadPacket.ID) { RotateHeadPacket(npc.entityId, yaw).write(it) }
            owner.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(npc.entityId, npc.x, npc.y, npc.z, yaw, pitch, onGround = true).write(it)
            }
        }

        val visible = here.map { it.entityId }.toSet()
        val gone = trackedNpcs.keys.filterNot { it in visible }
        if (gone.isEmpty()) return
        for (entityId in gone) trackedNpcs.remove(entityId)
        owner.sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
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
            ClientSession.EQUIPMENT_SLOTS.mapIndexed { index, type ->
                SetEquipmentPacket.Slot(type, other.inventory.toWire(worn[index]))
            }
        owner.sendPlayPacket(SetEquipmentPacket.ID) { SetEquipmentPacket(other.entityId, slots).write(it) }
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
    internal fun syncItemEntities() {
        val session = owner.playerSession ?: return
        val visible = HashSet<Int>()
        for (item in owner.world.items()) {
            val dx = item.x - session.x
            val dz = item.z - session.z
            if (dx * dx + dz * dz > ClientSession.ITEM_VIEW_RADIUS_SQUARED) continue
            val protocolId = Items.REGISTRY.byIdentifier(item.item)?.protocolId ?: continue
            visible += item.entityId
            if (trackedItems.add(item.entityId)) {
                trackedItemCounts[item.entityId] = item.count
                owner.sendPlayPacket(SpawnEntityPacket.ID) {
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
                owner.sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket.item(item.entityId, WireItemStack(protocolId, item.count)).write(it)
                }
            } else if (item.countChanged && trackedItemCounts[item.entityId] != item.count) {
                // A merge changed the stack: the client draws the number it was told, so it has to
                // hear the new one or two logs on the floor keep looking like one.
                trackedItemCounts[item.entityId] = item.count
                owner.sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket.item(item.entityId, WireItemStack(protocolId, item.count)).write(it)
                }
            } else if (item.resting && settledItems.add(item.entityId)) {
                // One correction when it stops, and nothing while it falls: the client simulates
                // the item from the velocity it was spawned with, and a teleport every tick fights
                // that simulation on a different clock, which shows up as a stutter.
                owner.sendPlayPacket(EntityTeleportPacket.ID) {
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
        owner.sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
    }

    /** Spawns immutable entities emitted by loaded generated chunks and village templates. */
    internal fun syncGeneratedEntities() {
        val session = owner.playerSession ?: return
        val registry = owner.playerRegistry ?: return
        val visible = HashSet<Int>()
        val entities =
            owner.world.generatedEntities(
                registry::allocateEntityId,
                centerChunkX = floor(session.x).toInt() shr 4,
                centerChunkZ = floor(session.z).toInt() shr 4,
                radiusChunks = ClientSession.GENERATED_ENTITY_VIEW_RADIUS_CHUNKS,
            )
        for (entity in entities) {
            val descriptor = entity.descriptor
            val dx = descriptor.x - session.x
            val dz = descriptor.z - session.z
            if (dx * dx + dz * dz > ClientSession.GENERATED_ENTITY_VIEW_RADIUS_SQUARED) continue
            val typeId = ClientSession.GENERATED_ENTITY_TYPE_IDS[descriptor.type.path] ?: continue
            visible += entity.entityId
            if (!trackedGeneratedEntities.add(entity.entityId)) continue
            val isFrame = descriptor.type.path == "item_frame"
            owner.sendPlayPacket(SpawnEntityPacket.ID) {
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
                    objectData = if (isFrame) owner.frameDirectionData(descriptor.data.getString("facing")) else 0,
                ).write(it)
            }
            if (isFrame && descriptor.data.getString("item") == "minecraft:elytra") {
                val elytra = Items.REGISTRY.byIdentifier(Identifier.vanilla("elytra")) ?: continue
                owner.sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket.itemFrame(entity.entityId, WireItemStack(elytra.protocolId, 1)).write(it)
                }
            }
            val metadata = generatedEntityMetadata(descriptor.type.path, descriptor.data)
            if (metadata.isNotEmpty()) {
                owner.sendPlayPacket(SetEntityDataPacket.ID) {
                    SetEntityDataPacket(entity.entityId, metadata).write(it)
                }
            }
            val equipment =
                generatedEntityEquipment(descriptor.data).mapNotNull { slot ->
                    val item = Items.REGISTRY.byIdentifier(Identifier.parse(slot.item)) ?: return@mapNotNull null
                    SetEquipmentPacket.Slot(
                        slot.slot,
                        WireItemStack(
                            item.protocolId,
                            slot.count,
                            ItemComponentCodecs.toWire(slot.components, owner.enchantmentIds ?: { null }),
                        ),
                    )
                }
            if (equipment.isNotEmpty()) {
                owner.sendPlayPacket(SetEquipmentPacket.ID) {
                    SetEquipmentPacket(entity.entityId, equipment).write(it)
                }
            }
        }

        val gone = trackedGeneratedEntities.filterNot { it in visible }
        if (gone.isEmpty()) return
        trackedGeneratedEntities.removeAll(gone.toSet())
        owner.sendPlayPacket(RemoveEntitiesPacket.ID) { RemoveEntitiesPacket(gone).write(it) }
    }
}
