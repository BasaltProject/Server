package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.noise.NormalNoise
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.world.terrain.decoration.tree.FallenTreeConfigs
import kotlin.math.max
import kotlin.math.min

/**
 * Real Flower Forest decoration, ported from the real jar's own decompiled
 * `net.minecraft.world.level.levelgen.feature.stateproviders.NoiseProvider`/`NoiseBasedStateProvider`
 * (`flower_flower_forest`'s real 11-flower `NoiseProvider`, seed `2345L`, `NoiseParameters(0, [1.0])`,
 * scale `0.020833334`) plus its real trees (`trees_flower_forest`) and default grass.
 *
 * The preceding `flower_forest_flowers` selector is composed by [BiomeDecoration].
 */
public object FlowerForestDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        TREES.decorate(chunk, random)
        placeFlowers(chunk, random)
        DefaultVegetationDecoration.GRASS.decorate(chunk, random)
    }

    // parity: VegetationFeatures.TREES_FLOWER_FOREST, VegetationPlacements.treePlacement(countExtra(6, 0.1, 1)).
    private val TREES: BiomeVegetalDecoration =
        TreeDecoration.candidateSequenceCountExtra(
            listOf(
                0.0025f to TreeDecoration.fallen(FallenTreeConfigs.BIRCH),
                0.2f to TreeDecoration.tree(TreeConfigs.BIRCH_BEES_002),
                0.1f to TreeDecoration.tree(TreeConfigs.FANCY_OAK_BEES_002),
            ),
            TreeDecoration.tree(TreeConfigs.OAK_BEES_002),
            6,
            0.1f,
            1,
        )

    // parity: placed_feature/flower_flower_forest.json + the real NoiseProvider algorithm.
    private fun placeFlowers(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, pos -> flowerAt(pos.x, pos.y, pos.z) })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in
            listOf(
                CountPlacement(3),
                RarityFilter(2),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(96),
                RandomOffsetPlacement(xzSpread = 6, ySpread = 2),
                AirTargetFilter(),
            )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private fun flowerAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val noiseValue = FLOWER_NOISE.getValue(x * SCALE, y * SCALE, z * SCALE)
        val placementValue = max(0.0, min((1.0 + noiseValue) / 2.0, 0.9999))
        val index = (placementValue * flowerStateIds.size).toInt()
        return flowerStateIds[index]
    }

    private const val SCALE = 0.020833334

    // parity: NoiseBasedStateProvider(seed=2345L, NoiseParameters(0, [1.0]), 0.020833334F).
    private val FLOWER_NOISE = NormalNoise.create(LegacyRandom(2345L), 0, doubleArrayOf(1.0))

    private fun state(name: String): Int = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

    // parity: real order matters (index selects into this list): dandelion, poppy, allium, azure_bluet,
    // red_tulip, orange_tulip, white_tulip, pink_tulip, oxeye_daisy, cornflower, lily_of_the_valley.
    private val flowerStateIds =
        listOf(
            "dandelion", "poppy", "allium", "azure_bluet", "red_tulip", "orange_tulip",
            "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley",
        ).map { state(it) }
}
