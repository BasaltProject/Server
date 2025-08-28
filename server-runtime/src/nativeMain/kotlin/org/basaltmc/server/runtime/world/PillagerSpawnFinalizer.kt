package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType

/** Minecraft 26.2's pillager, patrolling-monster and raider finalization stack. */
internal object PillagerSpawnFinalizer {
    fun beforeCommon(
        group: MobSpawnGroupData,
        levelRandom: BasaltRandom,
        entityRandom: BasaltRandom,
        specialMultiplier: Float,
        reason: MobSpawnReason,
        equipment: SpawnEquipmentFinalizer,
        data: NbtCompound,
    ) {
        equipment.finalizePillagerEquipmentBeforeCommon(data, levelRandom, specialMultiplier)

        var leader = group.patrolLeader
        if (!leader && reason != MobSpawnReason.PATROL && reason != MobSpawnReason.STRUCTURE) {
            leader = levelRandom.nextFloat() < RANDOM_LEADER_CHANCE
        }
        if (group.patrolLeader) {
            // parity: PatrolSpawner.findPatrolTarget runs before setPos, while the fresh entity is
            // still at the origin, and consumes the entity's own RNG rather than the level RNG.
            data.putIntArray(
                "patrol_target",
                intArrayOf(
                    -PATROL_TARGET_RADIUS + entityRandom.nextInt(PATROL_TARGET_DIAMETER),
                    0,
                    -PATROL_TARGET_RADIUS + entityRandom.nextInt(PATROL_TARGET_DIAMETER),
                ),
            )
        }
        if (leader) {
            data.putBoolean("PatrolLeader", true)
            data.putBoolean("Patrolling", true)
            writeOminousBanner(data, equipment)
        } else if (reason == MobSpawnReason.PATROL) {
            data.putBoolean("Patrolling", true)
        }
        // parity: Raider.finalizeSpawn; only naturally spawned witches disable this flag.
        data.putBoolean("CanJoinRaid", true)
    }

    private fun writeOminousBanner(
        data: NbtCompound,
        equipment: SpawnEquipmentFinalizer,
    ) {
        equipment.setItem(data, "head", "white_banner")
        equipment.setGuaranteedDrop(data, "head")
        val banner = data.getCompound("equipment").getCompound("head")
        val components = banner.getCompound("components")
        components["minecraft:banner_patterns"] =
            NbtList(
                NbtType.COMPOUND,
                OMINOUS_PATTERNS.map { (pattern, color) ->
                    NbtCompound().apply {
                        putString("pattern", "minecraft:$pattern")
                        putString("color", color)
                    }
                },
            )
        components["minecraft:tooltip_display"] =
            NbtCompound().apply {
                this["hidden_components"] =
                    NbtList(NbtType.STRING, listOf(NbtString("minecraft:banner_patterns")))
            }
        components["minecraft:item_name"] =
            NbtCompound().apply { putString("translate", "block.minecraft.ominous_banner") }
        components.putString("minecraft:rarity", "uncommon")
        banner["components"] = components
    }

    private const val RANDOM_LEADER_CHANCE = 0.06f
    private const val PATROL_TARGET_RADIUS = 500
    private const val PATROL_TARGET_DIAMETER = 1000

    private val OMINOUS_PATTERNS =
        listOf(
            "rhombus" to "cyan",
            "stripe_bottom" to "light_gray",
            "stripe_center" to "gray",
            "border" to "light_gray",
            "stripe_middle" to "black",
            "half_horizontal" to "light_gray",
            "circle" to "light_gray",
            "border" to "black",
        )
}
