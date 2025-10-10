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

/** The frozen Kotlin as the oracle for the two jungle melon patches. */
class MelonOracle {
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
     * Grass floor with a stripe of soft cover above it: a melon replaces more than air, so a chunk of
     * bare air would never exercise the replaceable list at all.
     */
    private fun jungleChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, Blocks.GRASS_BLOCK.stateId)
                if (z % 4 == 0) chunk.setBlockStateId(x, 71, z, stateId("short_grass"))
                if (z % 4 == 1) chunk.setBlockStateId(x, 71, z, stateId("fern"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var melons = 0
        for (y in 69..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("melon")) melons++
                }
            }
        }
        return checksum to melons
    }

    private fun report(
        name: String,
        biome: String,
        body: (Chunk, FakeEnvironment) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var melons = 0
        var deferred = 0
        for (index in 0 until 200) {
            val chunk = jungleChunk(index * 3 - 90, index * -7 + 30)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkMelons) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            melons += chunkMelons
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name melons=$melons deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("jungle", "minecraft:jungle") { c, e -> MelonPatchDecoration.decorate(c, 246810L, e) }
        report("bambooJungle", "minecraft:bamboo_jungle") { c, e -> MelonPatchDecoration.decorate(c, 246810L, e) }
        report("sparseJungle", "minecraft:sparse_jungle") { c, e -> MelonPatchDecoration.decorate(c, 246810L, e) }
        // plains is in neither spec's list, so this is the real rejection case.
        report("wrong", "minecraft:plains") { c, e -> MelonPatchDecoration.decorate(c, 246810L, e) }
        report("sparseOnly", "minecraft:sparse_jungle") { c, e ->
            MelonPatchDecoration.decorateFeature(c, 246810L, e, "patch_melon_sparse")
        }
        report("regional", "minecraft:jungle") { c, e ->
            MelonPatchDecoration.decorate(c, 246810L, e, externalBlockState = { _, y, _ ->
                if (y in 66..69) {
                    Blocks.STONE.stateId
                } else if (y == 70) {
                    Blocks.GRASS_BLOCK.stateId
                } else {
                    Blocks.AIR.stateId
                }
            })
        }

        val chunk = jungleChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(112233L))
        MelonPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8))
        println(
            "ORACLE direct=" + fold(chunk).second + " checksum=" + fold(chunk).first +
                " after=" + random.nextInt(),
        )
    }
}
