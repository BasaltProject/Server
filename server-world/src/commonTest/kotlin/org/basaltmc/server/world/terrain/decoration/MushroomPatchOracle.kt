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

/** The frozen Kotlin as the oracle for the eight small-mushroom patches. */
class MushroomPatchOracle {
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
     * A stripe of non-solid-render top blocks beside solid ones: mushrooms test isSolidRender, so a
     * uniformly solid floor would never exercise the rejection half of that check.
     */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, if (x % 4 == 0) stateId("glass") else Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var brown = 0
        var red = 0
        for (y in 69..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("brown_mushroom")) brown++
                    if (id == stateId("red_mushroom")) red++
                }
            }
        }
        return Triple(checksum, brown, red)
    }

    /**
     * [chunks] is per case on purpose: the normal specs are 1/256 and 1/512, so they need a far wider
     * sample than the rest before they fire at all, and making every case pay for that made the suite
     * six times slower for no extra coverage.
     */
    private fun report(
        name: String,
        biome: String,
        chunks: Int = 150,
        body: (Chunk, FakeEnvironment) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var brown = 0
        var red = 0
        var deferred = 0
        for (index in 0 until chunks) {
            val chunk = groundChunk(index * 3 - 70, index * -5 + 25)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkBrown, chunkRed) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            brown += chunkBrown
            red += chunkRed
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name brown=$brown red=$red deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("swamp", "minecraft:swamp") { c, e -> MushroomPatchDecoration.decorate(c, 424242L, e) }
        report("oldGrowth", "minecraft:old_growth_pine_taiga") { c, e ->
            MushroomPatchDecoration.decorate(c, 424242L, e)
        }
        report("taiga", "minecraft:taiga") { c, e -> MushroomPatchDecoration.decorate(c, 424242L, e) }
        report("normal", "minecraft:plains", chunks = 900) { c, e -> MushroomPatchDecoration.decorate(c, 424242L, e) }
        // lush_caves is in none of the four lists, so this is the real rejection case.
        report(
            "wrong",
            "minecraft:lush_caves",
            chunks = 900,
        ) { c, e -> MushroomPatchDecoration.decorate(c, 424242L, e) }
        report("before", "minecraft:swamp") { c, e ->
            MushroomPatchDecoration.decorateBeforeMushroomIsland(c, 424242L, e)
        }
        report("after", "minecraft:taiga") { c, e ->
            MushroomPatchDecoration.decorateAfterMushroomIsland(c, 424242L, e)
        }
        report("swampBrownOnly", "minecraft:swamp") { c, e ->
            MushroomPatchDecoration.decorateFeature(c, 424242L, e, "brown_mushroom_swamp")
        }
        report("regional", "minecraft:swamp") { c, e ->
            MushroomPatchDecoration.decorate(c, 424242L, e, externalBlockState = { _, y, _ ->
                if (y in 66..70) Blocks.STONE.stateId else Blocks.AIR.stateId
            })
        }

        val chunk = groundChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(777L))
        val ok = MushroomPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "red_mushroom_taiga")
        val bad = MushroomPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "nope")
        val (checksum, brown, red) = fold(chunk)
        println("ORACLE direct=$ok,$bad brown=$brown red=$red checksum=$checksum after=" + random.nextInt())
        println("ORACLE ids=" + MushroomPatchDecoration.PLACED_IDS.sorted().joinToString(","))
    }
}
