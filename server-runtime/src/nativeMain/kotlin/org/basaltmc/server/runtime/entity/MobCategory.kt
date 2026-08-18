package org.basaltmc.server.runtime.entity

/**
 * The eight kinds a mob can be, and what each is worth to the world.
 *
 * parity: `MobCategory`, whose numbers decide how many of that kind the world keeps per chunk, how
 * far away one despawns, and whether it counts as friendly. They are declared in code rather than
 * data, so they are declared here too, in the same order and with the same values.
 */
public enum class MobCategory(
    public val categoryName: String,
    public val maxPerChunk: Int,
    public val friendly: Boolean,
    public val persistent: Boolean,
    public val despawnDistance: Int,
) {
    MONSTER("monster", 70, false, false, 128),
    CREATURE("creature", 10, true, true, 128),
    AMBIENT("ambient", 15, true, false, 128),
    AXOLOTLS("axolotls", 5, true, false, 128),
    UNDERGROUND_WATER_CREATURE("underground_water_creature", 5, true, false, 128),
    WATER_CREATURE("water_creature", 5, true, false, 128),
    WATER_AMBIENT("water_ambient", 20, true, false, 64),
    MISC("misc", -1, true, true, 128),
    ;

    public companion object {
        /** parity: `MobCategory.noDespawnDistance`, the same 32 for every category. */
        public const val NO_DESPAWN_DISTANCE: Int = 32

        public fun byName(name: String): MobCategory? = entries.firstOrNull { it.categoryName == name }
    }
}
