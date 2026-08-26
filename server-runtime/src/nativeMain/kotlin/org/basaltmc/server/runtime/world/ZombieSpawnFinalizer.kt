package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.entity.AttributeOperation
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtFloat
import kotlin.math.floor

/** Minecraft 26.2's zombie-family layers after the shared mob finalizer. */
internal object ZombieSpawnFinalizer {
    fun afterCommon(
        builder: MobSpawnForestBuilder,
        node: MobSpawnNode,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        reason: MobSpawnReason,
        entityRandom: BasaltRandom,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        if (node.type.path !in ZOMBIE_TYPES) return
        val data = node.data
        data.putBoolean("CanPickUpLoot", random.nextFloat() < PICKUP_CHANCE * specialMultiplier)
        val baby = group.zombieBaby ?: (random.nextFloat() < BABY_CHANCE).also { group.zombieBaby = it }
        if (baby) {
            data.putBoolean("IsBaby", true)
            maybeSpawnChickenJockey(
                builder,
                node,
                group,
                random,
                specialMultiplier,
                difficulty,
                equipment,
                biome,
                gameTime,
                entityRandomFactory,
            )
        }
        data.putBoolean("CanBreakDoors", random.nextFloat() < specialMultiplier * BREAK_DOOR_CHANCE)
        equipment.finalizeZombieEquipmentAfterCommon(data, node.type.path, random, specialMultiplier, difficulty)
        writeAttributes(data, node.type.path, specialMultiplier, entityRandom)

        when (node.type.path) {
            "drowned" ->
                finalizeDrowned(
                    builder,
                    node,
                    random,
                    specialMultiplier,
                    difficulty,
                    equipment,
                    biome,
                    gameTime,
                    reason,
                    entityRandomFactory,
                )
            "husk" ->
                finalizeHusk(
                    builder,
                    node,
                    random,
                    specialMultiplier,
                    difficulty,
                    equipment,
                    biome,
                    gameTime,
                    reason,
                    entityRandomFactory,
                )
        }
    }

    private fun maybeSpawnChickenJockey(
        builder: MobSpawnForestBuilder,
        zombie: MobSpawnNode,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        if (!group.zombieCanSpawnJockey) return
        val reuseNearbyChicken = random.nextFloat() < CHICKEN_JOCKEY_CHANCE
        if (reuseNearbyChicken || random.nextFloat() >= CHICKEN_JOCKEY_CHANCE) return

        val chicken = builder.create(Identifier.vanilla("chicken"), zombie.x, zombie.y, zombie.z, zombie.yaw)
        finalizeChild(
            builder,
            chicken,
            random,
            specialMultiplier,
            difficulty,
            equipment,
            biome,
            gameTime,
            MobSpawnReason.JOCKEY,
            MobSpawnGroupData(),
            entityRandomFactory,
        )
        chicken.data.putBoolean("IsChickenJockey", true)
        builder.mount(zombie, chicken, force = false)
    }

    private fun finalizeDrowned(
        builder: MobSpawnForestBuilder,
        drowned: MobSpawnNode,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        reason: MobSpawnReason,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        if (equipment.itemId(drowned.data, "offhand") == null && random.nextFloat() < NAUTILUS_SHELL_CHANCE) {
            equipment.setItem(drowned.data, "offhand", "nautilus_shell")
            equipment.setGuaranteedDrop(drowned.data, "offhand")
        }
        if (reason !in DROWNED_VEHICLE_REASONS ||
            equipment.itemId(drowned.data, "mainhand") != "minecraft:trident" ||
            random.nextFloat() >= DROWNED_VEHICLE_CHANCE ||
            drowned.data.getBoolean("IsBaby") ||
            biome?.path in FREQUENT_DROWNED_BIOMES
        ) {
            return
        }
        val vehicle =
            builder.create(
                Identifier.vanilla("zombie_nautilus"),
                drowned.x,
                drowned.y,
                drowned.z,
                drowned.yaw,
            )
        if (reason == MobSpawnReason.STRUCTURE) vehicle.data.putBoolean("PersistenceRequired", true)
        finalizeChild(
            builder,
            vehicle,
            random,
            specialMultiplier,
            difficulty,
            equipment,
            biome,
            gameTime,
            reason,
            MobSpawnGroupData(),
            entityRandomFactory,
        )
        builder.mount(drowned, vehicle, force = false)
    }

    private fun finalizeHusk(
        builder: MobSpawnForestBuilder,
        husk: MobSpawnNode,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        reason: MobSpawnReason,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        husk.data.putBoolean("CanPickUpLoot", random.nextFloat() < PICKUP_CHANCE * specialMultiplier)
        if (reason != MobSpawnReason.NATURAL) return
        if (random.nextFloat() >= CAMEL_HUSK_CHANCE) return

        equipment.setItem(husk.data, "mainhand", "iron_spear")
        val camel = builder.create(Identifier.vanilla("camel_husk"), husk.x, husk.y, husk.z, husk.yaw)
        finalizeChild(
            builder,
            camel,
            random,
            specialMultiplier,
            difficulty,
            equipment,
            biome,
            gameTime,
            reason,
            MobSpawnGroupData(),
            entityRandomFactory,
        )
        builder.mount(husk, camel, force = true)

        val parched = builder.create(Identifier.vanilla("parched"), husk.x, husk.y, husk.z, husk.yaw)
        finalizeChild(
            builder,
            parched,
            random,
            specialMultiplier,
            difficulty,
            equipment,
            biome,
            gameTime,
            reason,
            MobSpawnGroupData(),
            entityRandomFactory,
        )
        builder.mount(parched, camel, force = false)
    }

    private fun finalizeChild(
        builder: MobSpawnForestBuilder,
        child: MobSpawnNode,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        biome: Identifier?,
        gameTime: Long,
        reason: MobSpawnReason,
        group: MobSpawnGroupData,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        MobSpawnFinalization.writeGeneratedNode(
            builder,
            child,
            groupIndex = 0,
            random = random,
            specialMultiplier = specialMultiplier,
            group = group,
            biome = biome,
            blockX = floor(child.x).toInt(),
            blockY = floor(child.y).toInt(),
            blockZ = floor(child.z).toInt(),
            gameTime = gameTime,
            difficulty = difficulty,
            equipmentFinalizer = equipment,
            reason = reason,
            entityRandom = entityRandomFactory(),
            entityRandomFactory = entityRandomFactory,
        )
    }

    private fun writeAttributes(
        data: NbtCompound,
        type: String,
        specialMultiplier: Float,
        random: BasaltRandom,
    ) {
        val reinforcementBase =
            if (type == "zombified_piglin") 0.0 else random.nextDouble() * REINFORCEMENT_BASE_RANGE
        SpawnAttributeNbt.setBase(data, REINFORCEMENTS, reinforcementBase)
        SpawnAttributeNbt.addModifier(
            data,
            KNOCKBACK_RESISTANCE,
            0.0,
            RANDOM_SPAWN_BONUS,
            random.nextDouble() * KNOCKBACK_BONUS_RANGE,
            AttributeOperation.ADD_VALUE,
        )
        val followRangeBonus = random.nextDouble() * FOLLOW_RANGE_BONUS_RANGE * specialMultiplier
        if (followRangeBonus > FOLLOW_RANGE_BONUS_THRESHOLD) {
            SpawnAttributeNbt.addModifier(
                data,
                FOLLOW_RANGE,
                FOLLOW_RANGE_BASE,
                ZOMBIE_RANDOM_SPAWN_BONUS,
                followRangeBonus,
                AttributeOperation.ADD_MULTIPLIED_TOTAL,
            )
        }
        if (random.nextFloat() >= specialMultiplier * LEADER_CHANCE) return
        val reinforcementBonus = random.nextDouble() * LEADER_REINFORCEMENT_RANGE + LEADER_REINFORCEMENT_MIN
        val healthBonus = random.nextDouble() * LEADER_HEALTH_RANGE + LEADER_HEALTH_MIN
        SpawnAttributeNbt.addModifier(
            data,
            REINFORCEMENTS,
            reinforcementBase,
            LEADER_ZOMBIE_BONUS,
            reinforcementBonus,
            AttributeOperation.ADD_VALUE,
        )
        SpawnAttributeNbt.addModifier(
            data,
            MAX_HEALTH,
            MAX_HEALTH_BASE,
            LEADER_ZOMBIE_BONUS,
            healthBonus,
            AttributeOperation.ADD_MULTIPLIED_TOTAL,
        )
        data["Health"] = NbtFloat((MAX_HEALTH_BASE * (1.0 + healthBonus)).toFloat())
        data.putBoolean("CanBreakDoors", true)
    }

    private val ZOMBIE_TYPES = setOf("zombie", "zombie_villager", "husk", "drowned", "zombified_piglin")
    private val DROWNED_VEHICLE_REASONS = setOf(MobSpawnReason.NATURAL, MobSpawnReason.STRUCTURE)
    private val FREQUENT_DROWNED_BIOMES = setOf("river", "frozen_river")

    private const val PICKUP_CHANCE = 0.55f
    private const val BABY_CHANCE = 0.05f
    private const val BREAK_DOOR_CHANCE = 0.1f
    private const val CHICKEN_JOCKEY_CHANCE = 0.05f
    private const val NAUTILUS_SHELL_CHANCE = 0.03f
    private const val DROWNED_VEHICLE_CHANCE = 0.5f
    private const val CAMEL_HUSK_CHANCE = 0.1f
    private const val REINFORCEMENT_BASE_RANGE = 0.1f
    private const val KNOCKBACK_BONUS_RANGE = 0.05f
    private const val FOLLOW_RANGE_BONUS_RANGE = 1.5
    private const val FOLLOW_RANGE_BONUS_THRESHOLD = 1.0
    private const val LEADER_CHANCE = 0.05f
    private const val LEADER_REINFORCEMENT_RANGE = 0.25
    private const val LEADER_REINFORCEMENT_MIN = 0.5
    private const val LEADER_HEALTH_RANGE = 3.0
    private const val LEADER_HEALTH_MIN = 1.0
    private const val FOLLOW_RANGE_BASE = 35.0
    private const val MAX_HEALTH_BASE = 20.0
    private const val REINFORCEMENTS = "minecraft:spawn_reinforcements_chance"
    private const val KNOCKBACK_RESISTANCE = "minecraft:knockback_resistance"
    private const val FOLLOW_RANGE = "minecraft:follow_range"
    private const val MAX_HEALTH = "minecraft:max_health"
    private const val RANDOM_SPAWN_BONUS = "minecraft:random_spawn_bonus"
    private const val ZOMBIE_RANDOM_SPAWN_BONUS = "minecraft:zombie_random_spawn_bonus"
    private const val LEADER_ZOMBIE_BONUS = "minecraft:leader_zombie_bonus"
}
