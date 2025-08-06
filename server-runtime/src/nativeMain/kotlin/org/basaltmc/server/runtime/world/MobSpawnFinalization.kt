package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.runtime.entity.AgeableMobs
import org.basaltmc.server.runtime.entity.AttributeModifier
import org.basaltmc.server.runtime.entity.AttributeOperation
import org.basaltmc.server.runtime.entity.CubeMobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType

/** The common 26.2 `finalizeSpawn` state shared by every mob, ageable mobs and cube mobs. */
public object MobSpawnFinalization {
    /**
     * Applies the common superclass finalizers in their exact order.
     *
     * `AgeableMob` first gives every accepted pack member after the first a five-percent baby
     * roll. `Mob` then adds its triangular follow-range bonus and rolls for left-handedness.
     * `AbstractCubeMob` finally selects size 1/2/4, with local difficulty able to promote a size-1
     * or size-2 result one step. Type-specific variants/equipment remain their own layers.
     */
    public fun apply(
        entity: ServerEntity,
        groupIndex: Int,
        random: BasaltRandom,
        specialMultiplier: Float,
    ) {
        val state = stateFor(entity.type, groupIndex, random, specialMultiplier)
        entity.age = state.age
        entity.attributeModifiers
            .getOrPut(FOLLOW_RANGE_ATTRIBUTE) { mutableMapOf() }[RANDOM_SPAWN_BONUS_ID] =
            AttributeModifier(
                RANDOM_SPAWN_BONUS_ID,
                state.followRangeBonus,
                AttributeOperation.ADD_MULTIPLIED_BASE,
            )
        val data = entity.sourceNbt ?: NbtCompound().also { entity.sourceNbt = it }
        writeMobState(data, entity.type, state)
        CubeMobs.applySize(entity, state.cubeSize)
    }

    /** Writes finalization state into an off-thread generated entity descriptor. */
    public fun writeGenerated(
        data: NbtCompound,
        type: Identifier,
        groupIndex: Int,
        random: BasaltRandom,
        specialMultiplier: Float = 0f,
    ) {
        val state = stateFor(type, groupIndex, random, specialMultiplier)
        if (state.age != 0) data.putInt("Age", state.age)
        writeMobState(data, type, state)
        // parity: AbstractCubeMob saves one less than its live size, so absent/zero means size one.
        if (CubeMobs.isCube(type)) data.putInt("Size", state.cubeSize - 1)
    }

    internal fun stateFor(
        type: Identifier,
        groupIndex: Int,
        random: BasaltRandom,
        specialMultiplier: Float,
    ): CommonSpawnState {
        val age =
            if (
                AgeableMobs.isAgeable(type) &&
                groupIndex > 0 &&
                random.nextFloat() <= BABY_SPAWN_CHANCE
            ) {
                AgeableMobs.BABY_START_AGE
            } else {
                0
            }
        // parity: Mob.finalizeSpawn. RandomSource.triangle consumes two doubles before the
        // left-handed float, even though neither result usually changes what the client draws.
        val followRangeBonus = random.triangle(0.0, FOLLOW_RANGE_BONUS_SPREAD)
        val leftHanded = random.nextFloat() < LEFT_HANDED_CHANCE
        if (!CubeMobs.isCube(type)) {
            return CommonSpawnState(age, CubeMobs.DEFAULT_SIZE, followRangeBonus, leftHanded)
        }

        var sizeScale = random.nextInt(MAX_NATURAL_SIZE_SCALE + 1)
        if (sizeScale < MAX_NATURAL_SIZE_SCALE && random.nextFloat() < 0.5f * specialMultiplier) {
            sizeScale++
        }
        return CommonSpawnState(age, 1 shl sizeScale, followRangeBonus, leftHanded)
    }

    /** Writes the vanilla fields that let generated mobs survive materialization and chunk saves. */
    private fun writeMobState(
        data: NbtCompound,
        type: Identifier,
        state: CommonSpawnState,
    ) {
        data.putBoolean("LeftHanded", state.leftHanded)
        val base = EntityAttributes.baseValue(type, EntityAttributes.FOLLOW_RANGE) ?: return
        val modifier =
            NbtCompound().apply {
                putString("id", RANDOM_SPAWN_BONUS_ID)
                this["amount"] = NbtDouble(state.followRangeBonus)
                putString("operation", AttributeOperation.ADD_MULTIPLIED_BASE.serializedName)
            }
        val attribute =
            NbtCompound().apply {
                putString("id", FOLLOW_RANGE_ATTRIBUTE)
                this["base"] = NbtDouble(base)
                this["modifiers"] = NbtList(NbtType.COMPOUND, listOf(modifier))
            }
        data["attributes"] = NbtList(NbtType.COMPOUND, listOf(attribute))
    }

    /**
     * parity: `DifficultyInstance`, including the moon contribution being capped by the global
     * time bonus (not by the full 0.75 base scale).
     */
    public fun specialMultiplier(
        difficulty: String,
        gameTime: Long,
        inhabitedTime: Long,
        moonBrightness: Float,
    ): Float {
        val difficultyId =
            when (difficulty) {
                "peaceful" -> 0
                "easy" -> 1
                "normal" -> 2
                "hard" -> 3
                else -> 1
            }
        if (difficultyId == 0) return 0f

        val globalBonus =
            ((gameTime.toFloat() - GLOBAL_TIME_OFFSET) / MAX_GLOBAL_TIME)
                .coerceIn(0f, 1f) * GLOBAL_TIME_WEIGHT
        var localScale =
            (inhabitedTime.toFloat() / MAX_LOCAL_TIME).coerceIn(0f, 1f) *
                if (difficulty == "hard") 1f else 0.75f
        localScale += (moonBrightness * MOON_WEIGHT).coerceIn(0f, globalBonus)
        if (difficulty == "easy") localScale *= 0.5f

        val effectiveDifficulty = difficultyId * (BASE_SCALE + globalBonus + localScale)
        return when {
            effectiveDifficulty < 2f -> 0f
            effectiveDifficulty > 4f -> 1f
            else -> (effectiveDifficulty - 2f) / 2f
        }
    }

    internal data class CommonSpawnState(
        val age: Int,
        val cubeSize: Int,
        val followRangeBonus: Double,
        val leftHanded: Boolean,
    )

    private const val BABY_SPAWN_CHANCE = 0.05f
    private const val LEFT_HANDED_CHANCE = 0.05f
    private const val FOLLOW_RANGE_BONUS_SPREAD = 0.11485000000000001
    private const val FOLLOW_RANGE_ATTRIBUTE = "minecraft:follow_range"
    private const val RANDOM_SPAWN_BONUS_ID = "minecraft:random_spawn_bonus"
    private const val MAX_NATURAL_SIZE_SCALE = 2
    private const val GLOBAL_TIME_OFFSET = 72_000f
    private const val MAX_GLOBAL_TIME = 1_440_000f
    private const val MAX_LOCAL_TIME = 3_600_000f
    private const val BASE_SCALE = 0.75f
    private const val GLOBAL_TIME_WEIGHT = 0.25f
    private const val MOON_WEIGHT = 0.25f
}
