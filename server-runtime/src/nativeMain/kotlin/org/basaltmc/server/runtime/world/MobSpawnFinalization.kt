package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.runtime.entity.AgeableMobs
import org.basaltmc.server.runtime.entity.AttributeModifier
import org.basaltmc.server.runtime.entity.AttributeOperation
import org.basaltmc.server.runtime.entity.CubeMobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.math.floor

/** Mutable concrete `SpawnGroupData` shared by accepted members of one natural pack. */
public class MobSpawnGroupData {
    internal var variant: Int? = null
    internal var secondaryVariant: Int? = null
    internal var spiderEffect: String? = null
    internal var zombieBaby: Boolean? = null
    internal var zombieCanSpawnJockey: Boolean = true
}

/** Minecraft 26.2 spawn finalization, including concrete pre/post layers around shared mob state. */
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
        group: MobSpawnGroupData = MobSpawnGroupData(),
        biome: Identifier? = null,
        gameTime: Long = 0L,
        difficulty: String = "easy",
        equipmentFinalizer: SpawnEquipmentFinalizer? = null,
        entityRandom: BasaltRandom = random,
    ): List<ChunkGeneratedEntity> {
        val equipment = equipmentFinalizer ?: spawnEquipmentWithoutAssets()
        val state =
            stateFor(
                entity.type,
                groupIndex,
                random,
                specialMultiplier,
                group,
                biome,
                floor(entity.x).toInt(),
                floor(entity.y).toInt(),
                floor(entity.z).toInt(),
                gameTime,
                difficulty,
                equipment,
                entityRandom,
            )
        entity.age = state.age
        if (entity.type.path == "dolphin") entity.pitch = 0f
        entity.attributeValues.putAll(state.attributeBases)
        state.attributeBases[MAX_HEALTH_ATTRIBUTE]?.let { entity.health = minOf(entity.health, it) }
        if (state.ranMobFinalizer) {
            entity.attributeModifiers
                .getOrPut(FOLLOW_RANGE_ATTRIBUTE) { mutableMapOf() }[RANDOM_SPAWN_BONUS_ID] =
                AttributeModifier(
                    RANDOM_SPAWN_BONUS_ID,
                    state.followRangeBonus,
                    AttributeOperation.ADD_MULTIPLIED_BASE,
                )
        }
        val data = entity.sourceNbt ?: NbtCompound().also { entity.sourceNbt = it }
        writeTypeState(data, state)
        equipment.finalizeAfterCommon(data, entity.type.path, random, specialMultiplier, difficulty)
        if ("Air" in state.typeData) entity.airSupply = state.typeData.getShort("Air").toInt()
        if ("CanPickUpLoot" in data) {
            entity.canPickUpLoot = data.getBoolean("CanPickUpLoot")
        }
        writeMobState(data, entity.type, state)
        CubeMobs.applySize(entity, state.cubeSize)
        val builder = MobSpawnForestBuilder()
        val main = builder.create(entity.type, entity.x, entity.y, entity.z, entity.yaw, data = data)
        MobJockeyFinalization.afterCommon(
            builder,
            main,
            groupIndex,
            group,
            random,
            specialMultiplier,
            difficulty,
            equipment,
            biome,
            gameTime,
            entityRandomFactory = { entityRandom },
        )
        return builder.freeze(main).main.passengers
    }

    /** Writes finalization state into an off-thread generated entity descriptor. */
    public fun writeGenerated(
        data: NbtCompound,
        type: Identifier,
        groupIndex: Int,
        random: BasaltRandom,
        specialMultiplier: Float = 0f,
        group: MobSpawnGroupData = MobSpawnGroupData(),
        biome: Identifier? = null,
        blockX: Int = 0,
        blockY: Int = 0,
        blockZ: Int = 0,
        gameTime: Long = 0L,
        difficulty: String = "easy",
        equipmentFinalizer: SpawnEquipmentFinalizer? = null,
        entityX: Double = blockX + 0.5,
        entityY: Double = blockY.toDouble(),
        entityZ: Double = blockZ + 0.5,
        entityYaw: Float = 0f,
        entityRandom: BasaltRandom = random,
    ): List<ChunkGeneratedEntity> {
        val forest =
            writeGeneratedForest(
                data,
                type,
                groupIndex,
                random,
                specialMultiplier,
                group,
                biome,
                blockX,
                blockY,
                blockZ,
                gameTime,
                difficulty,
                equipmentFinalizer,
                entityX,
                entityY,
                entityZ,
                entityYaw,
                entityRandom,
            )
        return forest.main.passengers
    }

    internal fun writeGeneratedNode(
        builder: MobSpawnForestBuilder,
        node: MobSpawnNode,
        groupIndex: Int,
        random: BasaltRandom,
        specialMultiplier: Float,
        group: MobSpawnGroupData,
        biome: Identifier?,
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        gameTime: Long,
        difficulty: String,
        equipmentFinalizer: SpawnEquipmentFinalizer,
        reason: MobSpawnReason,
        entityRandom: BasaltRandom,
        entityRandomFactory: () -> BasaltRandom,
    ) {
        val data = node.data
        val type = node.type
        val equipment = equipmentFinalizer
        EquineSpawnFinalizer.beforeCommon(
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
        StriderSpawnFinalizer.beforeCommon(
            builder,
            node,
            random,
            specialMultiplier,
            difficulty,
            equipment,
            biome,
            gameTime,
            entityRandomFactory,
        )
        val state =
            stateFor(
                type,
                groupIndex,
                random,
                specialMultiplier,
                group,
                biome,
                blockX,
                blockY,
                blockZ,
                gameTime,
                difficulty,
                equipment,
                entityRandom,
                reason,
            )
        if (state.age != 0) data.putInt("Age", state.age)
        writeTypeState(data, state)
        writeMobState(data, type, state)
        equipment.finalizeAfterCommon(data, type.path, random, specialMultiplier, difficulty)
        ZombieSpawnFinalizer.afterCommon(
            builder,
            node,
            group,
            random,
            specialMultiplier,
            difficulty,
            equipment,
            biome,
            gameTime,
            reason,
            entityRandom,
            entityRandomFactory,
        )
        // parity: AbstractCubeMob saves one less than its live size, so absent/zero means size one.
        if (CubeMobs.isCube(type)) data.putInt("Size", state.cubeSize - 1)
        MobJockeyFinalization.afterCommon(
            builder,
            node,
            groupIndex,
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

    internal fun stateFor(
        type: Identifier,
        groupIndex: Int,
        random: BasaltRandom,
        specialMultiplier: Float,
        group: MobSpawnGroupData = MobSpawnGroupData(),
        biome: Identifier? = null,
        blockX: Int = 0,
        blockY: Int = 0,
        blockZ: Int = 0,
        gameTime: Long = 0L,
        difficulty: String = "easy",
        equipmentFinalizer: SpawnEquipmentFinalizer? = null,
        entityRandom: BasaltRandom = random,
        reason: MobSpawnReason = MobSpawnReason.NATURAL,
    ): CommonSpawnState {
        val typeData = NbtCompound()
        if (type.path == "cave_spider") {
            // CaveSpider.finalizeSpawn deliberately returns groupData without calling Mob at all.
            return CommonSpawnState(
                age = 0,
                cubeSize = CubeMobs.DEFAULT_SIZE,
                followRangeBonus = 0.0,
                leftHanded = false,
                ranMobFinalizer = false,
                typeData = typeData,
                attributeBases = emptyMap(),
            )
        }
        val attributeBases = mutableMapOf<String, Double>()
        val concreteBaby =
            MobVariantSpawnFinalizer.beforeCommon(
                type.path,
                biome?.path,
                group,
                random,
                typeData,
                blockX,
                blockY,
                blockZ,
                gameTime,
                specialMultiplier,
                equipmentFinalizer ?: spawnEquipmentWithoutAssets(),
                entityRandom,
                reason,
            )
        MobSpawnAttributes.write(type.path, random, typeData, attributeBases)
        val babyChance = babySpawnChance(type.path)
        val inheritedBaby =
            when {
                !AgeableMobs.isAgeable(type) || groupIndex <= 0 -> false
                type.path == "axolotl" || type.path == "fox" ->
                    groupIndex >= FORCED_BABY_GROUP_INDEX
                babyChance != null -> random.nextFloat() <= babyChance
                else -> false
            }
        val age =
            if ((concreteBaby || inheritedBaby) && type.path != "zombie_nautilus") {
                AgeableMobs.BABY_START_AGE
            } else {
                0
            }
        // parity: Mob.finalizeSpawn. RandomSource.triangle consumes two doubles before the
        // left-handed float, even though neither result usually changes what the client draws.
        val followRangeBonus = random.triangle(0.0, FOLLOW_RANGE_BONUS_SPREAD)
        val leftHanded = random.nextFloat() < LEFT_HANDED_CHANCE
        if (!CubeMobs.isCube(type)) {
            MobVariantSpawnFinalizer.afterCommon(type.path, group, random, typeData)
            return CommonSpawnState(
                age,
                CubeMobs.DEFAULT_SIZE,
                followRangeBonus,
                leftHanded,
                true,
                typeData,
                attributeBases,
            )
        }

        var sizeScale = random.nextInt(MAX_NATURAL_SIZE_SCALE + 1)
        if (sizeScale < MAX_NATURAL_SIZE_SCALE && random.nextFloat() < 0.5f * specialMultiplier) {
            sizeScale++
        }
        return CommonSpawnState(age, 1 shl sizeScale, followRangeBonus, leftHanded, true, typeData, attributeBases)
    }

    private fun writeTypeState(
        data: NbtCompound,
        state: CommonSpawnState,
    ) {
        for ((key, value) in state.typeData) data[key] = value
    }

    /** Writes the vanilla fields that let generated mobs survive materialization and chunk saves. */
    private fun writeMobState(
        data: NbtCompound,
        type: Identifier,
        state: CommonSpawnState,
    ) {
        if (!state.ranMobFinalizer) return
        data.putBoolean("LeftHanded", state.leftHanded)
        val packedAttributes =
            state.attributeBases.map { (id, base) -> packedAttribute(id, base) }.toMutableList()
        EntityAttributes.baseValue(type, EntityAttributes.FOLLOW_RANGE)?.let { base ->
            val modifier =
                NbtCompound().apply {
                    putString("id", RANDOM_SPAWN_BONUS_ID)
                    this["amount"] = NbtDouble(state.followRangeBonus)
                    putString("operation", AttributeOperation.ADD_MULTIPLIED_BASE.serializedName)
                }
            packedAttributes += packedAttribute(FOLLOW_RANGE_ATTRIBUTE, base, listOf(modifier))
        }
        if (packedAttributes.isNotEmpty()) data["attributes"] = NbtList(NbtType.COMPOUND, packedAttributes)
    }

    private fun packedAttribute(
        id: String,
        base: Double,
        modifiers: List<NbtCompound> = emptyList(),
    ): NbtCompound =
        NbtCompound().apply {
            putString("id", id)
            this["base"] = NbtDouble(base)
            if (modifiers.isNotEmpty()) this["modifiers"] = NbtList(NbtType.COMPOUND, modifiers)
        }

    internal data class CommonSpawnState(
        val age: Int,
        val cubeSize: Int,
        val followRangeBonus: Double,
        val leftHanded: Boolean,
        val ranMobFinalizer: Boolean,
        val typeData: NbtCompound,
        val attributeBases: Map<String, Double>,
    )

    /** The AgeableMobGroupData selected by each concrete natural-spawn class. */
    internal fun babySpawnChance(type: String): Float? =
        when (type) {
            // These concrete classes construct AgeableMobGroupData(false).
            "magma_cube", "parrot", "slime", "strider", "villager", "wandering_trader", "wolf" -> null
            // Their custom group data disables the random roll and makes only member three onward a baby.
            "axolotl", "fox" -> null
            "dolphin" -> 0.1f
            "donkey", "mule", "panda", "zombie_horse" -> 0.2f
            "ocelot", "polar_bear", "rabbit" -> 1f
            else -> BABY_SPAWN_CHANCE
        }

    private const val BABY_SPAWN_CHANCE = 0.05f
    private const val FORCED_BABY_GROUP_INDEX = 2
    private const val LEFT_HANDED_CHANCE = 0.05f
    private const val FOLLOW_RANGE_BONUS_SPREAD = 0.11485000000000001
    private const val FOLLOW_RANGE_ATTRIBUTE = "minecraft:follow_range"
    private const val RANDOM_SPAWN_BONUS_ID = "minecraft:random_spawn_bonus"
    private const val MAX_HEALTH_ATTRIBUTE = "minecraft:max_health"
    private const val MAX_NATURAL_SIZE_SCALE = 2
}
