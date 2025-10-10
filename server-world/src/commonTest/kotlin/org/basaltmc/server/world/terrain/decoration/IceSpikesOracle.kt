package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the ice spikes and ice patches. */
class IceSpikesOracle {
    private inner class FakeEnvironment(
        private val biome: String,
    ) : SurfaceRuleEnvironment {
        override fun surfaceDepthNoise(
            x: Int,
            z: Int,
        ): Int = 0

        override fun surfaceSecondaryNoise(
            x: Int,
            z: Int,
        ): Double = 0.0

        override fun sample2d(
            noiseId: String,
            x: Int,
            z: Int,
        ): Double = 0.0

        override fun sample3d(
            noiseId: String,
            x: Int,
            y: Int,
            z: Int,
        ): Double = 0.0

        override fun verticalGradientRandom(
            name: String,
            x: Int,
            y: Int,
            z: Int,
        ): Float = 0.5f

        override fun positionalRandom(
            x: Int,
            y: Int,
            z: Int,
        ): BasaltRandom = XoroshiroRandom(x * 31L + z)

        override fun estimatedSurfaceHeight(
            x: Int,
            z: Int,
        ): Int = 70

        override fun biomeAt(
            x: Int,
            y: Int,
            z: Int,
        ): String = biome

        override fun noiseBiomeAt(
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            cache: DensityCache?,
        ): String = biome

        override fun isSteep(
            x: Int,
            z: Int,
        ): Boolean = false

        override fun clayBand(
            x: Int,
            y: Int,
            z: Int,
        ): Int = 0
    }

    /**
     * Snow blocks over stone, well above y=50: the spike roots downward until it hits something it
     * may not replace, and its pillar stops at the literal 50 rather than the world floor, so the
     * surface has to sit above that for the run-length logic to be reached at all.
     */
    private fun snowChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 40 until 66) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                // Dirt is replaceable by a spike, plain stone is not: the pillar stops on stone.
                for (y in 66 until 72) chunk.setBlockStateId(x, y, z, stateId("dirt"))
                chunk.setBlockStateId(x, 72, z, Blocks.SNOW_BLOCK.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var ice = 0
        for (y in 40..120) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == Blocks.PACKED_ICE.stateId) ice++
                }
            }
        }
        return checksum to ice
    }

    private fun report(
        name: String,
        biome: String,
        body: (Chunk, FakeEnvironment) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var ice = 0
        for (index in 0 until 60) {
            val chunk = snowChunk(index * 3 - 30, index * -5 + 10)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkIce) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            ice += chunkIce
        }
        println("ORACLE $name ice=$ice checksum=$checksum")
    }

    @Test
    fun dump() {
        report("iceSpikes", "minecraft:ice_spikes") { c, e -> IceSpikesDecoration.decorate(c, 24242L, e) }
        // snowy_plains is not the ice-spikes biome, so this is the real rejection case.
        report("wrong", "minecraft:snowy_plains") { c, e -> IceSpikesDecoration.decorate(c, 24242L, e) }

        // The two features on their own, so the spike shape and the patch disc are pinned apart.
        val sizes = StringBuilder()
        for (feature in listOf("ice_spike", "ice_patch", "nope")) {
            var ok = true
            var total = 0
            for (index in 0 until 20) {
                val chunk = snowChunk(0, 0)
                val random = WorldgenRandom(LegacyRandom(70707L + index * 6364136223846793005L))
                ok = IceSpikesDecoration.placeAt(chunk, random, DecorationPos(8, 72, 8), feature)
                total += fold(chunk).second
            }
            sizes.append("$feature=$ok/$total,")
        }
        println("ORACLE features=$sizes")
        println("ORACLE ids=" + IceSpikesDecoration.PLACED_IDS.sorted().joinToString(","))
    }
}
