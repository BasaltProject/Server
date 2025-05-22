package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/**
 * Real Desert `vegetal_decoration`: `patch_cactus_desert` and `patch_dead_bush_2`, ported from the
 * real jar's own `VegetationPlacements`. **Not ported**: `patch_dry_grass_desert` (a
 * `WeightedStateProvider` between two dry-grass variants, not modeled yet),
 * `patch_sugar_cane_desert`/`patch_pumpkin` (need water-adjacency/specific-noise checks this
 * project doesn't model), matching [PlainsDecoration]'s own precedent for documenting gaps rather
 * than approximating them.
 */
public object DesertDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        placeCactus(chunk, random)
        placeDeadBush(chunk, random)
    }

    // parity: placed_feature/patch_cactus_desert.json.
    private fun placeCactus(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        runPipeline(
            chunk,
            random,
            listOf(
                RarityFilter(6),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(10),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
        ) { c, r, pos -> CactusFeature.place(c, r, pos) }
    }

    // parity: placed_feature/patch_dead_bush_2.json (VegetationPlacements.worldSurfaceSquaredWithCount(2) + copyAndAdd).
    private fun placeDeadBush(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> deadBushStateId })
        runPipeline(
            chunk,
            random,
            listOf(
                CountPlacement(2),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(4),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
        ) { c, r, pos -> feature.place(c, r, pos) }
    }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        place: (Chunk, WorldgenRandom, DecorationPos) -> Boolean,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> place(chunk, random, pos) }
    }

    private val deadBushStateId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dead_bush"))?.stateId ?: error("unknown block 'dead_bush'")
}
