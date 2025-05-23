package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.decoration.tree.TreeConfiguration
import org.basaltmc.world.terrain.decoration.tree.TreeFeature

/**
 * Real per-biome tree placement, ported from the real jar's own decompiled
 * `net.minecraft.data.worldgen.placement.VegetationPlacements`/`BiomeDefaultFeatures` (the
 * `count`/`chance`/`extra` triples and, where a biome's real tree list is a weighted choice
 * between species, the real chance for the non-default species).
 *
 * **Documented simplification**: real vanilla's per-biome tree list is a
 * `Feature.RANDOM_SELECTOR` over several weighted entries (commonly a rare "fallen tree" log pile
 * and/or a "fancy oak"/"mega" variant alongside the common species), each independently rolled with
 * its own `random.nextFloat()` draw before falling through to the next. Only entries this project's
 * [TreeConfigs] can build are ported; unported entries (fallen trees, fancy oak, mega spruce/pine,
 * jungle bush) are dropped from the roll entirely rather than approximated, so the exact sequence of
 * random draws (and everything decorated after it in the same chunk) diverges from real vanilla
 * from a biome's very first tree onward. This is a real, deliberate parity gap, not an oversight:
 * closing it needs every dropped configured feature ported first.
 */
public object TreeDecoration {
    private fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        frequency: PlacementModifier,
        pick: (WorldgenRandom) -> TreeConfiguration?,
    ) {
        val context = PlacementContext(chunk)
        val origin = DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16)
        var positions = frequency.getPositions(context, random, origin)
        positions = positions.flatMap { InSquarePlacement.getPositions(context, random, it) }
        positions =
            positions.flatMap {
                HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR).getPositions(context, random, it)
            }
        positions.forEach { pos -> pick(random)?.let { TreeFeature.place(chunk, random, pos.x, pos.y, pos.z, it) } }
    }

    public fun single(
        config: TreeConfiguration,
        count: Int,
        chance: Float,
        extra: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random -> place(chunk, random, CountExtraPlacement(count, chance, extra)) { config } }

    /** Matches a real plain `CountPlacement.of(count)` frequency (no weighted "extra" roll), e.g. `pale_garden_vegetation`. */
    public fun fixed(
        config: TreeConfiguration,
        count: Int,
    ): BiomeVegetalDecoration = BiomeVegetalDecoration { chunk, random -> place(chunk, random, CountPlacement(count)) { config } }

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
            place(chunk, random, CountExtraPlacement(count, chance, extra)) { r -> if (r.nextFloat() < rareChance) rareConfig else defaultConfig }
        }

    /**
     * [entries] are rolled in order, each with its own `random.nextFloat()` draw, matching a real
     * `Feature.RANDOM_SELECTOR`: the first entry whose draw lands under its own chance stops the
     * roll there, `null` meaning "an unported real entry landed here, consume the draw and place
     * nothing" (see this file's own KDoc); [default] runs if no entry hits. [frequency] is a fixed
     * count, not `countExtra` (matches real `CountPlacement.of(n)`, e.g. `dark_forest_vegetation`).
     */
    public fun sequence(
        entries: List<Pair<Float, TreeConfiguration?>>,
        default: TreeConfiguration,
        frequency: Int,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            place(chunk, random, CountPlacement(frequency)) { r ->
                entries.firstOrNull { (chance, _) -> r.nextFloat() < chance }?.second ?: default
            }
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
            place(chunk, random, CountExtraPlacement(count, chance, extra)) { r ->
                entries.firstOrNull { (c, _) -> r.nextFloat() < c }?.second ?: default
            }
        }
}
