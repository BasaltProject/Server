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

/** The frozen Kotlin as the oracle for the berry and pumpkin patches. */
class GrassScatterOracle {
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

    private fun grassChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                // Only the layers these patches actually read: they touch the origin and the block
                // under it, never deeper, and filling the whole column dominated the run time.
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 69..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (!Blocks.isAir(id) && id != Blocks.STONE.stateId && id != Blocks.GRASS_BLOCK.stateId) {
                        placed++
                    }
                }
            }
        }
        return checksum to placed
    }

    /**
     * Over many chunks with spaced seeds: rarity here runs to 1/384 and 1/300, and sequential legacy
     * seeds leave the very draw those filters read correlated.
     */
    private fun report(name: String, biome: String, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (index in 0 until 600) {
            val chunk = grassChunk(index * 3 - 100, index * -7 + 50)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("berryTaiga", "minecraft:taiga") { c, e -> BerryPatchDecoration.decorate(c, 606060L, e) }
        report("berrySnowy", "minecraft:snowy_taiga") { c, e -> BerryPatchDecoration.decorate(c, 606060L, e) }
        report("berryWrong", "minecraft:desert") { c, e -> BerryPatchDecoration.decorate(c, 606060L, e) }
        report("berryCommonOnly", "minecraft:taiga") { c, e ->
            BerryPatchDecoration.decorateFeature(c, 606060L, e, "patch_berry_common")
        }
        report("pumpkin", "minecraft:plains") { c, e -> PumpkinPatchDecoration.decorate(c, 606060L, e) }
        // mushroom_fields is genuinely absent from the pumpkin list; desert is in it, so using
        // desert here would have tested acceptance twice and rejection never.
        report("pumpkinWrong", "minecraft:mushroom_fields") { c, e ->
            PumpkinPatchDecoration.decorate(c, 606060L, e)
        }
        report("berryRegional", "minecraft:taiga") { c, e ->
            BerryPatchDecoration.decorate(c, 606060L, e, externalBlockState = { _, y, _ ->
                if (y < 70) Blocks.STONE.stateId
                else if (y == 70) Blocks.GRASS_BLOCK.stateId
                else Blocks.AIR.stateId
            })
        }

        val chunk = grassChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(90210L))
        BerryPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8))
        PumpkinPatchDecoration.placeAt(chunk, random, DecorationPos(4, 71, 12))
        println("ORACLE direct=" + fold(chunk).second + " checksum=" + fold(chunk).first +
            " after=" + random.nextInt())
    }
}
