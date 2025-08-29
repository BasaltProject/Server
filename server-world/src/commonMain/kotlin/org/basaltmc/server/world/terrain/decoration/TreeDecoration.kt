package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeConfiguration
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeFeature
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfiguration
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature
import org.basaltmc.wire.common.Identifier

/**
 * Real per-biome tree placement, ported from the real jar's own decompiled
 * `net.minecraft.data.worldgen.placement.VegetationPlacements`/`BiomeDefaultFeatures` (the
 * `count`/`chance`/`extra` triples and, where a biome's real tree list is a weighted choice
 * between species, the real chance for the non-default species).
 *
 * Real `Feature.RANDOM_SELECTOR` entries are rolled independently in declaration order. Both
 * standing [TreeConfiguration] candidates and fallen-tree candidates share this placement path;
 * `null` is reserved for configured features that remain genuinely unported.
 */
public object TreeDecoration {
    public fun interface Candidate {
        public fun place(
            chunk: Chunk,
            random: WorldgenRandom,
            position: DecorationPos,
            mossyCarpetRandom: BasaltRandom,
        )
    }

    public fun tree(config: TreeConfiguration): Candidate =
        Candidate {
            chunk,
            random,
            position,
            mossyCarpetRandom,
            ->
            TreeFeature.place(chunk, random, position.x, position.y, position.z, config, mossyCarpetRandom)
        }

    public fun fallen(config: FallenTreeConfiguration): Candidate =
        Candidate {
            chunk,
            random,
            position,
            _,
            ->
            FallenTreeFeature.place(chunk, random, position.x, position.y, position.z, config)
        }

    public fun hugeMushroom(kind: HugeMushroomFeature.Kind): Candidate =
        Candidate {
            chunk,
            random,
            position,
            _,
            ->
            HugeMushroomFeature.place(chunk, random, position.x, position.y, position.z, kind)
        }

    private fun checked(
        candidate: Candidate,
        sapling: String,
    ): Candidate =
        Candidate { chunk, random, position, mossyCarpetRandom ->
            if (survivesPlacementPredicate(chunk, position, sapling)) {
                candidate.place(chunk, random, position, mossyCarpetRandom)
            }
        }

    private fun checkedTree(
        config: TreeConfiguration,
        sapling: String,
    ): Candidate = checked(tree(config), sapling)

    private fun checkedFallen(
        config: FallenTreeConfiguration,
        sapling: String,
    ): Candidate = checked(fallen(config), sapling)

    private fun onSnow(config: TreeConfiguration): Candidate {
        val candidate = tree(config)
        return Candidate { chunk, random, position, mossyCarpetRandom ->
            grovePosition(chunk, position)?.let { candidate.place(chunk, random, it, mossyCarpetRandom) }
        }
    }

    /** Exact `would_survive` predicate shared by outer tree placements and nested selector entries. */
    internal fun survivesPlacementPredicate(
        chunk: Chunk,
        position: DecorationPos,
        sapling: String?,
    ): Boolean {
        if (sapling == null) return true
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        val stateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(sapling))?.stateId
                ?: error("unknown sapling '$sapling'")

        fun get(
            dx: Int,
            dy: Int,
            dz: Int,
        ): Int = chunk.getDecorationBlockStateId(position.x + dx, position.y + dy, position.z + dz)

        return BlockSupport.canSurvive(
            stateId,
            down = get(0, -1, 0),
            up = get(0, 1, 0),
            north = get(0, 0, -1),
            south = get(0, 0, 1),
            west = get(-1, 0, 0),
            east = get(1, 0, 0),
        )
    }

    /** Nested `pine_on_snow`/`spruce_on_snow`: scan upward through at most eight powder-snow blocks. */
    private fun grovePosition(
        chunk: Chunk,
        origin: DecorationPos,
    ): DecorationPos? {
        var position = origin
        var steps = 0
        while (steps < GROVE_SCAN_STEPS && blockPath(chunk, position) == "powder_snow") {
            if (position.y == WorldHeight.MAX_Y) return null
            position = position.copy(y = position.y + 1)
            steps++
        }
        if (blockPath(chunk, position) == "powder_snow") return null
        val below = position.copy(y = position.y - 1)
        return position.takeIf { blockPath(chunk, below) in GROVE_SUPPORT_PATHS }
    }

    private fun blockPath(
        chunk: Chunk,
        position: DecorationPos,
    ): String? = Blocks.byStateId(chunk.getDecorationBlockStateId(position.x, position.y, position.z))?.block?.path

    /** // parity: `trees_water`, weighted count 0x9/1x1 and fancy oak 10% before oak fallback. */
    public val WATER: BiomeVegetalDecoration =
        providerCountChoice(
            TreeConfigs.FANCY_OAK,
            0.1f,
            TreeConfigs.OAK,
            IntProvider.Weighted(0 to 9, 1 to 1),
        )

    private fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        frequency: PlacementModifier,
        pick: (WorldgenRandom) -> Candidate?,
    ) {
        val context = PlacementContext(chunk)
        val origin = DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16)
        var positions = frequency.getPositions(context, random, origin)
        positions = positions.flatMap { InSquarePlacement.getPositions(context, random, it) }
        positions = positions.flatMap { SurfaceWaterDepthFilter(0).getPositions(context, random, it) }
        positions =
            positions.flatMap {
                HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR).getPositions(context, random, it)
            }
        positions.forEach { pos -> pick(random)?.place(chunk, random, pos, random) }
    }

    public fun single(
        config: TreeConfiguration,
        count: Int,
        chance: Float,
        extra: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration {
            chunk,
            random,
            ->
            place(chunk, random, CountExtraPlacement(count, chance, extra)) { tree(config) }
        }

    /** Matches a real plain `CountPlacement.of(count)` frequency (no weighted "extra" roll), e.g. `pale_garden_vegetation`. */
    public fun fixed(
        config: TreeConfiguration,
        count: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration {
            chunk,
            random,
            ->
            place(chunk, random, CountPlacement(count)) { tree(config) }
        }

    /** [rareConfig] is picked with probability [rareChance] (a single `nextFloat()` draw), otherwise [defaultConfig]: matches a two-entry real `RANDOM_SELECTOR` with every other real entry dropped (see this file's own KDoc). */
    public fun choice(
        rareConfig: TreeConfiguration,
        rareChance: Float,
        defaultConfig: TreeConfiguration,
        count: Int,
        chance: Float,
        extra: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val rare = tree(rareConfig)
            val default = tree(defaultConfig)
            place(chunk, random, CountExtraPlacement(count, chance, extra)) { r ->
                if (r.nextFloat() <
                    rareChance
                ) {
                    rare
                } else {
                    default
                }
            }
        }

    /** A two-entry real `RANDOM_SELECTOR` behind one `RarityFilter`, such as `trees_meadow`. */
    public fun rarityChoice(
        rareConfig: TreeConfiguration,
        rareChance: Float,
        defaultConfig: TreeConfiguration,
        rarity: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val rare = tree(rareConfig)
            val default = tree(defaultConfig)
            place(chunk, random, RarityFilter(rarity)) { r -> if (r.nextFloat() < rareChance) rare else default }
        }

    /** A two-entry selector whose attempt count comes from an arbitrary vanilla [IntProvider]. */
    public fun providerCountChoice(
        rareConfig: TreeConfiguration,
        rareChance: Float,
        defaultConfig: TreeConfiguration,
        count: IntProvider,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val rare = tree(rareConfig)
            val default = tree(defaultConfig)
            place(chunk, random, CountPlacement(count)) { r -> if (r.nextFloat() < rareChance) rare else default }
        }

    /**
     * [entries] are rolled in order, each with its own `random.nextFloat()` draw, matching a real
     * `Feature.RANDOM_SELECTOR`: the first entry whose draw lands under its own chance stops the
     * roll there, `null` meaning an unported real entry landed here, consuming the draw and placing
     * nothing; [default] runs if no entry hits. [frequency] is a fixed
     * count, not `countExtra` (matches real `CountPlacement.of(n)`, e.g. `dark_forest_vegetation`).
     */
    public fun sequence(
        entries: List<Pair<Float, TreeConfiguration?>>,
        default: TreeConfiguration,
        frequency: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val choices = entries.map { (chance, config) -> chance to config?.let(::tree) }
            val fallback = tree(default)
            place(chunk, random, CountPlacement(frequency)) { r -> pickSequence(r, choices, fallback) }
        }

    /** Same as [sequence], but with a real `treePlacement(countExtra(...))` frequency instead of a fixed count (e.g. `trees_old_growth_pine_taiga`). */
    public fun sequenceCountExtra(
        entries: List<Pair<Float, TreeConfiguration?>>,
        default: TreeConfiguration,
        count: Int,
        chance: Float,
        extra: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val choices = entries.map { (entryChance, config) -> entryChance to config?.let(::tree) }
            val fallback = tree(default)
            place(chunk, random, CountExtraPlacement(count, chance, extra)) { r -> pickSequence(r, choices, fallback) }
        }

    public fun candidateSequence(
        entries: List<Pair<Float, Candidate?>>,
        default: Candidate,
        frequency: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            place(chunk, random, CountPlacement(frequency)) { r -> pickSequence(r, entries, default) }
        }

    public fun candidateSequenceCountExtra(
        entries: List<Pair<Float, Candidate?>>,
        default: Candidate,
        count: Int,
        chance: Float,
        extra: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            place(chunk, random, CountExtraPlacement(count, chance, extra)) { r -> pickSequence(r, entries, default) }
        }

    /** One vanilla `RANDOM_SELECTOR`: the entries it rolls in order, and what it falls back to. */
    public data class Selector(
        val entries: List<Pair<Float, Candidate?>>,
        val default: Candidate,
    )

    /**
     * The named tree selectors, by their vanilla placed feature id.
     *
     * These are the real `RANDOM_SELECTOR` contents, read from the jar's own configured features.
     * The biome table builds its decorations from them rather than repeating the lists, so a
     * correction lands in one place, and `/place feature` rolls exactly the same selector.
     */
    public val SELECTORS: Map<String, Selector> =
        mapOf(
            "trees_plains" to
                Selector(
                    listOf(
                        0.33333334f to tree(TreeConfigs.FANCY_OAK_BEES_005),
                        // Only this entry is `fallen_oak_tree`, which carries its own would_survive.
                        0.0125f to checkedFallen(FallenTreeConfigs.OAK, "oak_sapling"),
                    ),
                    tree(TreeConfigs.OAK_BEES_005),
                ),
            "trees_birch_and_oak_leaf_litter" to
                Selector(
                    listOf(
                        0.0025f to checkedFallen(FallenTreeConfigs.BIRCH, "birch_sapling"),
                        0.2f to checkedTree(TreeConfigs.BIRCH_BEES_0002_LEAF_LITTER, "birch_sapling"),
                        0.1f to checkedTree(TreeConfigs.FANCY_OAK_BEES_0002_LEAF_LITTER, "oak_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.OAK, "oak_sapling"),
                    ),
                    checkedTree(TreeConfigs.OAK_BEES_0002_LEAF_LITTER, "oak_sapling"),
                ),
            "trees_birch" to
                Selector(
                    listOf(0.0125f to checkedFallen(FallenTreeConfigs.BIRCH, "birch_sapling")),
                    checkedTree(TreeConfigs.BIRCH_BEES_0002, "birch_sapling"),
                ),
            "birch_tall" to
                Selector(
                    listOf(
                        0.00625f to checkedFallen(FallenTreeConfigs.SUPER_BIRCH, "birch_sapling"),
                        0.5f to checkedTree(TreeConfigs.SUPER_BIRCH_BEES_0002, "birch_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.BIRCH, "birch_sapling"),
                    ),
                    checkedTree(TreeConfigs.BIRCH_BEES_0002, "birch_sapling"),
                ),
            "trees_taiga" to
                Selector(
                    listOf(
                        0.33333334f to checkedTree(TreeConfigs.PINE, "spruce_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.SPRUCE, "spruce_sapling"),
                    ),
                    checkedTree(TreeConfigs.SPRUCE, "spruce_sapling"),
                ),
            "trees_snowy" to
                Selector(
                    listOf(0.0125f to checkedFallen(FallenTreeConfigs.SPRUCE, "spruce_sapling")),
                    checkedTree(TreeConfigs.SPRUCE, "spruce_sapling"),
                ),
            "trees_old_growth_spruce_taiga" to
                Selector(
                    listOf(
                        0.33333334f to checkedTree(TreeConfigs.MEGA_SPRUCE, "spruce_sapling"),
                        0.33333334f to checkedTree(TreeConfigs.PINE, "spruce_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.SPRUCE, "spruce_sapling"),
                    ),
                    checkedTree(TreeConfigs.SPRUCE, "spruce_sapling"),
                ),
            "trees_old_growth_pine_taiga" to
                Selector(
                    listOf(
                        0.025641026f to checkedTree(TreeConfigs.MEGA_SPRUCE, "spruce_sapling"),
                        0.30769232f to checkedTree(TreeConfigs.MEGA_PINE, "spruce_sapling"),
                        0.33333334f to checkedTree(TreeConfigs.PINE, "spruce_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.SPRUCE, "spruce_sapling"),
                    ),
                    checkedTree(TreeConfigs.SPRUCE, "spruce_sapling"),
                ),
            "trees_savanna" to
                Selector(
                    listOf(
                        0.8f to checkedTree(TreeConfigs.ACACIA, "acacia_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.OAK, "oak_sapling"),
                    ),
                    checkedTree(TreeConfigs.OAK, "oak_sapling"),
                ),
            "trees_windswept_savanna" to
                Selector(
                    listOf(
                        0.8f to checkedTree(TreeConfigs.ACACIA, "acacia_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.OAK, "oak_sapling"),
                    ),
                    checkedTree(TreeConfigs.OAK, "oak_sapling"),
                ),
            "trees_jungle" to
                Selector(
                    listOf(
                        0.1f to checkedTree(TreeConfigs.FANCY_OAK, "oak_sapling"),
                        0.5f to checkedTree(TreeConfigs.JUNGLE_BUSH, "oak_sapling"),
                        0.33333334f to checkedTree(TreeConfigs.MEGA_JUNGLE, "jungle_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.JUNGLE, "jungle_sapling"),
                    ),
                    checkedTree(TreeConfigs.JUNGLE_TREE, "jungle_sapling"),
                ),
            "trees_sparse_jungle" to
                Selector(
                    listOf(
                        0.1f to checkedTree(TreeConfigs.FANCY_OAK, "oak_sapling"),
                        0.5f to checkedTree(TreeConfigs.JUNGLE_BUSH, "oak_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.JUNGLE, "jungle_sapling"),
                    ),
                    checkedTree(TreeConfigs.JUNGLE_TREE, "jungle_sapling"),
                ),
            "trees_windswept_hills" to windsweptSelector(),
            "trees_windswept_forest" to windsweptSelector(),
            "trees_grove" to Selector(listOf(0.33333334f to onSnow(TreeConfigs.PINE)), onSnow(TreeConfigs.SPRUCE)),
            "trees_badlands" to
                Selector(
                    listOf(0.0125f to checkedFallen(FallenTreeConfigs.OAK, "oak_sapling")),
                    checkedTree(TreeConfigs.OAK_LEAF_LITTER, "oak_sapling"),
                ),
            "trees_meadow" to
                Selector(
                    listOf(0.5f to checkedTree(TreeConfigs.FANCY_OAK_BEES, "oak_sapling")),
                    checkedTree(TreeConfigs.SUPER_BIRCH_BEES, "birch_sapling"),
                ),
            "trees_water" to
                Selector(
                    listOf(0.1f to checkedTree(TreeConfigs.FANCY_OAK, "oak_sapling")),
                    checkedTree(TreeConfigs.OAK, "oak_sapling"),
                ),
            "trees_flower_forest" to
                Selector(
                    listOf(
                        0.0025f to checkedFallen(FallenTreeConfigs.BIRCH, "birch_sapling"),
                        0.2f to checkedTree(TreeConfigs.BIRCH_BEES_002, "birch_sapling"),
                        0.1f to checkedTree(TreeConfigs.FANCY_OAK_BEES_002, "oak_sapling"),
                    ),
                    checkedTree(TreeConfigs.OAK_BEES_002, "oak_sapling"),
                ),
            "trees_mangrove" to
                Selector(
                    listOf(0.85f to checkedTree(TreeConfigs.TALL_MANGROVE, "mangrove_propagule")),
                    checkedTree(TreeConfigs.MANGROVE, "mangrove_propagule"),
                ),
            "trees_cherry" to Selector(emptyList(), tree(TreeConfigs.CHERRY_BEES_005)),
            "trees_swamp" to Selector(emptyList(), tree(TreeConfigs.SWAMP_OAK)),
            "pale_garden_vegetation" to
                Selector(
                    listOf(
                        0.1f to checkedTree(TreeConfigs.PALE_OAK_CREAKING, "pale_oak_sapling"),
                        0.9f to checkedTree(TreeConfigs.PALE_OAK, "pale_oak_sapling"),
                    ),
                    checkedTree(TreeConfigs.PALE_OAK, "pale_oak_sapling"),
                ),
            "dark_forest_vegetation" to
                Selector(
                    listOf(
                        0.025f to hugeMushroom(HugeMushroomFeature.Kind.BROWN),
                        0.05f to hugeMushroom(HugeMushroomFeature.Kind.RED),
                        0.6666667f to checkedTree(TreeConfigs.DARK_OAK_LEAF_LITTER, "dark_oak_sapling"),
                        0.0025f to checkedFallen(FallenTreeConfigs.BIRCH, "birch_sapling"),
                        0.2f to checkedTree(TreeConfigs.BIRCH_LEAF_LITTER, "birch_sapling"),
                        0.0125f to checkedFallen(FallenTreeConfigs.OAK, "oak_sapling"),
                        0.1f to checkedTree(TreeConfigs.FANCY_OAK_LEAF_LITTER, "oak_sapling"),
                    ),
                    checkedTree(TreeConfigs.OAK_LEAF_LITTER, "oak_sapling"),
                ),
        )

    /** Both windswept selectors are the same roll; only their counts differ, and the count is placement. */
    private fun windsweptSelector(): Selector =
        Selector(
            listOf(
                0.008325f to checkedFallen(FallenTreeConfigs.SPRUCE, "spruce_sapling"),
                0.666f to checkedTree(TreeConfigs.SPRUCE, "spruce_sapling"),
                0.1f to checkedTree(TreeConfigs.FANCY_OAK, "oak_sapling"),
                0.0125f to checkedFallen(FallenTreeConfigs.OAK, "oak_sapling"),
            ),
            checkedTree(TreeConfigs.OAK, "oak_sapling"),
        )

    /** One roll of [placedFeature]'s selector, placed at [origin]: what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        placedFeature: String,
        mossyCarpetRandom: BasaltRandom = random,
    ): Boolean {
        val selector = SELECTORS[placedFeature] ?: return false
        pickSequence(random, selector.entries, selector.default)?.place(chunk, random, origin, mossyCarpetRandom)
        return true
    }

    /** The named selector [placedFeature] refers to, for the biome table to build its own placement on. */
    public fun selector(placedFeature: String): Selector =
        SELECTORS[placedFeature] ?: error("unknown tree selector '$placedFeature'")

    /** [candidateSequenceCountExtra] over a named selector, which is how the biome table asks for one. */
    public fun countExtra(
        selector: Selector,
        count: Int,
        chance: Float,
        extra: Int,
    ): BiomeVegetalDecoration = candidateSequenceCountExtra(selector.entries, selector.default, count, chance, extra)

    /** [candidateSequence] over a named selector. */
    public fun sequenceOf(
        selector: Selector,
        frequency: Int,
    ): BiomeVegetalDecoration = candidateSequence(selector.entries, selector.default, frequency)

    private fun pickSequence(
        random: WorldgenRandom,
        entries: List<Pair<Float, Candidate?>>,
        default: Candidate,
    ): Candidate? {
        for ((chance, candidate) in entries) if (random.nextFloat() < chance) return candidate
        return default
    }

    private val GROVE_SUPPORT_PATHS = setOf("snow_block", "powder_snow")
    private const val GROVE_SCAN_STEPS = 8
}
