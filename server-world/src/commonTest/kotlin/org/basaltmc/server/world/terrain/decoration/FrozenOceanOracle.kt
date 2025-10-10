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

/** The frozen Kotlin as the oracle for the icebergs and the blue ice. */
class FrozenOceanOracle {
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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Deep water over gravel, an ice sheet on top, and packed ice floes for the blue ice to grow on. */
    private fun oceanChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                // Only the band the icebergs and the blue ice can reach is filled; the rest stays air.
                for (y in 28..40) chunk.setBlockStateId(x, y, z, stateId("gravel"))
                for (y in 41..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                if ((worldX * 5 + worldZ).mod(3) != 0) chunk.setBlockStateId(x, 63, z, stateId("ice"))
                // Floes hanging under the sheet: the blue ice only grows where packed ice touches it.
                if ((worldX * 7 + worldZ).mod(11) == 0) {
                    for (y in 55..62) chunk.setBlockStateId(x, y, z, stateId("packed_ice"))
                }
                // Lone blocks with water all round them: the only support a growth finds there is
                // the one below, which is the direction the search deliberately skips.
                if ((worldX * 3 + worldZ * 5).mod(5) == 0) {
                    for (y in listOf(44, 47, 50, 53)) chunk.setBlockStateId(x, y, z, stateId("packed_ice"))
                }
            }
        }
        return chunk
    }

    private val counted = setOf("packed_ice", "blue_ice", "snow_block", "snow")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 30..100) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.byStateId(id)?.block?.path in counted) placed++
                }
            }
        }
        return checksum to placed
    }

    private fun report(
        name: String,
        biome: String,
        surface: Boolean,
        count: Int,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        for (chunkIndex in 0 until count) {
            val chunk = oceanChunk(chunkIndex * 2 - 9, chunkIndex * -3 + 6)
            if (surface) {
                FrozenOceanDecoration.decorateSurfaceStructures(chunk, 6673L + chunkIndex, FakeEnvironment(biome))
            } else {
                FrozenOceanDecoration.decorateLocalModifications(chunk, 6673L + chunkIndex, FakeEnvironment(biome))
            }
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
        }
        println("ORACLE $name placed=$placed checksum=$checksum")
    }

    @Test
    fun dump() {
        report("icebergs", "minecraft:frozen_ocean", surface = false, count = 64)
        report("blueIce", "minecraft:deep_frozen_ocean", surface = true, count = 8)
        // Any other biome leaves both steps drawing positions and placing nothing.
        report("warmIcebergs", "minecraft:warm_ocean", surface = false, count = 64)
        report("warmBlueIce", "minecraft:warm_ocean", surface = true, count = 8)

        var checksum = -3750763034362895579L
        var placed = 0
        val known = StringBuilder()
        var trailing = 0
        val features = FrozenOceanDecoration.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            for (chunkIndex in 0 until 3) {
                val chunk = oceanChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(4423L + chunkIndex * 1699L))
                val origin = DecorationPos(chunk.chunkX * 16 + 8, 58 + chunkIndex, chunk.chunkZ * 16 + 8)
                known.append(if (FrozenOceanDecoration.placeAt(chunk, random, origin, feature)) '1' else '0')
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known placed=$placed checksum=$checksum after=$trailing")
    }
}
