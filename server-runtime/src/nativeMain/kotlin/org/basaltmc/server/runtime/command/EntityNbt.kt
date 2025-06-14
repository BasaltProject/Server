package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType

/**
 * A live entity as NBT, in the shape vanilla's own entity data uses for the same fields.
 *
 * One view for `/data get entity` and for a selector's `nbt=`, because a player who reads a field
 * out of the first and filters on it with the second has to see the same name and the same type.
 * Only what this build models: everything else arrives with the system that makes it exist.
 */
public object EntityNbt {
    public fun of(player: PlayerSession): NbtCompound =
        NbtCompound().apply {
            putString("id", "minecraft:player")
            putString("Name", player.name)
            putFloat("Health", player.health)
            putInt("XpLevel", player.experience.level)
            putFloat("XpP", player.experience.progress)
            putInt("XpTotal", player.experience.total)
            putString("Dimension", player.worldName)
            this["Pos"] = NbtList(NbtType.DOUBLE, listOf(NbtDouble(player.x), NbtDouble(player.y), NbtDouble(player.z)))
            this["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(player.yaw), NbtFloat(player.pitch)))
            this["Tags"] = NbtList(NbtType.STRING, player.tags.map { NbtString(it) })
            this["foodLevel"] = NbtInt(player.food.foodLevel)
        }

    public fun of(entity: ServerEntity): NbtCompound =
        NbtCompound().apply {
            putString("id", entity.type.toString())
            entity.customName?.let { putString("CustomName", it) }
            putFloat("Health", entity.health.toFloat())
            this["Pos"] = NbtList(NbtType.DOUBLE, listOf(NbtDouble(entity.x), NbtDouble(entity.y), NbtDouble(entity.z)))
            this["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(entity.yaw), NbtFloat(entity.pitch)))
            this["Tags"] = NbtList(NbtType.STRING, entity.tags.map { NbtString(it) })
        }
}
