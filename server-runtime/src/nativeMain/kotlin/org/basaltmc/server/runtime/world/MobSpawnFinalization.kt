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

/** Mutable concrete `SpawnGroupData` shared by accepted members of one natural pack. */
public class MobSpawnGroupData {
    internal var variant: Int? = null
    internal var secondaryVariant: Int? = null
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
    ) {
        val state = stateFor(entity.type, groupIndex, random, specialMultiplier, group, biome)
        entity.age = state.age
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
        group: MobSpawnGroupData = MobSpawnGroupData(),
        biome: Identifier? = null,
    ) {
        val state = stateFor(type, groupIndex, random, specialMultiplier, group, biome)
        if (state.age != 0) data.putInt("Age", state.age)
        writeTypeState(data, state)
        writeMobState(data, type, state)
        // parity: AbstractCubeMob saves one less than its live size, so absent/zero means size one.
        if (CubeMobs.isCube(type)) data.putInt("Size", state.cubeSize - 1)
    }

    internal fun stateFor(
        type: Identifier,
        groupIndex: Int,
        random: BasaltRandom,
        specialMultiplier: Float,
        group: MobSpawnGroupData = MobSpawnGroupData(),
        biome: Identifier? = null,
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
        writePreCommonTypeState(type.path, biome?.path, group, random, typeData)
        writeHorseAttributes(type.path, random, attributeBases)
        writePandaAttributes(type.path, typeData, attributeBases)
        val babyChance = babySpawnChance(type.path)
        val age =
            when {
                !AgeableMobs.isAgeable(type) || groupIndex <= 0 -> 0
                type.path == "axolotl" || type.path == "fox" ->
                    if (groupIndex >= FORCED_BABY_GROUP_INDEX) AgeableMobs.BABY_START_AGE else 0
                babyChance != null && random.nextFloat() <= babyChance -> AgeableMobs.BABY_START_AGE
                else -> 0
            }
        // parity: Mob.finalizeSpawn. RandomSource.triangle consumes two doubles before the
        // left-handed float, even though neither result usually changes what the client draws.
        val followRangeBonus = random.triangle(0.0, FOLLOW_RANGE_BONUS_SPREAD)
        val leftHanded = random.nextFloat() < LEFT_HANDED_CHANCE
        if (!CubeMobs.isCube(type)) {
            writePostCommonTypeState(type.path, group, random, typeData)
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

    /** Concrete finalizers that run after their superclass has consumed the common mob RNG. */
    private fun writePostCommonTypeState(
        type: String,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        data: NbtCompound,
    ) {
        if (type != "tropical_fish") return
        val existing = group.variant
        val packed =
            when {
                existing != null -> existing
                random.nextFloat().toDouble() < TROPICAL_COMMON_CHANCE ->
                    TROPICAL_COMMON_VARIANTS[random.nextInt(TROPICAL_COMMON_VARIANTS.size)].also {
                        group.variant = it
                    }
                else -> {
                    val pattern = random.nextInt(TROPICAL_PATTERNS)
                    val baseColor = random.nextInt(DYE_COLORS)
                    val patternColor = random.nextInt(DYE_COLORS)
                    packTropicalFish(pattern, baseColor, patternColor)
                }
            }
        data.putInt("Variant", packed)
    }

    private fun packTropicalFish(
        pattern: Int,
        baseColor: Int,
        patternColor: Int,
    ): Int {
        val large = if (pattern >= TROPICAL_SMALL_PATTERNS) 1 else 0
        val patternIndex = pattern % TROPICAL_SMALL_PATTERNS
        return large or (patternIndex shl 8) or (baseColor shl 16) or (patternColor shl 24)
    }

    /** Concrete finalizers whose observable state and RNG happen before their superclass call. */
    private fun writePreCommonTypeState(
        type: String,
        biome: String?,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        data: NbtCompound,
    ) {
        when (type) {
            "axolotl" -> {
                val first = group.variant ?: random.nextInt(COMMON_AXOLOTL_VARIANTS).also { group.variant = it }
                val second =
                    group.secondaryVariant
                        ?: random.nextInt(COMMON_AXOLOTL_VARIANTS).also { group.secondaryVariant = it }
                data.putInt("Variant", if (random.nextInt(2) == 0) first else second)
            }
            "chicken" -> {
                data.putString(
                    "variant",
                    registryKey(selectTemperatureVariant(biome, random, WARM_FARM_BIOMES, COLD_FARM_BIOMES)),
                )
                data.putString(
                    "sound_variant",
                    registryKey(CHICKEN_SOUND_VARIANTS[random.nextInt(CHICKEN_SOUND_VARIANTS.size)]),
                )
            }
            "cow" -> {
                data.putString(
                    "variant",
                    registryKey(selectTemperatureVariant(biome, random, WARM_FARM_BIOMES, COLD_FARM_BIOMES)),
                )
                data.putString(
                    "sound_variant",
                    registryKey(COW_SOUND_VARIANTS[random.nextInt(COW_SOUND_VARIANTS.size)]),
                )
            }
            "fox" -> {
                val biomeVariant = if (biome in WHITE_RABBIT_BIOMES) FOX_SNOW else FOX_RED
                val variant = group.variant ?: biomeVariant.also { group.variant = it }
                data.putString("Type", if (variant == FOX_SNOW) "snow" else "red")
                writeFoxEquipment(random, data)
            }
            "frog" -> {
                data.putString(
                    "variant",
                    registryKey(selectTemperatureVariant(biome, random, WARM_FROG_BIOMES, COLD_FROG_BIOMES)),
                )
                writeBrainMemories(
                    data,
                    "minecraft:long_jump_cooldown_ticks" to
                        FROG_MIN_LONG_JUMP_COOLDOWN + random.nextInt(FROG_LONG_JUMP_COOLDOWN_RANGE),
                )
            }
            "goat" -> {
                writeBrainMemories(
                    data,
                    "minecraft:long_jump_cooldown_ticks" to
                        GOAT_MIN_LONG_JUMP_COOLDOWN + random.nextInt(GOAT_LONG_JUMP_COOLDOWN_RANGE),
                    "minecraft:ram_cooldown_ticks" to
                        GOAT_MIN_RAM_COOLDOWN + random.nextInt(GOAT_RAM_COOLDOWN_RANGE),
                )
                data.putBoolean("IsScreamingGoat", random.nextDouble() < GOAT_SCREAMING_CHANCE)
                var hasLeftHorn = true
                var hasRightHorn = true
                if (random.nextFloat() < GOAT_UNIHORN_CHANCE) {
                    if (random.nextBoolean()) hasLeftHorn = false else hasRightHorn = false
                }
                data.putBoolean("HasLeftHorn", hasLeftHorn)
                data.putBoolean("HasRightHorn", hasRightHorn)
            }
            "horse" -> {
                val variant = group.variant ?: random.nextInt(HORSE_VARIANTS).also { group.variant = it }
                val markings = random.nextInt(HORSE_MARKINGS)
                data.putInt("Variant", variant or (markings shl 8))
            }
            "llama", "trader_llama" -> {
                val maxStrength = if (random.nextFloat() < LLAMA_MAX_STRENGTH_CHANCE) 5 else 3
                data.putInt("Strength", 1 + random.nextInt(maxStrength))
                val variant = group.variant ?: random.nextInt(LLAMA_VARIANTS).also { group.variant = it }
                data.putInt("Variant", variant)
            }
            "parrot" -> data.putInt("Variant", random.nextInt(PARROT_VARIANTS))
            "panda" -> {
                data.putString("MainGene", pandaGene(random.nextInt(PANDA_GENE_ROLLS)).serializedName)
                data.putString("HiddenGene", pandaGene(random.nextInt(PANDA_GENE_ROLLS)).serializedName)
            }
            "pig" -> {
                data.putString(
                    "variant",
                    registryKey(selectTemperatureVariant(biome, random, WARM_FARM_BIOMES, COLD_FARM_BIOMES)),
                )
                data.putString(
                    "sound_variant",
                    registryKey(PIG_SOUND_VARIANTS[random.nextInt(PIG_SOUND_VARIANTS.size)]),
                )
            }
            "rabbit" -> {
                val rolled = rabbitVariant(biome, random.nextInt(100))
                val variant = group.variant ?: rolled.also { group.variant = it }
                data.putInt("RabbitType", variant)
            }
            "salmon" -> {
                val roll = random.nextInt(SALMON_TOTAL_WEIGHT)
                data.putString(
                    "type",
                    when {
                        roll < SALMON_SMALL_WEIGHT -> "small"
                        roll < SALMON_SMALL_WEIGHT + SALMON_MEDIUM_WEIGHT -> "medium"
                        else -> "large"
                    },
                )
            }
            "sheep" -> data.putByte("Color", sheepColor(biome, random).toByte())
            "wolf" -> {
                val variant =
                    group.variant
                        ?: selectWolfVariant(biome, random).also { group.variant = it }
                data.putString("variant", registryKey(WOLF_VARIANTS[variant]))
                data.putString(
                    "sound_variant",
                    registryKey(WOLF_SOUND_VARIANTS[random.nextInt(WOLF_SOUND_VARIANTS.size)]),
                )
            }
        }
    }

    private fun selectTemperatureVariant(
        biome: String?,
        random: BasaltRandom,
        warmBiomes: Set<String>,
        coldBiomes: Set<String>,
    ): String {
        // PriorityProvider.pick still calls nextInt(1) after reducing the registry to one match.
        random.nextInt(1)
        return when (biome) {
            in warmBiomes -> "warm"
            in coldBiomes -> "cold"
            else -> "temperate"
        }
    }

    private fun selectWolfVariant(
        biome: String?,
        random: BasaltRandom,
    ): Int {
        random.nextInt(1)
        return when {
            biome in SAVANNA_BIOMES -> 1
            biome == "grove" -> 2
            biome == "old_growth_pine_taiga" -> 3
            biome == "snowy_taiga" -> 4
            biome in JUNGLE_BIOMES -> 5
            biome == "forest" -> 6
            biome == "old_growth_spruce_taiga" -> 7
            biome in BADLANDS_BIOMES -> 8
            else -> 0
        }
    }

    private fun registryKey(path: String): String = "minecraft:$path"

    private fun writeBrainMemories(
        data: NbtCompound,
        vararg values: Pair<String, Int>,
    ) {
        val memories =
            NbtCompound().apply {
                values.forEach { (key, value) ->
                    this[key] = NbtCompound().apply { putInt("value", value) }
                }
            }
        data["Brain"] = NbtCompound().apply { this["memories"] = memories }
    }

    private fun writeFoxEquipment(
        random: BasaltRandom,
        data: NbtCompound,
    ) {
        if (random.nextFloat() >= FOX_EQUIPMENT_CHANCE) return
        val roll = random.nextFloat()
        val item =
            when {
                roll < 0.05f -> "emerald"
                roll < 0.2f -> "egg"
                roll < 0.4f -> if (random.nextInt(2) == 0) "rabbit_foot" else "rabbit_hide"
                roll < 0.6f -> "wheat"
                roll < 0.8f -> "leather"
                else -> "feather"
            }
        val stack =
            NbtCompound().apply {
                putString("id", "minecraft:$item")
                putInt("count", 1)
            }
        data["equipment"] = NbtCompound().apply { this["mainhand"] = stack }
    }

    /** `AbstractHorse.randomizeAttributes`, dispatched through each concrete override. */
    private fun writeHorseAttributes(
        type: String,
        random: BasaltRandom,
        attributes: MutableMap<String, Double>,
    ) {
        when (type) {
            "horse" -> {
                attributes[MAX_HEALTH_ATTRIBUTE] = horseMaxHealth(random)
                attributes[MOVEMENT_SPEED_ATTRIBUTE] = horseSpeed(random)
                attributes[JUMP_STRENGTH_ATTRIBUTE] = horseJumpStrength(random)
            }
            "donkey", "llama", "mule", "trader_llama" ->
                attributes[MAX_HEALTH_ATTRIBUTE] = horseMaxHealth(random)
            "skeleton_horse" -> attributes[JUMP_STRENGTH_ATTRIBUTE] = horseJumpStrength(random)
        }
    }

    private fun horseMaxHealth(random: BasaltRandom): Double =
        (HORSE_BASE_HEALTH + random.nextInt(8) + random.nextInt(9)).toDouble()

    private fun horseSpeed(random: BasaltRandom): Double =
        (0.45f + random.nextDouble() * 0.3 + random.nextDouble() * 0.3 + random.nextDouble() * 0.3) * 0.25

    private fun horseJumpStrength(random: BasaltRandom): Double =
        0.4f + random.nextDouble() * 0.2 + random.nextDouble() * 0.2 + random.nextDouble() * 0.2

    private fun writePandaAttributes(
        type: String,
        data: NbtCompound,
        attributes: MutableMap<String, Double>,
    ) {
        if (type != "panda") return
        val main = PANDA_GENES_BY_NAME[data.getString("MainGene")] ?: PandaGene.NORMAL
        val hidden = PANDA_GENES_BY_NAME[data.getString("HiddenGene")] ?: PandaGene.NORMAL
        val expressed = if (main.recessive && main != hidden) PandaGene.NORMAL else main
        when (expressed) {
            PandaGene.WEAK -> attributes[MAX_HEALTH_ATTRIBUTE] = PANDA_WEAK_HEALTH
            PandaGene.LAZY -> attributes[MOVEMENT_SPEED_ATTRIBUTE] = PANDA_LAZY_SPEED
            else -> Unit
        }
    }

    private fun pandaGene(roll: Int): PandaGene =
        when (roll) {
            0 -> PandaGene.LAZY
            1 -> PandaGene.WORRIED
            2 -> PandaGene.PLAYFUL
            4 -> PandaGene.AGGRESSIVE
            in 0..8 -> PandaGene.WEAK
            9, 10 -> PandaGene.BROWN
            else -> PandaGene.NORMAL
        }

    private fun rabbitVariant(
        biome: String?,
        roll: Int,
    ): Int =
        when {
            biome in WHITE_RABBIT_BIOMES -> if (roll < 80) RABBIT_WHITE else RABBIT_WHITE_SPLOTCHED
            biome == "desert" -> RABBIT_GOLD
            roll < 50 -> RABBIT_BROWN
            roll < 90 -> RABBIT_SALT
            else -> RABBIT_BLACK
        }

    /** `SheepColorSpawnRules`, including the nested 499:1 default-to-pink provider. */
    private fun sheepColor(
        biome: String?,
        random: BasaltRandom,
    ): Int {
        val outer = random.nextInt(100)
        return when {
            biome in WARM_FARM_BIOMES ->
                when {
                    outer < 5 -> DYE_GRAY
                    outer < 10 -> DYE_LIGHT_GRAY
                    outer < 15 -> DYE_WHITE
                    outer < 18 -> DYE_BLACK
                    else -> if (random.nextInt(500) == 499) DYE_PINK else DYE_BROWN
                }
            biome in COLD_FARM_BIOMES ->
                when {
                    outer < 5 -> DYE_LIGHT_GRAY
                    outer < 10 -> DYE_GRAY
                    outer < 15 -> DYE_WHITE
                    outer < 18 -> DYE_BROWN
                    else -> if (random.nextInt(500) == 499) DYE_PINK else DYE_BLACK
                }
            else ->
                when {
                    outer < 5 -> DYE_BLACK
                    outer < 10 -> DYE_GRAY
                    outer < 15 -> DYE_LIGHT_GRAY
                    outer < 18 -> DYE_BROWN
                    else -> if (random.nextInt(500) == 499) DYE_PINK else DYE_WHITE
                }
        }
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
    private const val MOVEMENT_SPEED_ATTRIBUTE = "minecraft:movement_speed"
    private const val JUMP_STRENGTH_ATTRIBUTE = "minecraft:jump_strength"
    private const val HORSE_BASE_HEALTH = 15
    private const val HORSE_VARIANTS = 7
    private const val HORSE_MARKINGS = 5
    private const val COMMON_AXOLOTL_VARIANTS = 4
    private const val FOX_RED = 0
    private const val FOX_SNOW = 1
    private const val FOX_EQUIPMENT_CHANCE = 0.2f
    private const val PANDA_GENE_ROLLS = 16
    private const val PANDA_WEAK_HEALTH = 10.0
    private const val PANDA_LAZY_SPEED = 0.07000000029802322
    private const val FROG_MIN_LONG_JUMP_COOLDOWN = 100
    private const val FROG_LONG_JUMP_COOLDOWN_RANGE = 41
    private const val GOAT_MIN_LONG_JUMP_COOLDOWN = 600
    private const val GOAT_LONG_JUMP_COOLDOWN_RANGE = 601
    private const val GOAT_MIN_RAM_COOLDOWN = 600
    private const val GOAT_RAM_COOLDOWN_RANGE = 5_401
    private const val GOAT_SCREAMING_CHANCE = 0.02
    private const val GOAT_UNIHORN_CHANCE = 0.1f
    private const val TROPICAL_COMMON_CHANCE = 0.9
    private const val TROPICAL_PATTERNS = 12
    private const val TROPICAL_SMALL_PATTERNS = 6
    private const val DYE_COLORS = 16
    private const val LLAMA_MAX_STRENGTH_CHANCE = 0.04f
    private const val LLAMA_VARIANTS = 4
    private const val PARROT_VARIANTS = 5
    private const val SALMON_SMALL_WEIGHT = 30
    private const val SALMON_MEDIUM_WEIGHT = 50
    private const val SALMON_TOTAL_WEIGHT = 95
    private const val RABBIT_BROWN = 0
    private const val RABBIT_WHITE = 1
    private const val RABBIT_BLACK = 2
    private const val RABBIT_WHITE_SPLOTCHED = 3
    private const val RABBIT_GOLD = 4
    private const val RABBIT_SALT = 5
    private const val DYE_WHITE = 0
    private const val DYE_PINK = 6
    private const val DYE_GRAY = 7
    private const val DYE_LIGHT_GRAY = 8
    private const val DYE_BROWN = 12
    private const val DYE_BLACK = 15
    private const val MAX_NATURAL_SIZE_SCALE = 2
    private const val GLOBAL_TIME_OFFSET = 72_000f
    private const val MAX_GLOBAL_TIME = 1_440_000f
    private const val MAX_LOCAL_TIME = 3_600_000f
    private const val BASE_SCALE = 0.75f
    private const val GLOBAL_TIME_WEIGHT = 0.25f
    private const val MOON_WEIGHT = 0.25f

    private enum class PandaGene(
        val serializedName: String,
        val id: Int,
        val recessive: Boolean = false,
    ) {
        NORMAL("normal", 0),
        LAZY("lazy", 1),
        WORRIED("worried", 2),
        PLAYFUL("playful", 3),
        BROWN("brown", 4, true),
        WEAK("weak", 5, true),
        AGGRESSIVE("aggressive", 6),
    }

    private val PANDA_GENES_BY_NAME = PandaGene.entries.associateBy(PandaGene::serializedName)

    private val CHICKEN_SOUND_VARIANTS = listOf("classic", "picky")
    private val COW_SOUND_VARIANTS = listOf("classic", "moody")
    private val PIG_SOUND_VARIANTS = listOf("classic", "big", "mini")
    private val WOLF_VARIANTS =
        listOf("pale", "spotted", "snowy", "black", "ashen", "rusty", "woods", "chestnut", "striped")
    private val WOLF_SOUND_VARIANTS = listOf("classic", "puglin", "sad", "angry", "grumpy", "big", "cute")

    private val TROPICAL_COMMON_VARIANTS =
        listOf(
            packTropicalFish(7, 1, 7),
            packTropicalFish(6, 7, 7),
            packTropicalFish(6, 7, 11),
            packTropicalFish(11, 0, 7),
            packTropicalFish(1, 11, 7),
            packTropicalFish(0, 1, 0),
            packTropicalFish(5, 6, 3),
            packTropicalFish(9, 10, 4),
            packTropicalFish(11, 0, 14),
            packTropicalFish(5, 0, 4),
            packTropicalFish(8, 0, 7),
            packTropicalFish(11, 0, 1),
            packTropicalFish(3, 9, 6),
            packTropicalFish(4, 5, 3),
            packTropicalFish(10, 14, 0),
            packTropicalFish(2, 7, 14),
            packTropicalFish(9, 14, 0),
            packTropicalFish(6, 0, 4),
            packTropicalFish(0, 14, 0),
            packTropicalFish(1, 7, 0),
            packTropicalFish(3, 9, 4),
            packTropicalFish(6, 4, 4),
        )

    private val WHITE_RABBIT_BIOMES =
        setOf(
            "frozen_ocean",
            "frozen_peaks",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jagged_peaks",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
        )

    private val WARM_FARM_BIOMES =
        setOf(
            "badlands",
            "bamboo_jungle",
            "basalt_deltas",
            "crimson_forest",
            "deep_lukewarm_ocean",
            "desert",
            "eroded_badlands",
            "jungle",
            "lukewarm_ocean",
            "mangrove_swamp",
            "nether_wastes",
            "savanna",
            "savanna_plateau",
            "soul_sand_valley",
            "sparse_jungle",
            "warm_ocean",
            "warped_forest",
            "windswept_savanna",
            "wooded_badlands",
        )

    private val COLD_FARM_BIOMES =
        setOf(
            "cold_ocean",
            "deep_cold_ocean",
            "deep_dark",
            "deep_frozen_ocean",
            "end_barrens",
            "end_highlands",
            "end_midlands",
            "frozen_ocean",
            "frozen_peaks",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jagged_peaks",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "small_end_islands",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
            "stony_peaks",
            "taiga",
            "the_end",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
        )

    private val WARM_FROG_BIOMES =
        setOf(
            "badlands",
            "bamboo_jungle",
            "basalt_deltas",
            "crimson_forest",
            "desert",
            "eroded_badlands",
            "jungle",
            "mangrove_swamp",
            "nether_wastes",
            "savanna",
            "savanna_plateau",
            "soul_sand_valley",
            "sparse_jungle",
            "warm_ocean",
            "warped_forest",
            "windswept_savanna",
            "wooded_badlands",
        )

    private val COLD_FROG_BIOMES =
        setOf(
            "deep_dark",
            "deep_frozen_ocean",
            "end_barrens",
            "end_highlands",
            "end_midlands",
            "frozen_ocean",
            "frozen_peaks",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jagged_peaks",
            "small_end_islands",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
            "the_end",
        )

    private val SAVANNA_BIOMES = setOf("savanna", "savanna_plateau", "windswept_savanna")
    private val JUNGLE_BIOMES = setOf("bamboo_jungle", "jungle", "sparse_jungle")
    private val BADLANDS_BIOMES = setOf("badlands", "eroded_badlands", "wooded_badlands")
}
