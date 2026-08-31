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

/** The frozen Kotlin as the oracle for the pale moss patch. */
class PaleMossOracle {
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
        Blocks.REGISTRY.byIdentifier(org.basaltmc.wire.common.Identifier.vanilla(path))?.stateId
            ?: error("unknown block '$path'")

    /** Uneven ground: the patch has to find each column's own surface. */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val top = 68 + (x + z) % 3
                for (y in -64 until top) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, top, z, stateId(if ((x * 7 + z) % 5 == 0) "gravel" else "grass_block"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var moss = 0
        var plants = 0
        for (y in 60..90) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    when (Blocks.byStateId(id)?.block?.path) {
                        "pale_moss_block" -> moss++
                        "pale_moss_carpet", "short_grass", "tall_grass" -> plants++
                    }
                }
            }
        }
        return Triple(checksum, moss, plants)
    }

    private fun report(name: String, biome: String, count: Int, body: (Chunk, FakeEnvironment, Long) -> Unit) {
        var checksum = -3750763034362895579L
        var moss = 0
        var plants = 0
        var deferred = 0
        for (index in 0 until count) {
            val chunk = groundChunk(index * 5 - 30, index * -3 + 12)
            body(chunk, FakeEnvironment(biome), 900L + index)
            val (chunkChecksum, chunkMoss, chunkPlants) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            moss += chunkMoss
            plants += chunkPlants
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name moss=$moss plants=$plants deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("paleGarden", "minecraft:pale_garden", 20) { c, e, seed -> PaleMossDecoration.decorate(c, seed, e) }
        report("wrongBiome", "minecraft:plains", 20) { c, e, seed -> PaleMossDecoration.decorate(c, seed, e) }

        // The patch on its own, so the surface walk and the vegetation roll are pinned directly.
        var checksum = -3750763034362895579L
        var moss = 0
        var plants = 0
        val heights = StringBuilder()
        val random = WorldgenRandom(LegacyRandom(24680L))
        for (index in 0 until 12) {
            val chunk = groundChunk(index, -index)
            PaleMossDecoration.placePatchAt(chunk, random, DecorationPos(index * 16 + 8, 72, -index * 16 + 8))
            val (chunkChecksum, chunkMoss, chunkPlants) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            moss += chunkMoss
            plants += chunkPlants
            heights.append(chunkMoss).append(':').append(chunkPlants).append(',')
        }
        println("ORACLE patch moss=$moss plants=$plants checksum=$checksum after=${random.nextInt()} shape=$heights")
    }
}
