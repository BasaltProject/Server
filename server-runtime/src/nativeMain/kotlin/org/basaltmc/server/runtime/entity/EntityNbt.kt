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
        tag["id"] = NbtString(entity.type.toString())
        tag["UUID"] = NbtString(entity.uuid.toString())
        tag["Pos"] = doubles(entity.x, entity.y, entity.z)
        tag["Motion"] = doubles(entity.velocityX, entity.velocityY, entity.velocityZ)
        tag["Rotation"] = floats(entity.yaw, entity.pitch)
        tag["Health"] = NbtDouble(entity.health)
        tag["Age"] = NbtDouble(entity.age.toDouble())
        tag["Fuse"] = NbtDouble(entity.fuse.toDouble())
        tag.putShort("Air", entity.airSupply.toShort())
        if (CubeMobs.isCube(entity.type)) tag["Size"] = NbtInt(entity.cubeSize.coerceAtLeast(1) - 1)
        tag["PersistenceRequired"] = NbtDouble(if (entity.persistent) 1.0 else 0.0)
        tag.putBoolean("NoAI", entity.noAi)
        tag.putBoolean("CanPickUpLoot", entity.canPickUpLoot)
        tag["LoveTicks"] = NbtDouble(entity.loveTicks.toDouble())
        tag["BreedCooldown"] = NbtDouble(entity.breedCooldown.toDouble())
        tag["VillagerProfession"] = NbtString(entity.profession)
        tag["VillagerLevel"] = NbtDouble(entity.villagerLevel.toDouble())
        tag["VillagerExperience"] = NbtDouble(entity.villagerExperience.toDouble())
        if (entity.type.path == "villager" || entity.type.path == "zombie_villager") {
            val villagerData = tag.getCompound("VillagerData")
            if (villagerData.getString("type").isEmpty()) villagerData.putString("type", "minecraft:plains")
            villagerData.putString("profession", "minecraft:${entity.profession}")
            villagerData.putInt("level", entity.villagerLevel)
            tag["VillagerData"] = villagerData
            tag.putInt("Xp", entity.villagerExperience)
        }
        entity.owner?.let { tag["Owner"] = NbtString(it) }
        tag["Sitting"] = NbtDouble(if (entity.sitting) 1.0 else 0.0)
        entity.customName?.let { tag["CustomName"] = NbtString(it) }
        if (entity.tags.isNotEmpty()) {
            tag["Tags"] = NbtList(NbtType.STRING, entity.tags.map { NbtString(it) })
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
        entity.age = tag.numericInt("Age")
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

    /** Vanilla uses byte/int/float tags while this server's older saves used doubles for the same fields. */
    private fun NbtCompound.numericDouble(key: String): Double =
        when (val value = this[key]) {
            is NbtByte -> value.value.toDouble()
            is NbtShort -> value.value.toDouble()
            is NbtInt -> value.value.toDouble()
            is NbtLong -> value.value.toDouble()
            is NbtFloat -> value.value.toDouble()
            is NbtDouble -> value.value
            else -> 0.0
        }

    private fun NbtCompound.numericInt(key: String): Int = numericDouble(key).toInt()

    private fun NbtCompound.numericBoolean(key: String): Boolean = numericDouble(key) != 0.0

    private const val MAX_HEALTH_ATTRIBUTE = "minecraft:max_health"
}
