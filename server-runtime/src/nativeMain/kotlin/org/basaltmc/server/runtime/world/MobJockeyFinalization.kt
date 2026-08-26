package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.math.floor

/** Passenger trees and pack effects created by concrete mob finalizers after their superclass. */
internal object MobJockeyFinalization {
    fun afterCommon(
        builder: MobSpawnForestBuilder,
        node: MobSpawnNode,
        groupIndex: Int,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipmentFinalizer: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        if (node.type.path != "spider") return
        if (random.nextInt(SPIDER_JOCKEY_BOUND) == 0) {
            val passenger =
                builder.create(
                    Identifier.vanilla("skeleton"),
                    node.x,
                    node.y,
                    node.z,
                    node.yaw,
                    data = NbtCompound(),
                )
            MobSpawnFinalization.writeGeneratedNode(
                builder,
                passenger,
                groupIndex = 0,
                random = random,
                specialMultiplier = specialMultiplier,
                group = MobSpawnGroupData(),
                biome = biome,
                blockX = floor(node.x).toInt(),
                blockY = floor(node.y).toInt(),
                blockZ = floor(node.z).toInt(),
                gameTime = gameTime,
                difficulty = difficulty,
                equipmentFinalizer = equipmentFinalizer,
                reason = MobSpawnReason.JOCKEY,
                entityRandom = entityRandomFactory(),
                entityRandomFactory = entityRandomFactory,
            )
            builder.mount(passenger, node, force = true)
        }
        if (groupIndex == 0) {
            group.spiderEffect =
                if (difficulty == "hard" && random.nextFloat() < SPIDER_EFFECT_CHANCE * specialMultiplier) {
                    SPIDER_EFFECTS[random.nextInt(SPIDER_EFFECTS.size)]
                } else {
                    null
                }
        }
        group.spiderEffect?.let { writeInfiniteEffect(node.data, it) }
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
