package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isVineState
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the jungle vine feature. */
class VineOracle {
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

    /**
     * Stone pillars with air between them across the y range the feature scatters into. A vine needs
     * a sturdy face on one of five sides, so open air alone would place nothing and a solid block
     * would leave no air to place into: the pillars give both, and different faces at different
     * positions, which is what makes the direction order observable.
     */
    private fun pillarChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60 until 64) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                if ((x % 4 == 0 && z % 3 == 0) || (x % 5 == 2 && z % 4 == 1)) {
                    for (y in 64..100) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var vines = 0
        for (y in 60..101) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.isVineState(id)) vines++
                }
            }
        }
        return checksum to vines
    }

    private fun report(name: String, biome: String, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var vines = 0
        var deferred = 0
        for (index in 0 until 60) {
            val chunk = pillarChunk(index * 3 - 30, index * -5 + 15)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkVines) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            vines += chunkVines
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name vines=$vines deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("jungle", "minecraft:jungle") { c, e -> VinePatchDecoration.decorate(c, 191919L, e) }
        report("bamboo", "minecraft:bamboo_jungle") { c, e -> VinePatchDecoration.decorate(c, 191919L, e) }
        report("sparse", "minecraft:sparse_jungle") { c, e -> VinePatchDecoration.decorate(c, 191919L, e) }
        // plains is not one of the three jungle biomes, so this is the real rejection case.
        report("wrong", "minecraft:plains") { c, e -> VinePatchDecoration.decorate(c, 191919L, e) }
        report("regional", "minecraft:jungle") { c, e ->
            VinePatchDecoration.decorate(c, 191919L, e, externalBlockState = { _, y, _ ->
                if (y in 60..100) Blocks.STONE.stateId else Blocks.AIR.stateId
            })
        }

        // Each side on its own, so the five states and the direction priority are pinned directly.
        val states = StringBuilder()
        for (face in 0 until 6) {
            val chunk = Chunk(0, 0)
            val dx = when (face) { 2 -> 1; 3 -> -1; else -> 0 }
            val dy = if (face == 0) 1 else 0
            val dz = when (face) { 4 -> 1; 5 -> -1; else -> 0 }
            if (face != 1) chunk.setBlockStateId(8 + dx, 70 + dy, 8 + dz, Blocks.STONE.stateId)
            VinePatchDecoration.placeCaveAt(chunk, DecorationPos(8, 70, 8))
            states.append(chunk.getBlockStateId(8, 70, 8)).append(',')
        }
        println("ORACLE sides=$states")
    }
}
