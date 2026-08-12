package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeConfiguration
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeFeature
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfiguration
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature

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
        )
    }

    public fun tree(config: TreeConfiguration): Candidate =
        Candidate {
            chunk,
            random,
            position,
            ->
            TreeFeature.place(chunk, random, position.x, position.y, position.z, config)
        }

    public fun fallen(config: FallenTreeConfiguration): Candidate =
        Candidate {
            chunk,
            random,
            position,
            ->
            FallenTreeFeature.place(chunk, random, position.x, position.y, position.z, config)
        }

    public fun hugeMushroom(kind: HugeMushroomFeature.Kind): Candidate =
        Candidate {
            chunk,
            random,
            position,
            ->
            HugeMushroomFeature.place(chunk, random, position.x, position.y, position.z, kind)
        }

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
        positions.forEach { pos -> pick(random)?.place(chunk, random, pos) }
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

    private fun pickSequence(
        random: WorldgenRandom,
        entries: List<Pair<Float, Candidate?>>,
        default: Candidate,
    ): Candidate? {
        for ((chance, candidate) in entries) if (random.nextFloat() < chance) return candidate
        return default
    }
}
