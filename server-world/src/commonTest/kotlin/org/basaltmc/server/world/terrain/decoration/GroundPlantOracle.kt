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

/** The frozen Kotlin as the oracle for the five dead-bush and dry-grass ground patches. */
class GroundPlantOracle {
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

    /** Sand, terracotta and plain stone side by side: the first two support dry plants, stone does not. */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(
                    x, 70, z,
                    when ((x + z) % 3) {
                        0 -> stateId("sand")
                        1 -> stateId("red_terracotta")
                        else -> Blocks.STONE.stateId
                    },
                )
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var bushes = 0
        var grass = 0
        for (y in 69..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("dead_bush")) bushes++
                    if (id == stateId("short_dry_grass") || id == stateId("tall_dry_grass")) grass++
                }
            }
        }
        return Triple(checksum, bushes, grass)
    }

    private fun report(name: String, biome: String, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var bushes = 0
        var grass = 0
        var deferred = 0
        for (index in 0 until 120) {
            val chunk = groundChunk(index * 3 - 60, index * -5 + 20)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkBushes, chunkGrass) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            bushes += chunkBushes
            grass += chunkGrass
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name bushes=$bushes grass=$grass deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("desert", "minecraft:desert") { c, e -> GroundPlantPatchDecoration.decorate(c, 616161L, e) }
        report("badlands", "minecraft:badlands") { c, e -> GroundPlantPatchDecoration.decorate(c, 616161L, e) }
        report("swamp", "minecraft:swamp") { c, e -> GroundPlantPatchDecoration.decorate(c, 616161L, e) }
        // plains is in none of the five specs, so this is the real rejection case.
        report("wrong", "minecraft:plains") { c, e -> GroundPlantPatchDecoration.decorate(c, 616161L, e) }
        report("desertGrassOnly", "minecraft:desert") { c, e ->
            GroundPlantPatchDecoration.decorateFeature(c, 616161L, e, "patch_dry_grass_desert")
        }
        report("regional", "minecraft:desert") { c, e ->
            GroundPlantPatchDecoration.decorate(c, 616161L, e, externalBlockState = { _, y, _ ->
                if (y in 66..69) Blocks.STONE.stateId
                else if (y == 70) stateId("sand")
                else Blocks.AIR.stateId
            })
        }

        val chunk = groundChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(998877L))
        val ok = GroundPlantPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "patch_dry_grass_desert")
        val bad = GroundPlantPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "nope")
        val (checksum, bushes, grass) = fold(chunk)
        println("ORACLE direct=$ok,$bad bushes=$bushes grass=$grass checksum=$checksum after=" + random.nextInt())
        println("ORACLE ids=" + GroundPlantPatchDecoration.PLACED_IDS.sorted().joinToString(","))
    }
}
