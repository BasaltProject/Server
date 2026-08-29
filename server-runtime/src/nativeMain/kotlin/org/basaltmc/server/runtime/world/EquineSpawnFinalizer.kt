package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.common.Identifier
import kotlin.math.floor

/** Passenger topology created by Minecraft 26.2 equine finalizers before their superclass. */
internal object EquineSpawnFinalizer {
    fun beforeCommon(
        builder: MobSpawnForestBuilder,
        horse: MobSpawnNode,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        reason: MobSpawnReason,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        if (horse.type.path != "zombie_horse" || reason != MobSpawnReason.NATURAL) return

        // parity: ZombieHorse.finalizeSpawn creates and fully finalizes the zombie before
        // AbstractHorse randomizes the horse, then replaces the rider's main-hand item.
        val rider = builder.create(Identifier.vanilla("zombie"), horse.x, horse.y, horse.z, horse.yaw)
        MobSpawnFinalization.writeGeneratedNode(
            builder,
            rider,
            groupIndex = 0,
            random = random,
            specialMultiplier = specialMultiplier,
            group = MobSpawnGroupData(),
            biome = biome,
            blockX = floor(rider.x).toInt(),
            blockY = floor(rider.y).toInt(),
            blockZ = floor(rider.z).toInt(),
            gameTime = gameTime,
            difficulty = difficulty,
            equipmentFinalizer = equipment,
            reason = reason,
            entityRandom = entityRandomFactory(),
            entityRandomFactory = entityRandomFactory,
        )
        equipment.setItem(rider.data, "mainhand", "iron_spear")
        builder.mount(rider, horse, force = false)
    }
}
