package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature

/** // parity: bamboo jungle's `bamboo_vegetation` random selector. */
public object BambooVegetationDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (
        modifier in
        listOf(
            CountPlacement(IntProvider.Weighted(30 to 9, 31 to 1)),
            InSquarePlacement,
            SurfaceWaterDepthFilter(0),
            HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR),
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { position ->
            placeAt(chunk, random, position, context)
        }
    }

    internal fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        position: DecorationPos,
        context: PlacementContext,
    ) {
        when {
            random.nextFloat() < 0.05f ->
                placeCheckedTree(chunk, random, position, TreeConfigs.FANCY_OAK, "oak_sapling")
            random.nextFloat() < 0.15f ->
                placeCheckedTree(chunk, random, position, TreeConfigs.JUNGLE_BUSH, "oak_sapling")
            random.nextFloat() < 0.7f ->
                placeCheckedTree(chunk, random, position, TreeConfigs.MEGA_JUNGLE, "jungle_sapling")
            else -> JungleGrassDecoration.placePatchAt(chunk, random, position, context)
        }
    }

    private fun placeCheckedTree(
        chunk: Chunk,
        random: WorldgenRandom,
        position: DecorationPos,
        config: org.basaltmc.server.world.terrain.decoration.tree.TreeConfiguration,
        sapling: String,
    ) {
        if (TreeDecoration.survivesPlacementPredicate(chunk, position, sapling)) {
            placeTree(chunk, random, position, config)
        }
    }

    private fun placeTree(
        chunk: Chunk,
        random: WorldgenRandom,
        position: DecorationPos,
        config: org.basaltmc.server.world.terrain.decoration.tree.TreeConfiguration,
    ) {
        TreeFeature.place(chunk, random, position.x, position.y, position.z, config)
    }
}
