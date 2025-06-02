package org.basaltmc.world.terrain.decoration.nether

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.terrain.NetherBiomeSource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class NetherVegetalDecorationTest {
    @Test
    fun normalMushroomUsesRarityHeightmapAndBiomeFilter() {
        val wastes = floorChunk()
        val seed =
            (0L..2048L).first { candidate ->
                NetherVegetalDecoration.decorateFeature(wastes, candidate, source(0.0, 0.0), 1)
                count(wastes, "brown_mushroom") > 0
            }
        val basaltDeltas = floorChunk()

        NetherVegetalDecoration.decorateFeature(basaltDeltas, seed, source(-0.5, 0.0), 1)

        assertTrue(count(wastes, "brown_mushroom") > 0)
        assertEquals(0, count(basaltDeltas, "brown_mushroom"))
    }

    @Test
    fun overworldSpringConfigurationDoesNotReplaceNetherRock() {
        val chunk = solidChunk()

        NetherVegetalDecoration.decorateFeature(chunk, 1234L, source(0.0, 0.0), 0)

        assertEquals(0, count(chunk, "lava"))
    }

    @Test
    fun forestVegetationUsesNyliumAndWeightedProviders() {
        val warped = floorChunk("warped_nylium")
        val crimson = floorChunk("crimson_nylium")

        NetherVegetalDecoration.decorateFeature(warped, 1234L, source(0.0, 0.5), 4)
        NetherVegetalDecoration.decorateFeature(warped, 1234L, source(0.0, 0.5), 5)
        NetherVegetalDecoration.decorateFeature(crimson, 1234L, source(0.4, 0.0), 9)

        assertTrue(count(warped, "warped_roots") > 0)
        assertTrue(count(warped, "nether_sprouts") > 0)
        assertTrue(count(crimson, "crimson_roots") > 0)
    }

    @Test
    fun hugeFungiUseMatchingNyliumAndBiome() {
        val (crimsonSeed, crimson) =
            generatedChunk(
                factory = { floorChunk("crimson_nylium") },
                biomeSource = source(0.4, 0.0),
                featureIndex = 3,
                expectedBlock = "crimson_stem",
            )
        val (warpedSeed, warped) =
            generatedChunk(
                factory = { floorChunk("warped_nylium") },
                biomeSource = source(0.0, 0.5),
                featureIndex = 8,
                expectedBlock = "warped_stem",
            )
        val filtered = floorChunk("crimson_nylium")
        NetherVegetalDecoration.decorateFeature(filtered, crimsonSeed, source(0.0, 0.5), 3)

        assertTrue(crimsonSeed >= 0)
        assertTrue(warpedSeed >= 0)
        assertTrue(count(crimson, "nether_wart_block") > 0)
        assertTrue(count(warped, "warped_wart_block") > 0)
        assertEquals(0, count(filtered, "crimson_stem"))
    }

    @Test
    fun twistingAndWeepingVinesPlaceHeadAndPlantStates() {
        val (twistingSeed, twisting) =
            generatedChunk(
                factory = { floorChunk("warped_nylium") },
                biomeSource = source(0.0, 0.5),
                featureIndex = 6,
                expectedBlock = "twisting_vines",
            )
        val (weepingSeed, weeping) =
            generatedChunk(
                factory = ::ceilingChunk,
                biomeSource = source(0.4, 0.0),
                featureIndex = 7,
                expectedBlock = "weeping_vines",
            )

        assertTrue(twistingSeed >= 0)
        assertTrue(weepingSeed >= 0)
        assertTrue(count(twisting, "twisting_vines_plant") > 0)
        assertTrue(count(weeping, "weeping_vines_plant") > 0)
        assertTrue(hasAgedState(twisting, "twisting_vines"))
        assertTrue(hasAgedState(weeping, "weeping_vines"))
    }

    private fun generatedChunk(
        factory: () -> Chunk,
        biomeSource: NetherBiomeSource,
        featureIndex: Int,
        expectedBlock: String,
    ): Pair<Long, Chunk> {
        for (seed in 0L..2048L) {
            val chunk = factory()
            NetherVegetalDecoration.decorateFeature(chunk, seed, biomeSource, featureIndex)
            if (countBlock(chunk, expectedBlock) > 0) return seed to chunk
        }
        error("no seed generated $expectedBlock")
    }

    private fun floorChunk(blockPath: String = "netherrack"): Chunk =
        Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            val stateId = stateId(blockPath)
            for (y in 0..40) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, stateId)
                }
            }
        }

    private fun ceilingChunk(): Chunk =
        Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) chunk.setBlockStateId(x, 80, z, Blocks.NETHERRACK.stateId)
            }
        }

    private fun solidChunk(): Chunk =
        Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (y in 0..127) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, Blocks.NETHERRACK.stateId)
                }
            }
        }

    private fun source(
        temperature: Double,
        vegetation: Double,
    ): NetherBiomeSource = NetherBiomeSource(ConstantDensity(temperature), ConstantDensity(vegetation))

    private fun count(
        chunk: Chunk,
        blockPath: String,
    ): Int {
        val stateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(blockPath))?.stateId
                ?: error("missing vanilla block minecraft:$blockPath")
        var count = 0
        for (y in 0..127) {
            for (z in 0 until 16) {
                for (x in 0 until 16) if (chunk.getBlockStateId(x, y, z) == stateId) count++
            }
        }
        return count
    }

    private fun hasAgedState(
        chunk: Chunk,
        blockPath: String,
    ): Boolean {
        for (y in 0..127) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val state = Blocks.byStateId(chunk.getBlockStateId(x, y, z)) ?: continue
                    if (state.block.path == blockPath && state.properties["age"] != null) return true
                }
            }
        }
        return false
    }

    private fun countBlock(
        chunk: Chunk,
        blockPath: String,
    ): Int {
        var count = 0
        for (y in 0..127) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    if (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path == blockPath) count++
                }
            }
        }
        return count
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
