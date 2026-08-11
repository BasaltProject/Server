package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/** // parity: BambooFeature and the bamboo/bamboo_light placed features. */
public object BambooDecoration {
    public val LIGHT: BiomeVegetalDecoration =
        feature(
            podzolProbability = 0.0f,
            modifiers = listOf(RarityFilter(4), InSquarePlacement, HeightmapPlacement),
        )

    public val DENSE: BiomeVegetalDecoration =
        feature(
            podzolProbability = 0.2f,
            modifiers =
                listOf(
                    NoiseBasedCountPlacement(noiseToCountRatio = 160, noiseFactor = 80.0, noiseOffset = 0.3),
                    InSquarePlacement,
                    HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
                ),
        )

    private fun feature(
        podzolProbability: Float,
        modifiers: List<PlacementModifier>,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val context = PlacementContext(chunk)
            var positions: Sequence<DecorationPos> =
                sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
            for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
            positions.forEach { origin -> place(chunk, random, origin, podzolProbability) }
        }

    private fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        podzolProbability: Float,
    ) {
        if (!origin.inWorld() || !Blocks.isAir(chunk.blockAt(origin)) || !canBambooSurvive(chunk, origin)) return

        val height = random.nextInt(12) + 5
        if (random.nextFloat() < podzolProbability) {
            val radius = random.nextInt(4) + 1
            for (x in origin.x - radius..origin.x + radius) {
                for (z in origin.z - radius..origin.z + radius) {
                    val dx = x - origin.x
                    val dz = z - origin.z
                    if (dx * dx + dz * dz <= radius * radius) {
                        val surfaceY = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.WORLD_SURFACE) - 1
                        val surface = DecorationPos(x, surfaceY, z)
                        if (surface.inWorld() && chunk.blockAt(surface) in PODZOL_REPLACEABLE) {
                            chunk.setBlock(surface, Blocks.PODZOL.stateId)
                        }
                    }
                }
            }
        }

        var pos = origin
        var placed = 0
        while (placed < height && pos.inWorld() && Blocks.isAir(chunk.blockAt(pos))) {
            chunk.setBlock(pos, Blocks.BAMBOO.stateId)
            pos = pos.up()
            placed++
        }
        if (placed >= 3 && pos.inWorld()) {
            chunk.setBlock(pos, Blocks.BAMBOO.stateId)
            chunk.setBlock(pos.down(), Blocks.BAMBOO.stateId)
            chunk.setBlock(pos.down(2), Blocks.BAMBOO.stateId)
        }
    }

    private fun canBambooSurvive(
        chunk: Chunk,
        origin: DecorationPos,
    ): Boolean = origin.y > WorldHeight.MIN_Y && chunk.blockAt(origin.down()) in SUPPORTS_BAMBOO

    private fun DecorationPos.inWorld(): Boolean = y in WorldHeight.MIN_Y..WorldHeight.MAX_Y

    private fun DecorationPos.up(): DecorationPos = DecorationPos(x, y + 1, z)

    private fun DecorationPos.down(distance: Int = 1): DecorationPos = DecorationPos(x, y - distance, z)

    private fun Chunk.blockAt(pos: DecorationPos): Int = getBlockStateId(pos.x and 15, pos.y, pos.z and 15)

    private fun Chunk.setBlock(
        pos: DecorationPos,
        stateId: Int,
    ) {
        setBlockStateId(pos.x and 15, pos.y, pos.z and 15, stateId)
    }

    private val PODZOL_REPLACEABLE =
        states(
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
        )

    private val SUPPORTS_BAMBOO =
        PODZOL_REPLACEABLE +
            states(
                "sand",
                "red_sand",
                "suspicious_sand",
                "bamboo",
                "bamboo_sapling",
                "gravel",
                "suspicious_gravel",
            )

    private fun states(vararg names: String): Set<Int> =
        names
            .map { name ->
                Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
            }.toSet()
}
