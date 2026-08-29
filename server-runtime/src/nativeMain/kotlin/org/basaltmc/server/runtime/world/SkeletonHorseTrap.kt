package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.BiomeClimateData
import org.basaltmc.server.world.terrain.BiomeTemperature
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.math.floor

/** Minecraft 26.2's thunder-created skeleton horse and its four-rider activation forest. */
internal object SkeletonHorseTrap {
    const val PLAYER_RANGE = 10.0
    const val MAX_LIFE = 18_000
    const val SPAWN_CHANCE_SCALE = 0.01
    const val ACTIVATION_INVULNERABLE_TICKS = 60

    private const val HORSE_PUSH_SPREAD = 1.1485
    private val SKELETON_HORSE = Identifier.vanilla("skeleton_horse")
    private val SKELETON = Identifier.vanilla("skeleton")
    private val LIGHTNING_ROD = Identifier.vanilla("lightning_rod")

    /** `ServerLevel.tickThunder`: rain/biome/sky gate, local-difficulty roll, then rod exclusion. */
    fun shouldCreateFromThunder(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: BasaltRandom,
    ): Boolean {
        if (!rainingAt(world, x, y, z) || !world.gameRules[GameRuleRegistry.SPAWN_MOBS]) return false
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
        val moonBrightness =
            if (world.declaration.dimension.hasClock) {
                NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f
            } else {
                0f
            }
        val effectiveDifficulty =
            MobLocalDifficulty.effectiveDifficulty(
                world.difficulty,
                world.gameTime,
                chunk.inhabitedTime,
                moonBrightness,
            )
        // The chance is before the rod test in vanilla, so a protected strike still consumes it.
        if (random.nextDouble() >= effectiveDifficulty * SPAWN_CHANCE_SCALE) return false
        val below = world.state.getBlockState(x, y - 1, z) ?: return true
        return BlockStateDefinitions.blockOf(below) != LIGHTNING_ROD
    }

    /** Mutates the original trap exactly once and builds its original rider plus three horse pairs. */
    fun activate(
        world: LoadedWorld,
        horse: ServerEntity,
        levelRandom: BasaltRandom,
        horseRandom: BasaltRandom,
        entityRandomFactory: () -> BasaltRandom,
        equipment: SpawnEquipmentFinalizer,
    ): MobSpawnForest {
        val blockX = floor(horse.x).toInt()
        val blockY = floor(horse.y).toInt()
        val blockZ = floor(horse.z).toInt()
        val inhabitedTime = world.state.chunkIfLoaded(blockX shr 4, blockZ shr 4)?.inhabitedTime ?: 0L
        val moonBrightness =
            if (world.declaration.dimension.hasClock) {
                NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f
            } else {
                0f
            }
        val specialMultiplier =
            MobLocalDifficulty.specialMultiplier(
                world.difficulty,
                world.gameTime,
                inhabitedTime,
                moonBrightness,
            )
        return buildActivationForest(
            horse,
            levelRandom,
            horseRandom,
            entityRandomFactory,
            equipment,
            specialMultiplier,
            NaturalSpawner.biomeAt(world, blockX, blockY, blockZ),
            world.gameTime,
            world.difficulty,
        )
    }

    /** Pure topology/finalization half, kept separate from the world's local-difficulty lookup. */
    fun buildActivationForest(
        horse: ServerEntity,
        levelRandom: BasaltRandom,
        horseRandom: BasaltRandom,
        entityRandomFactory: () -> BasaltRandom,
        equipment: SpawnEquipmentFinalizer,
        specialMultiplier: Float,
        biome: Identifier?,
        gameTime: Long,
        difficulty: String,
    ): MobSpawnForest {
        horse.skeletonTrap = false
        horse.tamedHorse = true
        horse.age = 0

        val builder = MobSpawnForestBuilder()
        val originalRider =
            createSkeleton(
                builder,
                horse.x,
                horse.y,
                horse.z,
                levelRandom,
                specialMultiplier,
                biome,
                gameTime,
                difficulty,
                equipment,
                entityRandomFactory,
            )
        builder.mountExternal(originalRider, horse.entityId, force = false)

        repeat(3) {
            val otherHorse = builder.create(SKELETON_HORSE, horse.x, horse.y, horse.z, yaw = 0f)
            val horseEntityRandom = entityRandomFactory()
            finalizeNode(
                builder,
                otherHorse,
                levelRandom,
                specialMultiplier,
                biome,
                gameTime,
                difficulty,
                equipment,
                horseEntityRandom,
                entityRandomFactory,
            )
            otherHorse.data.putBoolean("PersistenceRequired", true)
            otherHorse.data.putBoolean("Tame", true)
            otherHorse.data.putInt("Age", 0)
            otherHorse.data["Motion"] =
                motion(
                    horseRandom.triangle(0.0, HORSE_PUSH_SPREAD),
                    0.0,
                    horseRandom.triangle(0.0, HORSE_PUSH_SPREAD),
                )

            val rider =
                createSkeleton(
                    builder,
                    horse.x,
                    horse.y,
                    horse.z,
                    levelRandom,
                    specialMultiplier,
                    biome,
                    gameTime,
                    difficulty,
                    equipment,
                    entityRandomFactory,
                )
            builder.mount(rider, otherHorse, force = false)
        }
        return builder.freeze(originalRider)
    }

    /** Post-increment lifetime from `SkeletonHorse.aiStep`; activation has already cleared the flag. */
    fun tickLifetime(horse: ServerEntity) {
        if (horse.persistent || !horse.skeletonTrap) return
        val previous = horse.skeletonTrapTime++
        if (previous >= MAX_LIFE) horse.removed = true
    }

    private fun createSkeleton(
        builder: MobSpawnForestBuilder,
        x: Double,
        y: Double,
        z: Double,
        levelRandom: BasaltRandom,
        specialMultiplier: Float,
        biome: Identifier?,
        gameTime: Long,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        entityRandomFactory: () -> BasaltRandom,
    ): MobSpawnNode {
        val skeleton = builder.create(SKELETON, x, y, z, yaw = 0f)
        val entityRandom = entityRandomFactory()
        finalizeNode(
            builder,
            skeleton,
            levelRandom,
            specialMultiplier,
            biome,
            gameTime,
            difficulty,
            equipment,
            entityRandom,
            entityRandomFactory,
        )
        skeleton.data.putBoolean("PersistenceRequired", true)
        equipment.finalizeSkeletonTrapEquipment(skeleton.data, entityRandom, specialMultiplier)
        return skeleton
    }

    private fun finalizeNode(
        builder: MobSpawnForestBuilder,
        node: MobSpawnNode,
        levelRandom: BasaltRandom,
        specialMultiplier: Float,
        biome: Identifier?,
        gameTime: Long,
        difficulty: String,
        equipment: SpawnEquipmentFinalizer,
        entityRandom: BasaltRandom,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        MobSpawnFinalization.writeGeneratedNode(
            builder,
            node,
            groupIndex = 0,
            random = levelRandom,
            specialMultiplier = specialMultiplier,
            group = MobSpawnGroupData(),
            biome = biome,
            blockX = floor(node.x).toInt(),
            blockY = floor(node.y).toInt(),
            blockZ = floor(node.z).toInt(),
            gameTime = gameTime,
            difficulty = difficulty,
            equipmentFinalizer = equipment,
            reason = MobSpawnReason.TRIGGERED,
            entityRandom = entityRandom,
            entityRandomFactory = entityRandomFactory,
        )
    }

    private fun rainingAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (!world.weather.raining || !NaturalSpawner.canSeeSky(world, x, y, z)) return false
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
        if (chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING) > y) return false
        val biome = NaturalSpawner.biomeAt(world, x, y, z) ?: return false
        val climate = BiomeClimateData.forBiomeId(biome.toString())
        return climate.hasPrecipitation && !BiomeTemperature.coldEnoughToSnow(biome.toString(), x, y, z)
    }

    private fun motion(
        x: Double,
        y: Double,
        z: Double,
    ): NbtList = NbtList(NbtType.DOUBLE, listOf(NbtDouble(x), NbtDouble(y), NbtDouble(z)))
}
