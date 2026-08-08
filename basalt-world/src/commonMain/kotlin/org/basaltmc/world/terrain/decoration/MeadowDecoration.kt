package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.noise.NormalNoise
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs

/** Real meadow vegetation, including `flower_meadow`'s DualNoiseProvider and rare trees. */
public object MeadowDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        PlainsDecoration.TALL_GRASS.decorate(chunk, random)
        GRASS.decorate(chunk, random)
        FLOWERS.decorate(chunk, random)
        TREES.decorate(chunk, random)
        ForestFloorDecoration.MEADOW_WILDFLOWERS.decorate(chunk, random)
    }

    /** // parity: `trees_meadow`, rarity 1/100 and a 50/50 fancy-oak/super-birch selector. */
    public val TREES: BiomeVegetalDecoration =
        TreeDecoration.rarityChoice(TreeConfigs.FANCY_OAK, 0.5f, TreeConfigs.SUPER_BIRCH, 100)

    public val GRASS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature = SimpleBlockFeature({ _, _ -> shortGrassStateId })
            runPipeline(
                chunk,
                random,
                listOf(
                    NoiseThresholdCountPlacement(-0.8, belowNoise = 5, aboveNoise = 10),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(16),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                ),
            ) { position -> feature.place(chunk, random, position) }
        }

    public val FLOWERS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            runPipeline(
                chunk,
                random,
                listOf(
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(96),
                    RandomOffsetPlacement(xzSpread = 6, ySpread = 2),
                    AirTargetFilter(),
                ),
            ) { position ->
                val stateId = flowerStateAt(position)
                SimpleBlockFeature({ _, _ -> stateId }, isDoublePlant = stateId == tallGrassStateId)
                    .place(chunk, random, position)
            }
        }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        place: (DecorationPos) -> Unit,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
        positions.forEach(place)
    }

    private fun flowerStateAt(position: DecorationPos): Int {
        val slowValue = slowNoise(position.x, position.y, position.z)
        val localVariety = clampedMap(slowValue, 1.0, 4.0).toInt()
        val possible =
            List(localVariety) { index ->
                val value = slowNoise(position.x + index * 54545, position.y, position.z + index * 34234)
                stateFromNoise(flowerStateIds, value)
            }
        return stateFromNoise(possible, fastNoise.getValue(position.x.toDouble(), position.y.toDouble(), position.z.toDouble()))
    }

    private fun slowNoise(
        x: Int,
        y: Int,
        z: Int,
    ): Double = slowNoise.getValue(x.toDouble(), y.toDouble(), z.toDouble())

    private fun stateFromNoise(
        states: List<Int>,
        noise: Double,
    ): Int {
        val placement = ((1.0 + noise) / 2.0).coerceIn(0.0, 0.9999)
        return states[(placement * states.size).toInt()]
    }

    private fun clampedMap(
        value: Double,
        outputMin: Double,
        outputMax: Double,
    ): Double = outputMin + ((value + 1.0) / 2.0).coerceIn(0.0, 1.0) * (outputMax - outputMin)

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val shortGrassStateId = stateId("short_grass")
    private val tallGrassStateId = stateId("tall_grass")
    private val flowerStateIds =
        listOf("tall_grass", "allium", "poppy", "azure_bluet", "dandelion", "cornflower", "oxeye_daisy", "short_grass")
            .map(::stateId)
    private val fastNoise = NormalNoise.create(LegacyRandom(2345L), -3, doubleArrayOf(1.0))
    private val slowNoise = NormalNoise.create(LegacyRandom(2345L), -10, doubleArrayOf(1.0))
}
