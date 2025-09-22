package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the top-layer freeze. */
class SurfaceFreezingOracle {
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
     * Water pools, grass, ice and honey stripes at varying heights: ice only forms on exposed water,
     * snow refuses to sit on ice and is forced onto honey, and the snowy property only exists on
     * grass, podzol and mycelium.
     */
    private fun terrainChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val top = 68 + ((x * 3 + z * 5) % 4)
                for (y in 60 until top) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                val surface = when ((x + z) % 5) {
                    0 -> Blocks.WATER.stateId
                    1 -> Blocks.GRASS_BLOCK.stateId
                    2 -> stateId("podzol")
                    3 -> stateId("ice")
                    else -> stateId("honey_block")
                }
                chunk.setBlockStateId(x, top, z, surface)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var snow = 0
        var ice = 0
        for (y in 60..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("snow")) snow++
                    if (id == stateId("ice")) ice++
                }
            }
        }
        return Triple(checksum, snow, ice)
    }

    private fun report(name: String, biome: String) {
        var checksum = -3750763034362895579L
        var snow = 0
        var ice = 0
        var snowy = 0
        for (index in 0 until 40) {
            val chunk = terrainChunk(index * 3 - 20, index * -5 + 10)
            SurfaceFreezingDecoration.decorate(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkSnow, chunkIce) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            snow += chunkSnow
            ice += chunkIce
            // The snowy variant of grass, podzol and mycelium sits one id below the bare state.
            for (y in 60..76) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        val id = chunk.getBlockStateId(x, y, z)
                        if (id == Blocks.GRASS_BLOCK.stateId - 1 || id == stateId("podzol") - 1) snowy++
                    }
                }
            }
        }
        println("ORACLE $name snow=$snow ice=$ice snowy=$snowy checksum=$checksum")
    }

    @Test
    fun dump() {
        report("snowyPlains", "minecraft:snowy_plains")
        report("frozenOcean", "minecraft:frozen_ocean")
        report("taiga", "minecraft:taiga")
        report("plains", "minecraft:plains")
        report("desert", "minecraft:desert")
    }
}
