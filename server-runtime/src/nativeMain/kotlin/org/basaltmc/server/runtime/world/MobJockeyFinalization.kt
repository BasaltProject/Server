package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.math.floor

/** Passenger trees and pack effects created by concrete mob finalizers after their superclass. */
internal object MobJockeyFinalization {
    fun afterCommon(
        data: NbtCompound,
        type: Identifier,
        groupIndex: Int,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipmentFinalizer: SpawnEquipmentFinalizer?,
        biome: Identifier?,
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        gameTime: Long,
    ): List<ChunkGeneratedEntity> {
        if (type.path != "spider") return emptyList()
        val passengers =
            if (random.nextInt(SPIDER_JOCKEY_BOUND) == 0) {
                listOf(
                    finalizedPassenger(
                        Identifier.vanilla("skeleton"),
                        random,
                        specialMultiplier,
                        difficulty,
                        equipmentFinalizer,
                        biome,
                        x,
                        y,
                        z,
                        yaw,
                        gameTime,
                    ),
                )
            } else {
                emptyList()
            }
        if (groupIndex == 0) {
            group.spiderEffect =
                if (difficulty == "hard" && random.nextFloat() < SPIDER_EFFECT_CHANCE * specialMultiplier) {
                    SPIDER_EFFECTS[random.nextInt(SPIDER_EFFECTS.size)]
                } else {
                    null
                }
        }
        group.spiderEffect?.let { writeInfiniteEffect(data, it) }
        return passengers
    }

    private fun finalizedPassenger(
        type: Identifier,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipmentFinalizer: SpawnEquipmentFinalizer?,
        biome: Identifier?,
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        gameTime: Long,
    ): ChunkGeneratedEntity {
        val data = NbtCompound()
        val passengers =
            MobSpawnFinalization.writeGenerated(
                data,
                type,
                groupIndex = 0,
                random = random,
                specialMultiplier = specialMultiplier,
                biome = biome,
                blockX = floor(x).toInt(),
                blockY = floor(y).toInt(),
                blockZ = floor(z).toInt(),
                gameTime = gameTime,
                difficulty = difficulty,
                equipmentFinalizer = equipmentFinalizer,
                entityX = x,
                entityY = y,
                entityZ = z,
                entityYaw = yaw,
            )
        return ChunkGeneratedEntity(
            type = type,
            x = x,
            y = y,
            z = z,
            yaw = yaw,
            pitch = 0f,
            data = data,
            passengers = passengers,
        )
    }

    private fun writeInfiniteEffect(
        data: NbtCompound,
        effect: String,
    ) {
        val instance =
            NbtCompound().apply {
                putString("id", "minecraft:$effect")
                putInt("duration", -1)
                putBoolean("show_icon", true)
            }
        data["active_effects"] = NbtList(NbtType.COMPOUND, listOf(instance))
        if (effect == "invisibility") data.putBoolean("Invisible", true)
    }

    private const val SPIDER_JOCKEY_BOUND = 100
    private const val SPIDER_EFFECT_CHANCE = 0.1f
    private val SPIDER_EFFECTS = listOf("speed", "speed", "strength", "regeneration", "invisibility")
}
