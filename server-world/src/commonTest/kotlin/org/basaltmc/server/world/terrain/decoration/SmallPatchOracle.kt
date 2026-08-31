package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the small vegetation patches. */
class SmallPatchOracle {
    private inner class FakeEnvironment(private val biome: String) : SurfaceRuleEnvironment {
        override fun surfaceDepthNoise(x: Int, z: Int): Int = 0
        override fun surfaceSecondaryNoise(x: Int, z: Int): Double = 0.0
        override fun sample2d(noiseId: String, x: Int, z: Int): Double = 0.0
        override fun sample3d(noiseId: String, x: Int, y: Int, z: Int): Double = 0.0
        override fun verticalGradientRandom(name: String, x: Int, y: Int, z: Int): Float = 0.5f
        override fun positionalRandom(x: Int, y: Int, z: Int): BasaltRandom = XoroshiroRandom(x * 31L + z)
        override fun estimatedSurfaceHeight(x: Int, z: Int): Int = 70
        override fun biomeAt(x: Int, y: Int, z: Int): String = biome
        override fun noiseBiomeAt(quartX: Int, quartY: Int, quartZ: Int, cache: DensityCache?): String = biome
        override fun isSteep(x: Int, z: Int): Boolean = false
        override fun clayBand(x: Int, y: Int, z: Int): Int = 0
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun groundChunk(chunkX: Int, chunkZ: Int, surface: String, pond: Boolean, wall: Boolean = false): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId(surface))
            }
        }
        // A vine needs a vertical face to hang from, which flat ground never gives it.
        if (wall) {
            for (y in 71..100) {
                for (x in 7..8) chunk.setBlockStateId(x, y, 8, Blocks.STONE.stateId)
            }
        }
        if (pond) {
            for (x in 3..12) {
                for (z in 3..12) chunk.setBlockStateId(x, 70, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 60..90) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path != null && path !in setOf("stone", "air", "water", surfacePaths)) placed++
                }
            }
        }
        return checksum to placed
    }

    private val surfacePaths = "grass_block"

    private fun report(
        name: String,
        surface: String = "grass_block",
        pond: Boolean = false,
        wall: Boolean = false,
        count: Int = 30,
        body: (Chunk, WorldgenRandom, DecorationPos, FakeEnvironment, Long) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        var ticks = 0
        var trailing = 0
        for (index in 0 until count) {
            val chunk = groundChunk(index * 5 - 30, index * -3 + 12, surface, pond, wall)
            val random = WorldgenRandom(LegacyRandom(1234L + index * 3719L))
            val origin = DecorationPos(chunk.chunkX * 16 + 8, 71, chunk.chunkZ * 16 + 8)
            body(chunk, random, origin, FakeEnvironment("minecraft:swamp"), 5150L + index)
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
            ticks += chunk.scheduledTickCount
            trailing = random.nextInt()
        }
        println("ORACLE $name placed=$placed deferred=$deferred ticks=$ticks checksum=$checksum after=$trailing")
    }

    @Test
    fun dump() {
        report("flowerDefault") { c, r, o, _, _ ->
            DefaultVegetationDecoration.placeDefaultPatchAt(c, r, o)
        }
        report("dryGrassAt", surface = "sand") { c, r, o, _, _ -> DryGrassDecoration.placeAt(c, r, o) }
        report("dryGrassDesert", surface = "sand") { c, r, _, _, _ -> DryGrassDecoration.DESERT.decorate(c, r) }
        report("dryGrassBadlands", surface = "red_sand") { c, r, _, _, _ ->
            DryGrassDecoration.BADLANDS.decorate(c, r)
        }
        report("forestFlowersAt") { c, r, o, _, _ -> ForestFlowerDecoration.placeAt(c, r, o) }
        report("forestFlowersNormal") { c, r, _, _, _ -> ForestFlowerDecoration.NORMAL.decorate(c, r) }
        report("forestFlowersFlowerForest") { c, r, _, _, _ ->
            ForestFlowerDecoration.FLOWER_FOREST.decorate(c, r)
        }
        report("jungleVines", wall = true) { c, r, _, _, _ -> JungleVineDecoration.decorate(c, r) }
        report("jungleVineAt", wall = true) { c, _, _, _, _ ->
            for (y in 72..90) JungleVineDecoration.placeAt(c, DecorationPos(c.chunkX * 16 + 9, y, c.chunkZ * 16 + 8))
        }
        report("waterlilyAt", pond = true) { c, r, o, _, _ -> WaterlilyPatchDecoration.placeAt(c, r, o) }
        report("waterlilyStep", pond = true) { c, _, _, e, seed ->
            WaterlilyPatchDecoration.decorate(c, seed, e)
        }
        report("pumpkinAt") { c, r, o, _, _ -> PumpkinPatchDecoration.placeAt(c, r, o) }
        // Pumpkin's own step is 1/300, so it needs far more chunks than the rest to fire at all.
        report("pumpkinStep", count = 900) { c, _, _, e, seed -> PumpkinPatchDecoration.decorate(c, seed, e) }
        // Stone carries no vegetation, so every candidate is refused.
        report("bareStone", surface = "stone") { c, r, o, _, _ ->
            DefaultVegetationDecoration.placeDefaultPatchAt(c, r, o)
        }
    }
}
