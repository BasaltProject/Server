package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier

/** Jungle `melon` simple-block feature with normal and sparse placed-feature rarity. */
public object JungleMelonDecoration {
    public val NORMAL: BiomeVegetalDecoration = placed(rarity = 6)
    public val SPARSE: BiomeVegetalDecoration = placed(rarity = 64)

    private fun placed(rarity: Int): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val context = PlacementContext(chunk)
            var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
            for (
            modifier in
            listOf(
                RarityFilter(rarity),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(64),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
            )
            ) {
                positions = positions.flatMap { modifier.getPositions(context, random, it) }
            }
            positions.forEach { position -> place(chunk, position) }
        }

    private fun place(
        chunk: Chunk,
        position: DecorationPos,
    ) {
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || position.y == WorldHeight.MIN_Y) return
        val x = position.x and 15
        val z = position.z and 15
        val current = chunk.getBlockStateId(x, position.y, z)
        if (current !in replaceableStateIds || Blocks.hasWaterFluid(current)) return
        if (chunk.getBlockStateId(x, position.y - 1, z) != Blocks.GRASS_BLOCK.stateId) return
        chunk.setBlockStateId(x, position.y, z, melonStateId)
    }

    private fun states(vararg paths: String): Set<Int> =
        paths.mapNotNull { path -> Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId }.toSet()

    private val replaceableStateIds =
        states(
            "air",
            "short_grass",
            "tall_grass",
            "bush",
            "fern",
            "large_fern",
            "dandelion",
            "poppy",
        )
    private val melonStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("melon"))?.stateId ?: error("unknown block 'melon'")
}
