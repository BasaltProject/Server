package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.registry.isLeavesState

/** Block tags and deferred replacement rules shared by tree generation and chunk delivery. */
public object TreeBlockRules {
    public fun isLogState(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path in LOG_BLOCK_PATHS

    public fun isValidTreeState(stateId: Int): Boolean =
        Blocks.isAir(stateId) || Blocks.byStateId(stateId)?.block?.path in REPLACEABLE_BY_TREES_PATHS

    /** Rechecks a tree write against the target after that chunk has independently decorated. */
    public fun canReplaceDeferred(
        actualStateId: Int,
        generatedStateId: Int,
    ): Boolean {
        val actualPath = Blocks.byStateId(actualStateId)?.block?.path
        return when {
            isLogState(generatedStateId) -> isValidTreeState(actualStateId)
            Blocks.isLeavesState(generatedStateId) ->
                Blocks.byStateId(actualStateId)?.properties?.get("persistent") != "true" &&
                    isValidTreeState(actualStateId)
            Blocks.byStateId(generatedStateId)?.block?.path == "mangrove_roots" ->
                isValidTreeState(actualStateId) || actualPath in MANGROVE_ROOT_GROW_THROUGH_PATHS
            Blocks.byStateId(generatedStateId)?.block?.path == "muddy_mangrove_roots" ->
                actualPath in MUDDY_ROOTS_IN_PATHS
            else -> false
        }
    }

    /** Waterloggable tree states retain the target fluid after deferred delivery. */
    public fun resolveDeferredState(
        actualStateId: Int,
        generatedStateId: Int,
    ): Int {
        val generated = Blocks.byStateId(generatedStateId) ?: return generatedStateId
        if ("waterlogged" !in generated.properties) return generatedStateId
        val waterlogged =
            if (Blocks.isLeavesState(generatedStateId)) {
                hasSourceWaterFluid(actualStateId)
            } else {
                Blocks.hasWaterFluid(actualStateId)
            }.toString()
        return Blocks.stateIdOf(generated.block, generated.properties + ("waterlogged" to waterlogged))
            ?: generatedStateId
    }

    // parity: BlockTags.LOGS, expanded from the 26.2 server data pack.
    public val LOG_BLOCK_PATHS: Set<String> =
        buildSet {
            for (wood in listOf(
                "dark_oak",
                "pale_oak",
                "oak",
                "acacia",
                "birch",
                "jungle",
                "spruce",
                "mangrove",
                "cherry",
            )) {
                add("${wood}_log")
                add("${wood}_wood")
                add("stripped_${wood}_log")
                add("stripped_${wood}_wood")
            }
            addAll(
                listOf(
                    "crimson_stem",
                    "stripped_crimson_stem",
                    "crimson_hyphae",
                    "stripped_crimson_hyphae",
                    "warped_stem",
                    "stripped_warped_stem",
                    "warped_hyphae",
                    "stripped_warped_hyphae",
                ),
            )
        }

    // parity: minecraft:replaceable_by_trees, flattened from its leaf and flower tags.
    private val REPLACEABLE_BY_TREES_PATHS =
        setOf(
            "jungle_leaves",
            "oak_leaves",
            "spruce_leaves",
            "pale_oak_leaves",
            "dark_oak_leaves",
            "acacia_leaves",
            "birch_leaves",
            "azalea_leaves",
            "flowering_azalea_leaves",
            "mangrove_leaves",
            "cherry_leaves",
            "dandelion",
            "open_eyeblossom",
            "poppy",
            "blue_orchid",
            "allium",
            "azure_bluet",
            "red_tulip",
            "orange_tulip",
            "white_tulip",
            "pink_tulip",
            "oxeye_daisy",
            "cornflower",
            "lily_of_the_valley",
            "wither_rose",
            "torchflower",
            "closed_eyeblossom",
            "golden_dandelion",
            "pale_moss_carpet",
            "short_grass",
            "fern",
            "dead_bush",
            "vine",
            "glow_lichen",
            "sunflower",
            "lilac",
            "rose_bush",
            "peony",
            "tall_grass",
            "large_fern",
            "hanging_roots",
            "pitcher_plant",
            "water",
            "seagrass",
            "tall_seagrass",
            "bush",
            "firefly_bush",
            "warped_roots",
            "nether_sprouts",
            "crimson_roots",
            "leaf_litter",
            "short_dry_grass",
            "tall_dry_grass",
        )

    // parity: MangroveRootPlacement tags used by MangroveRootPlacer.canPlaceRoot/placeRoot.
    private val MANGROVE_ROOT_GROW_THROUGH_PATHS =
        setOf("mud", "muddy_mangrove_roots", "mangrove_roots", "moss_carpet", "vine", "mangrove_propagule", "snow")
    private val MUDDY_ROOTS_IN_PATHS = setOf("mud", "muddy_mangrove_roots")

    private fun hasSourceWaterFluid(stateId: Int): Boolean =
        Blocks.hasWaterFluid(stateId) && stateId !in Blocks.WATER.stateId + 1..Blocks.WATER.stateId + 15
}
