package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.world.MobSpawnVariantData.BADLANDS_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.CHICKEN_SOUND_VARIANTS
import org.basaltmc.server.runtime.world.MobSpawnVariantData.COLD_FARM_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.COLD_FROG_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.COW_SOUND_VARIANTS
import org.basaltmc.server.runtime.world.MobSpawnVariantData.JUNGLE_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.PIG_SOUND_VARIANTS
import org.basaltmc.server.runtime.world.MobSpawnVariantData.SAVANNA_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.TROPICAL_COMMON_VARIANTS
import org.basaltmc.server.runtime.world.MobSpawnVariantData.WARM_FARM_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.WARM_FROG_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.WHITE_RABBIT_BIOMES
import org.basaltmc.server.runtime.world.MobSpawnVariantData.WOLF_SOUND_VARIANTS
import org.basaltmc.server.runtime.world.MobSpawnVariantData.WOLF_VARIANTS
import org.basaltmc.wire.nbt.NbtCompound

/** Concrete variant, birth-state and memory layers around the shared mob finalizer. */
internal object MobVariantSpawnFinalizer {
    /** Concrete finalizers whose observable state and RNG happen before their superclass call. */
    fun beforeCommon(
        type: String,
        biome: String?,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        data: NbtCompound,
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        gameTime: Long,
        specialMultiplier: Float,
        equipmentFinalizer: SpawnEquipmentFinalizer,
        entityRandom: BasaltRandom,
        reason: MobSpawnReason,
    ): Boolean {
        var concreteBaby = false
        VillagerSpawnFinalizer.beforeCommon(type, biome, reason, entityRandom, data)
        when (type) {
            "axolotl" -> {
                val first = group.variant ?: random.nextInt(COMMON_AXOLOTL_VARIANTS).also { group.variant = it }
                val second =
                    group.secondaryVariant
                        ?: random.nextInt(COMMON_AXOLOTL_VARIANTS).also { group.secondaryVariant = it }
                data.putInt("Variant", if (random.nextInt(2) == 0) first else second)
            }
            // parity: Camel.finalizeSpawn starts every natural camel in the fully standing pose.
            "camel", "camel_husk" -> data.putLong("LastPoseTick", maxOf(0L, gameTime - CAMEL_FULL_STAND_TICKS))
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
            "dolphin" -> {
                data.putShort("Air", DOLPHIN_MAX_AIR.toShort())
                data.putBoolean("CanPickUpLoot", true)
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
            "hoglin" -> concreteBaby = random.nextFloat() < HOGLIN_BABY_CHANCE
            "llama", "trader_llama" -> {
                val maxStrength = if (random.nextFloat() < LLAMA_MAX_STRENGTH_CHANCE) 5 else 3
                data.putInt("Strength", 1 + random.nextInt(maxStrength))
                val variant = group.variant ?: random.nextInt(LLAMA_VARIANTS).also { group.variant = it }
                data.putInt("Variant", variant)
            }
            "nautilus" ->
                writeBrainMemories(
                    data,
                    "minecraft:attack_target_cooldown" to
                        NAUTILUS_MIN_ATTACK_COOLDOWN + random.nextInt(NAUTILUS_ATTACK_COOLDOWN_RANGE),
                )
            "parrot" -> data.putInt("Variant", random.nextInt(PARROT_VARIANTS))
            "phantom" -> {
                data.putIntArray("anchor_pos", intArrayOf(blockX, blockY + PHANTOM_ANCHOR_HEIGHT, blockZ))
                data.putInt("size", 0)
            }
            "pillager" ->
                PillagerSpawnFinalizer.beforeCommon(
                    group,
                    random,
                    entityRandom,
                    specialMultiplier,
                    reason,
                    equipmentFinalizer,
                    data,
                )
            "piglin" ->
                concreteBaby =
                    PiglinSpawnFinalizer.finalizeBeforeCommon(
                        data,
                        random,
                        entityRandom,
                        specialMultiplier,
                        equipmentFinalizer,
                        reason,
                    )
            "panda" -> {
                data.putString(
                    "MainGene",
                    MobSpawnAttributes.pandaGene(random.nextInt(PANDA_GENE_ROLLS)).serializedName,
                )
                data.putString(
                    "HiddenGene",
                    MobSpawnAttributes.pandaGene(random.nextInt(PANDA_GENE_ROLLS)).serializedName,
                )
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
            // parity: Turtle.finalizeSpawn anchors navigation and egg laying to its birth block.
            "turtle" -> data.putIntArray("home_pos", intArrayOf(blockX, blockY, blockZ))
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
            "zombie_nautilus" -> {
                // PriorityProvider still consumes nextInt(1) after choosing the highest-priority match.
                random.nextInt(1)
                data.putString("variant", registryKey(if (biome == "warm_ocean") "warm" else "temperate"))
                writeBrainMemories(
                    data,
                    "minecraft:attack_target_cooldown" to
                        NAUTILUS_MIN_ATTACK_COOLDOWN + random.nextInt(NAUTILUS_ATTACK_COOLDOWN_RANGE),
                )
            }
        }
        return concreteBaby
    }

    /** Concrete finalizers that run after their superclass has consumed the common mob RNG. */
    fun afterCommon(
        type: String,
        group: MobSpawnGroupData,
        random: BasaltRandom,
        data: NbtCompound,
    ) {
        if (type == "cat") {
            CatSpawnFinalizer.afterCommon(group.catSpawnContext, random, data)
            return
        }
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

    private const val HORSE_VARIANTS = 7
    private const val HORSE_MARKINGS = 5
    private const val COMMON_AXOLOTL_VARIANTS = 4
    private const val CAMEL_FULL_STAND_TICKS = 53L
    private const val DOLPHIN_MAX_AIR = 4_800
    private const val FOX_RED = 0
    private const val FOX_SNOW = 1
    private const val FOX_EQUIPMENT_CHANCE = 0.2f
    private const val PANDA_GENE_ROLLS = 16
    private const val FROG_MIN_LONG_JUMP_COOLDOWN = 100
    private const val FROG_LONG_JUMP_COOLDOWN_RANGE = 41
    private const val GOAT_MIN_LONG_JUMP_COOLDOWN = 600
    private const val GOAT_LONG_JUMP_COOLDOWN_RANGE = 601
    private const val GOAT_MIN_RAM_COOLDOWN = 600
    private const val GOAT_RAM_COOLDOWN_RANGE = 5_401
    private const val GOAT_SCREAMING_CHANCE = 0.02
    private const val GOAT_UNIHORN_CHANCE = 0.1f
    private const val HOGLIN_BABY_CHANCE = 0.2f
    private const val NAUTILUS_MIN_ATTACK_COOLDOWN = 2_400
    private const val NAUTILUS_ATTACK_COOLDOWN_RANGE = 1_201
    private const val TROPICAL_COMMON_CHANCE = 0.9
    private const val TROPICAL_PATTERNS = 12
    private const val TROPICAL_SMALL_PATTERNS = 6
    private const val DYE_COLORS = 16
    private const val LLAMA_MAX_STRENGTH_CHANCE = 0.04f
    private const val LLAMA_VARIANTS = 4
    private const val PARROT_VARIANTS = 5
    private const val PHANTOM_ANCHOR_HEIGHT = 5
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
}
