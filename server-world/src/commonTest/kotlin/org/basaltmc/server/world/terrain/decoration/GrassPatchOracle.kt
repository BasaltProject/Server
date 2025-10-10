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

/** The frozen Kotlin as the oracle for the nine grass and fern patches. */
class GrassPatchOracle {
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

    /** Grass, podzol and bare stone in stripes: the jungle spec rejects podzol specifically. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(
                    x,
                    70,
                    z,
                    when ((x + z) % 3) {
                        0 -> stateId("podzol")
                        1 -> Blocks.STONE.stateId
                        else -> Blocks.GRASS_BLOCK.stateId
                    },
                )
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var grass = 0
        var fern = 0
        for (y in 69..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("short_grass")) grass++
                    if (id == stateId("fern")) fern++
                }
            }
        }
        return Triple(checksum, grass, fern)
    }

    private fun report(
        name: String,
        biome: String,
        body: (Chunk, FakeEnvironment) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var grass = 0
        var fern = 0
        var deferred = 0
        for (index in 0 until 100) {
            val chunk = groundChunk(index * 3 - 50, index * -5 + 20)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkGrass, chunkFern) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            grass += chunkGrass
            fern += chunkFern
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name grass=$grass fern=$fern deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("jungle", "minecraft:jungle") { c, e -> GrassPatchDecoration.decorate(c, 8642L, e) }
        report("taiga", "minecraft:taiga") { c, e -> GrassPatchDecoration.decorate(c, 8642L, e) }
        report("plains", "minecraft:plains") { c, e -> GrassPatchDecoration.decorate(c, 8642L, e) }
        report("meadow", "minecraft:meadow") { c, e -> GrassPatchDecoration.decorate(c, 8642L, e) }
        report("savanna", "minecraft:savanna") { c, e -> GrassPatchDecoration.decorate(c, 8642L, e) }
        // the_void is in none of the nine specs, so this is the real rejection case.
        report("wrong", "minecraft:the_void") { c, e -> GrassPatchDecoration.decorate(c, 8642L, e) }
        report("jungleOnly", "minecraft:jungle") { c, e -> GrassPatchDecoration.decorateFeature(11, c, 8642L, e) }
        report("regional", "minecraft:plains") { c, e ->
            GrassPatchDecoration.decorate(c, 8642L, e, externalBlockState = { _, y, _ ->
                if (y in 66..69) {
                    Blocks.STONE.stateId
                } else if (y == 70) {
                    Blocks.GRASS_BLOCK.stateId
                } else {
                    Blocks.AIR.stateId
                }
            })
        }

        val chunk = groundChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(13131L))
        val ok = GrassPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "patch_grass_taiga")
        val bad = GrassPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "nope")
        val (checksum, grass, fern) = fold(chunk)
        println("ORACLE direct=$ok,$bad grass=$grass fern=$fern checksum=$checksum after=" + random.nextInt())
    }
}
