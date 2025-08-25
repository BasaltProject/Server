package org.basaltmc.server.runtime.world

import kotlinx.datetime.Month
import kotlinx.datetime.TimeZone
import kotlinx.datetime.toLocalDateTime
import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtFloat
import kotlin.time.Clock

/** Type-specific spawn equipment and its exact enchantment RNG. */
public class SpawnEquipmentFinalizer(
    serverJar: ServerJarAssets?,
) {
    private val enchantments = SpawnEquipmentEnchantments(serverJar)

    internal fun finalizeAfterCommon(
        data: NbtCompound,
        type: String,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
    ) {
        if (type !in ABSTRACT_SKELETON_TYPES) return
        finalizeSkeleton(data, type, random, specialMultiplier, difficulty)
    }

    internal fun finalizePiglinEquipmentBeforeCommon(
        data: NbtCompound,
        random: BasaltRandom,
        specialMultiplier: Float,
        baby: Boolean,
    ) {
        if (!baby) {
            for ((slot, item) in PIGLIN_ARMOR) {
                if (random.nextFloat() < PIGLIN_ARMOR_CHANCE) setItem(data, slot, item)
            }
        }
        enchantSlot(data, "mainhand", random, WEAPON_ENCHANT_CHANCE, specialMultiplier)
        for (slot in ARMOR_ENCHANTMENT_ORDER) {
            enchantSlot(data, slot, random, ARMOR_ENCHANT_CHANCE, specialMultiplier)
        }
    }

    internal fun finalizeZombieEquipmentAfterCommon(
        data: NbtCompound,
        type: String,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
    ) {
        when (type) {
            "drowned" -> {
                if (random.nextFloat() > DROWNED_WEAPON_ROLL) {
                    val item =
                        if (random.nextInt(DROWNED_WEAPON_BOUND) <
                            DROWNED_TRIDENT_RESULTS
                        ) {
                            "trident"
                        } else {
                            "fishing_rod"
                        }
                    setItem(data, "mainhand", item)
                }
            }
            "zombified_piglin" ->
                setItem(
                    data,
                    "mainhand",
                    if (random.nextInt(ZOMBIFIED_PIGLIN_SPEAR_ODDS) == 0) "golden_spear" else "golden_sword",
                )
            else -> {
                populateArmor(data, random, specialMultiplier, difficulty)
                val weaponChance = if (difficulty == "hard") HARD_ZOMBIE_WEAPON_CHANCE else ZOMBIE_WEAPON_CHANCE
                if (random.nextFloat() < weaponChance) {
                    val item =
                        when (random.nextInt(ZOMBIE_WEAPON_BOUND)) {
                            0 -> "iron_sword"
                            1 -> "iron_spear"
                            else -> "iron_shovel"
                        }
                    setItem(data, "mainhand", item)
                }
            }
        }
        enchantSlot(data, "mainhand", random, WEAPON_ENCHANT_CHANCE, specialMultiplier)
        for (slot in ARMOR_ENCHANTMENT_ORDER) {
            enchantSlot(data, slot, random, ARMOR_ENCHANT_CHANCE, specialMultiplier)
        }
        addHalloweenHead(data, random)
    }

    private fun finalizeSkeleton(
        data: NbtCompound,
        type: String,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
    ) {
        if (type == "wither_skeleton") {
            setItem(data, "mainhand", "stone_sword")
        } else {
            populateArmor(data, random, specialMultiplier, difficulty)
            setItem(data, "mainhand", "bow")
            enchantSlot(data, "mainhand", random, WEAPON_ENCHANT_CHANCE, specialMultiplier)
            for (slot in ARMOR_ENCHANTMENT_ORDER) {
                enchantSlot(data, slot, random, ARMOR_ENCHANT_CHANCE, specialMultiplier)
            }
        }
        data.putBoolean("CanPickUpLoot", random.nextFloat() < PICKUP_LOOT_CHANCE * specialMultiplier)
        addHalloweenHead(data, random)
    }

    private fun populateArmor(
        data: NbtCompound,
        random: BasaltRandom,
        specialMultiplier: Float,
        difficulty: String,
    ) {
        if (random.nextFloat() >= ARMOR_CHANCE * specialMultiplier) return
        var tier = random.nextInt(INITIAL_ARMOR_TIERS)
        repeat(ARMOR_UPGRADE_ATTEMPTS) {
            if (random.nextFloat() < ARMOR_UPGRADE_CHANCE) tier++
        }
        val partialChance = if (difficulty == "hard") HARD_PARTIAL_ARMOR_CHANCE else PARTIAL_ARMOR_CHANCE
        for ((index, slot) in ARMOR_POPULATION_ORDER.withIndex()) {
            if (index > 0 && random.nextFloat() < partialChance) break
            if (item(data, slot).size == 0) setItem(data, slot, ARMOR_TIERS[tier][index])
        }
    }

    private fun enchantSlot(
        data: NbtCompound,
        slot: String,
        random: BasaltRandom,
        chance: Float,
        specialMultiplier: Float,
    ) {
        val stack = item(data, slot)
        if (stack.size == 0 || random.nextFloat() >= chance * specialMultiplier) return
        enchantments.enchant(stack, random, specialMultiplier)
    }

    private fun addHalloweenHead(
        data: NbtCompound,
        random: BasaltRandom,
    ) {
        if (item(data, "head").size != 0 || !isHalloween() || random.nextFloat() >= HALLOWEEN_HEAD_CHANCE) return
        val block = if (random.nextFloat() < JACK_O_LANTERN_CHANCE) "jack_o_lantern" else "carved_pumpkin"
        setItem(data, "head", block)
        val dropChances = data.getCompound("drop_chances")
        dropChances["head"] = NbtFloat(0f)
        data["drop_chances"] = dropChances
    }

    private fun item(
        data: NbtCompound,
        slot: String,
    ): NbtCompound = data.getCompound("equipment").getCompound(slot)

    internal fun itemId(
        data: NbtCompound,
        slot: String,
    ): String? = item(data, slot).getString("id").takeIf(String::isNotEmpty)

    internal fun setGuaranteedDrop(
        data: NbtCompound,
        slot: String,
    ) {
        val chances = data.getCompound("drop_chances")
        chances[slot] = NbtFloat(GUARANTEED_DROP_CHANCE)
        data["drop_chances"] = chances
    }

    internal fun setItem(
        data: NbtCompound,
        slot: String,
        item: String,
    ) {
        val equipment = data.getCompound("equipment")
        equipment[slot] =
            NbtCompound().apply {
                putString("id", "minecraft:$item")
                putInt("count", 1)
            }
        data["equipment"] = equipment
    }

    private fun isHalloween(): Boolean {
        val date =
            Clock.System
                .now()
                .toLocalDateTime(TimeZone.currentSystemDefault())
                .date
        return date.month == Month.OCTOBER && date.day == 31
    }

    private companion object {
        const val ARMOR_CHANCE = 0.15f
        const val ARMOR_UPGRADE_CHANCE = 0.1087f
        const val ARMOR_UPGRADE_ATTEMPTS = 3
        const val INITIAL_ARMOR_TIERS = 3
        const val PARTIAL_ARMOR_CHANCE = 0.25f
        const val HARD_PARTIAL_ARMOR_CHANCE = 0.1f
        const val WEAPON_ENCHANT_CHANCE = 0.25f
        const val ARMOR_ENCHANT_CHANCE = 0.5f
        const val PICKUP_LOOT_CHANCE = 0.55f
        const val PIGLIN_ARMOR_CHANCE = 0.1f
        const val ZOMBIE_WEAPON_CHANCE = 0.01f
        const val HARD_ZOMBIE_WEAPON_CHANCE = 0.05f
        const val DROWNED_WEAPON_ROLL = 0.9f
        const val DROWNED_WEAPON_BOUND = 16
        const val DROWNED_TRIDENT_RESULTS = 10
        const val ZOMBIE_WEAPON_BOUND = 6
        const val ZOMBIFIED_PIGLIN_SPEAR_ODDS = 20
        const val HALLOWEEN_HEAD_CHANCE = 0.25f
        const val JACK_O_LANTERN_CHANCE = 0.1f
        const val GUARANTEED_DROP_CHANCE = 2f

        val ABSTRACT_SKELETON_TYPES = setOf("skeleton", "stray", "bogged", "parched", "wither_skeleton")
        val ARMOR_POPULATION_ORDER = listOf("head", "chest", "legs", "feet")
        val ARMOR_ENCHANTMENT_ORDER = listOf("feet", "legs", "chest", "head")
        val PIGLIN_ARMOR =
            listOf(
                "head" to "golden_helmet",
                "chest" to "golden_chestplate",
                "legs" to "golden_leggings",
                "feet" to "golden_boots",
            )
        val ARMOR_TIERS =
            listOf(
                listOf("leather_helmet", "leather_chestplate", "leather_leggings", "leather_boots"),
                listOf("copper_helmet", "copper_chestplate", "copper_leggings", "copper_boots"),
                listOf("golden_helmet", "golden_chestplate", "golden_leggings", "golden_boots"),
                listOf("chainmail_helmet", "chainmail_chestplate", "chainmail_leggings", "chainmail_boots"),
                listOf("iron_helmet", "iron_chestplate", "iron_leggings", "iron_boots"),
                listOf("diamond_helmet", "diamond_chestplate", "diamond_leggings", "diamond_boots"),
            )
    }
}

private val FALLBACK_SPAWN_EQUIPMENT by lazy { SpawnEquipmentFinalizer(null) }

internal fun spawnEquipmentWithoutAssets(): SpawnEquipmentFinalizer = FALLBACK_SPAWN_EQUIPMENT
