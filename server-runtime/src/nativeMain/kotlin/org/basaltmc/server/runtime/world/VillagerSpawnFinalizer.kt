package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.nbt.NbtCompound

/** Minecraft 26.2 villager data selected before the shared ageable/mob finalizers. */
internal object VillagerSpawnFinalizer {
    fun beforeCommon(
        type: String,
        biome: String?,
        reason: MobSpawnReason,
        entityRandom: BasaltRandom,
        data: NbtCompound,
    ) {
        if (type != "villager" && type != "zombie_villager") return

        // parity: ZombieVillager initializes its profession from its own entity RNG in the
        // constructor, before finalizeSpawn selects the biome-derived villager type.
        val profession =
            if (type == "zombie_villager") {
                PROFESSIONS[entityRandom.nextInt(PROFESSIONS.size)]
            } else {
                "none"
            }
        data["VillagerData"] =
            NbtCompound().apply {
                putString("type", "minecraft:${villagerType(biome)}")
                putString("profession", "minecraft:$profession")
                putInt("level", 1)
            }
        data.putBoolean("VillagerDataFinalized", true)
        if (type == "villager" && reason == MobSpawnReason.STRUCTURE) {
            data.putBoolean("AssignProfessionWhenSpawned", true)
        }
    }

    private fun villagerType(biome: String?): String =
        when (biome) {
            in DESERT_BIOMES -> "desert"
            in JUNGLE_BIOMES -> "jungle"
            in SAVANNA_BIOMES -> "savanna"
            in SNOW_BIOMES -> "snow"
            in SWAMP_BIOMES -> "swamp"
            in TAIGA_BIOMES -> "taiga"
            else -> "plains"
        }

    internal val PROFESSIONS =
        listOf(
            "none",
            "armorer",
            "butcher",
            "cartographer",
            "cleric",
            "farmer",
            "fisherman",
            "fletcher",
            "leatherworker",
            "librarian",
            "mason",
            "nitwit",
            "shepherd",
            "toolsmith",
            "weaponsmith",
        )

    private val DESERT_BIOMES = setOf("badlands", "desert", "eroded_badlands", "wooded_badlands")
    private val JUNGLE_BIOMES = setOf("bamboo_jungle", "jungle", "sparse_jungle")
    private val SAVANNA_BIOMES = setOf("savanna_plateau", "savanna", "windswept_savanna")
    private val SNOW_BIOMES =
        setOf(
            "deep_frozen_ocean",
            "frozen_ocean",
            "frozen_river",
            "ice_spikes",
            "snowy_beach",
            "snowy_taiga",
            "snowy_plains",
            "grove",
            "snowy_slopes",
            "frozen_peaks",
            "jagged_peaks",
        )
    private val SWAMP_BIOMES = setOf("swamp", "mangrove_swamp")
    private val TAIGA_BIOMES =
        setOf(
            "old_growth_spruce_taiga",
            "old_growth_pine_taiga",
            "windswept_gravelly_hills",
            "windswept_hills",
            "taiga",
            "windswept_forest",
        )
}
