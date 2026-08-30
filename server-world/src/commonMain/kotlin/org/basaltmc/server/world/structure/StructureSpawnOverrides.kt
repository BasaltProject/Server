package org.basaltmc.server.world.structure

/**
 * // parity: the `spawn_overrides` field of the five vanilla structures that carry one. An empty spawn
 * list is meaningful: it silences a whole category inside the structure.
 */
public object StructureSpawnOverrides {
    /** Which box a category's override is tested against. // parity: `BoundingBoxType`. */
    public enum class Bounds { PIECE, FULL }

    public data class SpawnEntry(
        val type: String,
        val weight: Int,
        val minCount: Int,
        val maxCount: Int,
    )

    public data class Override(
        val bounds: Bounds,
        val spawns: List<SpawnEntry>,
    )

    private fun entry(
        type: String,
        weight: Int,
        min: Int,
        max: Int,
    ) = SpawnEntry("minecraft:$type", weight, min, max)

    private val FORTRESS_MONSTERS =
        listOf(
            entry("blaze", 10, 2, 3),
            entry("zombified_piglin", 5, 4, 4),
            entry("wither_skeleton", 8, 5, 5),
            entry("skeleton", 2, 5, 5),
            entry("magma_cube", 3, 4, 4),
        )

    private val EMPTY_FULL = Override(Bounds.FULL, emptyList())

    private val BY_STRUCTURE: Map<String, Map<String, Override>> =
        mapOf(
            "fortress" to mapOf("monster" to Override(Bounds.PIECE, FORTRESS_MONSTERS)),
            "monument" to
                mapOf(
                    "monster" to Override(Bounds.FULL, listOf(entry("guardian", 1, 2, 4))),
                    "axolotls" to EMPTY_FULL,
                    "underground_water_creature" to EMPTY_FULL,
                ),
            "pillager_outpost" to
                mapOf("monster" to Override(Bounds.FULL, listOf(entry("pillager", 1, 1, 1)))),
            "swamp_hut" to
                mapOf(
                    "monster" to Override(Bounds.PIECE, listOf(entry("witch", 1, 1, 1))),
                    "creature" to Override(Bounds.PIECE, listOf(entry("cat", 1, 1, 1))),
                ),
            "ancient_city" to
                listOf(
                    "monster",
                    "creature",
                    "ambient",
                    "axolotls",
                    "underground_water_creature",
                    "water_creature",
                    "water_ambient",
                    "misc",
                ).associateWith { EMPTY_FULL },
        )

    /** The structures that override anything, so a caller can skip the rest of the index. */
    public val STRUCTURES: Set<String> = BY_STRUCTURE.keys

    public fun of(
        structure: String,
        category: String,
    ): Override? = BY_STRUCTURE[structure]?.get(category)
}
