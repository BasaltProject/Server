package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * The `minecraft:point_of_interest_type` registry: which blocks each type is made of.
 *
 * parity: `PoiTypes`' own registrations. A built-in registry rather than datapack content, so it is
 * written here rather than read from the jar, and it is a block-to-name mapping like every other
 * table in this module.
 *
 * Kept as block ids, not block *state* ids: a bed, a cauldron or a lightning rod has dozens of
 * states and vanilla registers every one of them, so matching the block is both shorter and exactly
 * as precise.
 */
public object PointsOfInterest {
    private val BEDS: List<String> =
        listOf(
            "white",
            "orange",
            "magenta",
            "light_blue",
            "yellow",
            "lime",
            "pink",
            "gray",
            "light_gray",
            "cyan",
            "purple",
            "blue",
            "brown",
            "green",
            "red",
            "black",
        ).map { "${it}_bed" }

    private val CAULDRONS =
        listOf("cauldron", "water_cauldron", "lava_cauldron", "powder_snow_cauldron")

    private val LIGHTNING_RODS = listOf("lightning_rod")

    private val TYPES: Map<Identifier, Set<Identifier>> =
        buildMap {
            fun put(
                type: String,
                blocks: List<String>,
            ) {
                put(Identifier.vanilla(type), blocks.map(Identifier::vanilla).toSet())
            }
            put("armorer", listOf("blast_furnace"))
            put("butcher", listOf("smoker"))
            put("cartographer", listOf("cartography_table"))
            put("cleric", listOf("brewing_stand"))
            put("farmer", listOf("composter"))
            put("fisherman", listOf("barrel"))
            put("fletcher", listOf("fletching_table"))
            put("leatherworker", CAULDRONS)
            put("librarian", listOf("lectern"))
            put("mason", listOf("stonecutter"))
            put("shepherd", listOf("loom"))
            put("toolsmith", listOf("smithing_table"))
            put("weaponsmith", listOf("grindstone"))
            put("home", BEDS)
            put("meeting", listOf("bell"))
            put("beehive", listOf("beehive"))
            put("bee_nest", listOf("bee_nest"))
            put("nether_portal", listOf("nether_portal"))
            put("lodestone", listOf("lodestone"))
            put("lightning_rod", LIGHTNING_RODS)
            put("test_instance", listOf("test_instance_block"))
        }

    public val ids: Set<Identifier> get() = TYPES.keys

    /** The blocks [type] is made of, or null for a type nobody registered. */
    public operator fun get(type: Identifier): Set<Identifier>? = TYPES[type]
}
