package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
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
        val tag = NbtCompound()
        tag["id"] = NbtString(entity.type.toString())
        tag["UUID"] = NbtString(entity.uuid.toString())
        tag["Pos"] = doubles(entity.x, entity.y, entity.z)
        tag["Motion"] = doubles(entity.velocityX, entity.velocityY, entity.velocityZ)
        tag["Rotation"] = floats(entity.yaw, entity.pitch)
        tag["Health"] = NbtDouble(entity.health)
        tag["Age"] = NbtDouble(entity.age.toDouble())
        tag["Fuse"] = NbtDouble(entity.fuse.toDouble())
        tag["Size"] = NbtDouble(entity.cubeSize.toDouble())
        tag["PersistenceRequired"] = NbtDouble(if (entity.persistent) 1.0 else 0.0)
        tag["LoveTicks"] = NbtDouble(entity.loveTicks.toDouble())
        tag["BreedCooldown"] = NbtDouble(entity.breedCooldown.toDouble())
        tag["VillagerProfession"] = NbtString(entity.profession)
        tag["VillagerLevel"] = NbtDouble(entity.villagerLevel.toDouble())
        tag["VillagerExperience"] = NbtDouble(entity.villagerExperience.toDouble())
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
        entity.health = tag.getDouble("Health").takeIf { it > 0.0 } ?: entity.health
        entity.age = tag.getDouble("Age").toInt()
        entity.fuse = tag.getDouble("Fuse").toInt()
        entity.cubeSize = tag.getDouble("Size").toInt().coerceAtLeast(1)
        entity.persistent = tag.getDouble("PersistenceRequired") != 0.0
        entity.loveTicks = tag.getDouble("LoveTicks").toInt()
        entity.breedCooldown = tag.getDouble("BreedCooldown").toInt()
        tag.getString("VillagerProfession").takeIf { it.isNotEmpty() }?.let { entity.profession = it }
        entity.villagerLevel = tag.getDouble("VillagerLevel").toInt().coerceAtLeast(1)
        entity.villagerExperience = tag.getDouble("VillagerExperience").toInt()
        tag.getString("Owner").takeIf { it.isNotEmpty() }?.let { entity.owner = it }
        entity.sitting = tag.getDouble("Sitting") != 0.0
        tag.getString("CustomName").takeIf { it.isNotEmpty() }?.let { entity.customName = it }
        (tag["Tags"] as? NbtList)?.filterIsInstance<NbtString>()?.forEach { entity.tags += it.value }
        return entity
    }

    private fun doubles(vararg values: Double): NbtList = NbtList(NbtType.DOUBLE, values.map { NbtDouble(it) })

    private fun floats(vararg values: Float): NbtList = NbtList(NbtType.FLOAT, values.map { NbtFloat(it) })
}
