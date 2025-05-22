package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/**
 * Real Badlands `vegetal_decoration`: `patch_dead_bush_badlands`, ported from the real jar's own
 * `VegetationPlacements`. **Not ported**: `patch_grass_badlands`/`patch_dry_grass_badlands` (need a
 * `WeightedStateProvider` this project doesn't model yet), matching this project's own established
 * precedent of documenting gaps rather than approximating them.
 */
public object BadlandsDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> deadBushStateId })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        // parity: placed_feature/patch_dead_bush_badlands.json (worldSurfaceSquaredWithCount(20) + copyAndAdd).
        for (modifier in
            listOf(
                CountPlacement(20),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(4),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private val deadBushStateId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dead_bush"))?.stateId ?: error("unknown block 'dead_bush'")
}
