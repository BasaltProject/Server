package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.nbt.NbtCompound

/** Minecraft 26.2's concrete piglin state that runs before `Mob.finalizeSpawn`. */
internal object PiglinSpawnFinalizer {
    fun finalizeBeforeCommon(
        data: NbtCompound,
        levelRandom: BasaltRandom,
        entityRandom: BasaltRandom,
        specialMultiplier: Float,
        equipment: SpawnEquipmentFinalizer,
    ): Boolean {
        val baby = levelRandom.nextFloat() < BABY_CHANCE
        data.putBoolean("IsBaby", baby)
        if (!baby) {
            val weapon =
                if (entityRandom.nextFloat() < CROSSBOW_CHANCE) {
                    "crossbow"
                } else if (entityRandom.nextInt(GOLDEN_SPEAR_ODDS) == 0) {
                    "golden_spear"
                } else {
                    "golden_sword"
                }
            equipment.setItem(data, "mainhand", weapon)
        }

        val huntDelay = MIN_HUNT_DELAY + levelRandom.nextInt(HUNT_DELAY_RANGE)
        data["Brain"] =
            NbtCompound().apply {
                this["memories"] =
                    NbtCompound().apply {
                        this["minecraft:hunted_recently"] =
                            NbtCompound().apply {
                                putBoolean("value", true)
                                putLong("ttl", huntDelay.toLong())
                            }
                    }
            }
        equipment.finalizePiglinEquipmentBeforeCommon(data, levelRandom, specialMultiplier, baby)
        return baby
    }

    private const val BABY_CHANCE = 0.2f
    private const val CROSSBOW_CHANCE = 0.5f
    private const val GOLDEN_SPEAR_ODDS = 10
    private const val MIN_HUNT_DELAY = 30 * 20
    private const val HUNT_DELAY_RANGE = (120 - 30) * 20 + 1
}
