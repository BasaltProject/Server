package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.common.Identifier
import kotlin.math.floor

/** Minecraft 26.2's recursive strider jockey selection before its ageable/mob superclass. */
internal object StriderSpawnFinalizer {
    fun beforeCommon(
        builder: MobSpawnForestBuilder,
        strider: MobSpawnNode,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        if (strider.type.path != "strider" || strider.data.getInt("Age") < 0) return
        if (random.nextInt(ZOMBIFIED_PIGLIN_JOCKEY_ODDS) == 0) {
            val group =
                MobSpawnGroupData().apply {
                    zombieBaby = random.nextFloat() < ZOMBIE_BABY_CHANCE
                    zombieCanSpawnJockey = false
                }
            val jockey =
                builder.create(
                    Identifier.vanilla("zombified_piglin"),
                    strider.x,
                    strider.y,
                    strider.z,
                    strider.yaw,
                )
            finalizeJockey(
                builder,
                jockey,
                group,
                random,
                specialMultiplier,
                difficulty,
                equipment,
                biome,
                gameTime,
                entityRandomFactory,
            )
            equipment.setItem(jockey.data, "mainhand", "warped_fungus_on_a_stick")
            equipment.setItem(strider.data, "saddle", "saddle")
            equipment.setGuaranteedDrop(strider.data, "saddle")
            builder.mount(jockey, strider, force = true)
        } else if (random.nextInt(BABY_STRIDER_JOCKEY_ODDS) == 0) {
            val jockey = builder.create(Identifier.vanilla("strider"), strider.x, strider.y, strider.z, strider.yaw)
            jockey.data.putInt("Age", BABY_START_AGE)
            finalizeJockey(
                builder,
                jockey,
                MobSpawnGroupData(),
                random,
                specialMultiplier,
                difficulty,
                equipment,
                biome,
                gameTime,
                entityRandomFactory,
            )
            builder.mount(jockey, strider, force = true)
        }
    }

    private fun finalizeJockey(
        builder: MobSpawnForestBuilder,
        jockey: MobSpawnNode,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        MobSpawnFinalization.writeGeneratedNode(
            builder,
            jockey,
            groupIndex = 0,
            random = random,
            specialMultiplier = specialMultiplier,
            group = group,
            biome = biome,
            blockX = floor(jockey.x).toInt(),
            blockY = floor(jockey.y).toInt(),
            blockZ = floor(jockey.z).toInt(),
            gameTime = gameTime,
            difficulty = difficulty,
            equipmentFinalizer = equipment,
            reason = MobSpawnReason.JOCKEY,
            entityRandom = entityRandomFactory(),
            entityRandomFactory = entityRandomFactory,
        )
    }

    private const val ZOMBIFIED_PIGLIN_JOCKEY_ODDS = 30
    private const val BABY_STRIDER_JOCKEY_ODDS = 10
    private const val ZOMBIE_BABY_CHANCE = 0.05f
    private const val BABY_START_AGE = -24_000
}
