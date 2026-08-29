package org.basaltmc.server.runtime.entity

import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtByte
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtLong
import org.basaltmc.wire.nbt.NbtShort
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.deepCopy
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * An entity as it waits on disk between one run and the next.
 *
 * parity: `Entity.saveWithoutId`, in the keys it shares with it (`id`, `Pos`, `Motion`,
 * `Rotation`, `Health`, `Age`) so a future reader of vanilla's own format recognises them. What is
 * only this server's, like a villager's level, keeps its own name.
 */
@OptIn(ExperimentalUuidApi::class)
public object EntityNbt {
    public fun encode(entity: ServerEntity): NbtCompound {
        val tag = entity.sourceNbt?.deepCopy() ?: NbtCompound()
        tag.remove("Passengers")
        tag["id"] = NbtString(entity.type.toString())
        tag["UUID"] = NbtString(entity.uuid.toString())
        tag["Pos"] = doubles(entity.x, entity.y, entity.z)
        tag["Motion"] = doubles(entity.velocityX, entity.velocityY, entity.velocityZ)
        tag["Rotation"] = floats(entity.yaw, entity.pitch)
        tag.putFloat("Health", entity.health.toFloat())
        tag.putInt("Age", entity.age)
        tag.putShort("Fuse", entity.fuse.toShort())
        tag.putShort("Air", entity.airSupply.toShort())
        if (CubeMobs.isCube(entity.type)) tag["Size"] = NbtInt(entity.cubeSize.coerceAtLeast(1) - 1)
        tag.putBoolean("PersistenceRequired", entity.persistent)
        tag.putBoolean("NoAI", entity.noAi)
        tag.putBoolean("CanPickUpLoot", entity.canPickUpLoot)
        tag.putInt("LoveTicks", entity.loveTicks)
        tag.putInt("BreedCooldown", entity.breedCooldown)
        tag["VillagerProfession"] = NbtString(entity.profession)
        tag.putInt("VillagerLevel", entity.villagerLevel)
        tag.putInt("VillagerExperience", entity.villagerExperience)
        if (entity.type.path in PATROLLING_TYPES) {
            entity.patrolTarget?.let { tag.putIntArray("patrol_target", it) } ?: tag.remove("patrol_target")
            tag.putBoolean("PatrolLeader", entity.patrolLeader)
            tag.putBoolean("Patrolling", entity.patrolling)
        }
        if (entity.type.path == "wandering_trader" || entity.type.path == "trader_llama") {
            tag.putInt("DespawnDelay", entity.despawnDelay)
        }
        if (entity.type.path == "wandering_trader") {
            entity.wanderTarget?.let { tag.putIntArray("wander_target", it) } ?: tag.remove("wander_target")
        }
        if (entity.type.path == "skeleton_horse") {
            tag.putBoolean("SkeletonTrap", entity.skeletonTrap)
            tag.putInt("SkeletonTrapTime", entity.skeletonTrapTime)
            tag.putBoolean("Tame", entity.tamedHorse)
        }
        entity.leashHolderUuid?.let { uuid ->
            tag["leash"] = NbtCompound().apply { putString("UUID", uuid) }
        }
        if (entity.type.path == "villager" || entity.type.path == "zombie_villager") {
            val villagerData = tag.getCompound("VillagerData")
            if (villagerData.getString("type").isEmpty()) villagerData.putString("type", "minecraft:plains")
            villagerData.putString("profession", "minecraft:${entity.profession}")
            villagerData.putInt("level", entity.villagerLevel)
            tag["VillagerData"] = villagerData
            tag.putInt("Xp", entity.villagerExperience)
        }
        entity.owner?.let { tag["Owner"] = NbtString(it) }
        tag.putBoolean("Sitting", entity.sitting)
        entity.customName?.let { tag["CustomName"] = NbtString(it) }
        if (entity.tags.isNotEmpty()) {
            tag["Tags"] = NbtList(NbtType.STRING, entity.tags.map { NbtString(it) })
        }
        return tag
    }

    /** Encodes one vehicle root and all of its riders through vanilla's nested `Passengers` shape. */
    public fun encodeTree(
        root: ServerEntity,
        entityById: (Int) -> ServerEntity?,
    ): NbtCompound = encodeTree(root, entityById, mutableSetOf())

    private fun encodeTree(
        entity: ServerEntity,
        entityById: (Int) -> ServerEntity?,
        visited: MutableSet<Int>,
    ): NbtCompound {
        visited += entity.entityId
        val tag = encode(entity)
        val passengers =
            entity.passengerIds.mapNotNull { passengerId ->
                val passenger = entityById(passengerId)?.takeIf { !it.removed && visited.add(it.entityId) }
                passenger?.let { encodeTree(it, entityById, visited) }
            }
        if (passengers.isEmpty()) {
            tag.remove("Passengers")
        } else {
            tag["Passengers"] = NbtList(NbtType.COMPOUND, passengers)
        }
        return tag
    }

    /**
     * The entity that tag describes, or null when this build has no such type. [entityId] and the
     * protocol type come from the caller: neither is worth keeping on disk, since a run hands out
     * its own ids.
     */
    public fun decode(
        tag: NbtCompound,
        entityId: Int,
        protocolType: Int,
    ): ServerEntity? {
        val type = runCatching { Identifier.parse(tag.getString("id")) }.getOrNull() ?: return null
        val pos = (tag["Pos"] as? NbtList)?.filterIsInstance<NbtDouble>() ?: return null
        if (pos.size < 3) return null
        val rotation = (tag["Rotation"] as? NbtList)?.filterIsInstance<NbtFloat>().orEmpty()
        val entity =
            ServerEntity(
                entityId = entityId,
                uuid = runCatching { Uuid.parse(tag.getString("UUID")) }.getOrElse { Uuid.random() },
                type = type,
                protocolType = protocolType,
                x = pos[0].value,
                y = pos[1].value,
                z = pos[2].value,
                yaw = rotation.getOrNull(0)?.value ?: 0f,
                pitch = rotation.getOrNull(1)?.value ?: 0f,
            )
        (tag["Motion"] as? NbtList)?.filterIsInstance<NbtDouble>()?.takeIf { it.size >= 3 }?.let {
            entity.velocityX = it[0].value
            entity.velocityY = it[1].value
            entity.velocityZ = it[2].value
        }
        entity.health = tag.numericDouble("Health").takeIf { it > 0.0 } ?: entity.health
        entity.age =
            if (AgeableMobs.usesBooleanBabyFlag(type.path) && tag.numericBoolean("IsBaby")) {
                AgeableMobs.BABY_START_AGE
            } else {
                tag.numericInt("Age")
            }
        entity.fuse = tag.numericInt("Fuse")
        if ("Air" in tag) entity.airSupply = tag.getShort("Air").toInt()
        entity.cubeSize =
            if (CubeMobs.isCube(type)) {
                (tag.numericInt("Size") + 1).coerceIn(1, 127)
            } else {
                CubeMobs.DEFAULT_SIZE
            }
        entity.persistent = tag.numericBoolean("PersistenceRequired")
        entity.noAi = tag.numericBoolean("NoAI")
        entity.canPickUpLoot = tag.getBoolean("CanPickUpLoot")
        entity.loveTicks = tag.numericInt("LoveTicks")
        entity.breedCooldown = tag.numericInt("BreedCooldown")
        tag.getString("VillagerProfession").takeIf { it.isNotEmpty() }?.let { entity.profession = it }
        entity.villagerLevel = tag.numericInt("VillagerLevel").coerceAtLeast(1)
        entity.villagerExperience = tag.numericInt("VillagerExperience")
        if (type.path in PATROLLING_TYPES) {
            tag.getIntArray("patrol_target").takeIf { it.size >= 3 }?.let { entity.patrolTarget = it.copyOf(3) }
            entity.patrolLeader = tag.numericBoolean("PatrolLeader")
            entity.patrolling = tag.numericBoolean("Patrolling")
        }
        if ((type.path == "wandering_trader" || type.path == "trader_llama") && "DespawnDelay" in tag) {
            entity.despawnDelay = tag.getInt("DespawnDelay")
        }
        if (type.path == "wandering_trader") {
            tag.getIntArray("wander_target").takeIf { it.size >= 3 }?.let { entity.wanderTarget = it.copyOf(3) }
        }
        if (type.path == "skeleton_horse") {
            entity.skeletonTrap = tag.numericBoolean("SkeletonTrap")
            entity.skeletonTrapTime = tag.numericInt("SkeletonTrapTime")
            entity.tamedHorse = tag.numericBoolean("Tame")
        }
        tag.getCompound("leash").getString("UUID").takeIf { it.isNotEmpty() }?.let {
            entity.leashHolderUuid = it
        }
        val villagerData = tag.getCompound("VillagerData")
        villagerData.getString("profession").takeIf { it.isNotEmpty() }?.let {
            entity.profession = it.substringAfter(':')
        }
        if (villagerData.size > 0) entity.villagerLevel = villagerData.getInt("level", 1).coerceAtLeast(1)
        if ("Xp" in tag) entity.villagerExperience = tag.getInt("Xp")
        tag.getString("Owner").takeIf { it.isNotEmpty() }?.let { entity.owner = it }
        entity.sitting = tag.numericBoolean("Sitting")
        tag.getString("CustomName").takeIf { it.isNotEmpty() }?.let { entity.customName = it }
        (tag["Tags"] as? NbtList)?.filterIsInstance<NbtString>()?.forEach { entity.tags += it.value }
        readAttributes(tag, entity)
        val hasPackedMaxHealth =
            (tag["attributes"] as? NbtList)?.any {
                (it as? NbtCompound)?.getString("id") == MAX_HEALTH_ATTRIBUTE
            } == true
        if (hasPackedMaxHealth) {
            entity.health = minOf(entity.health, entity.attributeWithModifiers(MAX_HEALTH_ATTRIBUTE))
        }
        entity.sourceNbt = tag.deepCopy()
        return entity
    }

    /** Restores one saved vehicle root and its nested passengers, allocating fresh runtime ids. */
    public fun decodeTree(
        tag: NbtCompound,
        allocateEntityId: () -> Int,
        protocolTypeOf: (Identifier) -> Int?,
    ): List<ServerEntity> {
        val entities = mutableListOf<ServerEntity>()
        decodeTreeNode(tag, null, entities, allocateEntityId, protocolTypeOf)
        return entities
    }

    private fun decodeTreeNode(
        tag: NbtCompound,
        vehicle: ServerEntity?,
        entities: MutableList<ServerEntity>,
        allocateEntityId: () -> Int,
        protocolTypeOf: (Identifier) -> Int?,
    ): ServerEntity? {
        val type = runCatching { Identifier.parse(tag.getString("id")) }.getOrNull() ?: return null
        val protocolType = protocolTypeOf(type) ?: return null
        val entity = decode(tag, allocateEntityId(), protocolType) ?: return null
        if (vehicle != null) {
            entity.vehicleId = vehicle.entityId
            vehicle.passengerIds += entity.entityId
        }
        entities += entity
        tag.getList("Passengers")?.filterIsInstance<NbtCompound>()?.forEach { passenger ->
            decodeTreeNode(passenger, entity, entities, allocateEntityId, protocolTypeOf)
        }
        return entity
    }

    /** Materializes one transformed structure entity through the same decoder used on chunk load. */
    public fun decodeGenerated(
        descriptor: ChunkGeneratedEntity,
        entityId: Int,
        protocolType: Int,
    ): ServerEntity? {
        val tag = descriptor.data.deepCopy()
        tag.putString("id", descriptor.type.toString())
        tag["Pos"] = doubles(descriptor.x, descriptor.y, descriptor.z)
        tag["Rotation"] = floats(descriptor.yaw, descriptor.pitch)
        return decode(tag, entityId, protocolType)
    }

    /** Materializes one generated vehicle root and its passenger descriptors with fresh runtime ids. */
    public fun decodeGeneratedTree(
        descriptor: ChunkGeneratedEntity,
        allocateEntityId: () -> Int,
        protocolTypeOf: (Identifier) -> Int?,
    ): List<ServerEntity> {
        val entities = mutableListOf<ServerEntity>()
        decodeGeneratedTreeNode(descriptor, null, entities, allocateEntityId, protocolTypeOf)
        return entities
    }

    private fun decodeGeneratedTreeNode(
        descriptor: ChunkGeneratedEntity,
        vehicle: ServerEntity?,
        entities: MutableList<ServerEntity>,
        allocateEntityId: () -> Int,
        protocolTypeOf: (Identifier) -> Int?,
    ): ServerEntity? {
        val protocolType = protocolTypeOf(descriptor.type) ?: return null
        val entity = decodeGenerated(descriptor, allocateEntityId(), protocolType) ?: return null
        if (vehicle != null) {
            entity.vehicleId = vehicle.entityId
            vehicle.passengerIds += entity.entityId
        }
        entities += entity
        descriptor.passengers.forEach { passenger ->
            decodeGeneratedTreeNode(passenger, entity, entities, allocateEntityId, protocolTypeOf)
        }
        return entity
    }

    private fun doubles(vararg values: Double): NbtList = NbtList(NbtType.DOUBLE, values.map { NbtDouble(it) })

    private fun floats(vararg values: Float): NbtList = NbtList(NbtType.FLOAT, values.map { NbtFloat(it) })

    /** Reads Minecraft 26.2's `AttributeInstance.Packed.LIST_CODEC` representation. */
    private fun readAttributes(
        tag: NbtCompound,
        entity: ServerEntity,
    ) {
        val attributes = (tag["attributes"] as? NbtList)?.filterIsInstance<NbtCompound>().orEmpty()
        for (attribute in attributes) {
            val id = attribute.getString("id")
            if (id.isEmpty()) continue
            if ("base" in attribute) entity.attributeValues[id] = attribute.numericDouble("base")
            val modifiers =
                (attribute["modifiers"] as? NbtList)?.filterIsInstance<NbtCompound>().orEmpty()
            for (packed in modifiers) {
                val modifierId = packed.getString("id")
                val operation = AttributeOperation.byName(packed.getString("operation"))
                if (modifierId.isEmpty() || operation == null) continue
                entity.attributeModifiers.getOrPut(id) { mutableMapOf() }[modifierId] =
                    AttributeModifier(modifierId, packed.numericDouble("amount"), operation)
            }
        }
    }

    private const val MAX_HEALTH_ATTRIBUTE = "minecraft:max_health"
    private val PATROLLING_TYPES = setOf("pillager", "vindicator", "evoker", "illusioner", "witch", "ravager")
}

/** Vanilla uses byte/int/float tags while this server's older saves used doubles for the same fields. */
internal fun NbtCompound.numericDouble(key: String): Double =
    when (val value = this[key]) {
        is NbtByte -> value.value.toDouble()
        is NbtShort -> value.value.toDouble()
        is NbtInt -> value.value.toDouble()
        is NbtLong -> value.value.toDouble()
        is NbtFloat -> value.value.toDouble()
        is NbtDouble -> value.value
        else -> 0.0
    }

internal fun NbtCompound.numericInt(key: String): Int = numericDouble(key).toInt()

internal fun NbtCompound.numericBoolean(key: String): Boolean = numericDouble(key) != 0.0
