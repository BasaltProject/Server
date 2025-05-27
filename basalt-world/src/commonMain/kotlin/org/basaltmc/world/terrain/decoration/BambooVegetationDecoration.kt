package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.world.terrain.decoration.tree.TreeFeature

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
            when {
                random.nextFloat() < 0.05f -> placeTree(chunk, random, position, TreeConfigs.FANCY_OAK)
                random.nextFloat() < 0.15f -> placeTree(chunk, random, position, TreeConfigs.JUNGLE_BUSH)
                random.nextFloat() < 0.7f -> placeTree(chunk, random, position, TreeConfigs.MEGA_JUNGLE)
                else -> JungleGrassDecoration.placePatchAt(chunk, random, position, context)
            }
        }
    }

    private fun placeTree(
        chunk: Chunk,
        random: WorldgenRandom,
        position: DecorationPos,
        config: org.basaltmc.world.terrain.decoration.tree.TreeConfiguration,
    ) {
        TreeFeature.place(chunk, random, position.x, position.y, position.z, config)
    }
}
