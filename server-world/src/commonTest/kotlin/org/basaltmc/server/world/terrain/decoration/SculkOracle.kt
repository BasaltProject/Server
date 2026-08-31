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

/** The frozen Kotlin as the oracle for the deep dark sculk veins and patches. */
class SculkOracle {
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

    private val cavernBottoms = listOf(-40, -16, 8)
    private val cavernHeight = 6

    /** Shared by the chunk fill and the neighbour reader: a patch spreads well past its own chunk. */
    private fun terrainAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        if (y < -64 || y >= 32) return null
        for (bottom in cavernBottoms) {
            if (y in bottom until bottom + cavernHeight) {
                // A flooded strip so the waterlogged vein and the drowned sensor both get placed.
                return if (bottom == -16 && y == bottom) Blocks.WATER.stateId else Blocks.AIR.stateId
            }
        }
        // Single-block seams: a cursor in one has rock right above it, which is the only way the
        // charge decays by distance instead of being spent on a sensor.
        if (y == -28 || y == -4 || y == 20) return Blocks.AIR.stateId
        // Deepslate bricks are replaceable by world generation only, which is the one difference
        // between the two replaceable sets.
        if ((x * 13 + y * 7 + z).mod(23) == 0) return stateId("deepslate_bricks")
        return if ((x * 3 + z).mod(7) == 0) stateId("stone") else stateId("deepslate")
    }

    private fun caveChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 32) {
                    chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z)!!)
                }
            }
        }
        return chunk
    }

    private val counted = setOf("sculk", "sculk_vein", "sculk_catalyst", "sculk_sensor", "sculk_shrieker")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in -64 until 32) {
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
        index: Int? = null,
        count: Int = 2,
        outside: Boolean = false,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = caveChunk(chunkIndex * 4 - 6, chunkIndex * -3 + 9)
            val external = if (outside) ::terrainAt else null
            if (index == null) {
                SculkDecoration.decorate(chunk, 2287L + chunkIndex, FakeEnvironment(biome), external)
            } else {
                SculkDecoration.decorateFeatures(
                    chunk,
                    2287L + chunkIndex,
                    FakeEnvironment(biome),
                    listOf(index),
                    external,
                )
            }
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("bothSteps", "minecraft:deep_dark")
        report("veinIndex", "minecraft:deep_dark", index = 0)
        report("patchIndex", "minecraft:deep_dark", index = 1)
        report("patchOutside", "minecraft:deep_dark", index = 1, outside = true)
        // Any other biome leaves both steps drawing positions and placing nothing.
        report("plainsStep", "minecraft:plains")

        var checksum = -3750763034362895579L
        var placed = 0
        val known = StringBuilder()
        var trailing = 0
        val features = SculkDecoration.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            for (chunkIndex in 0 until 3) {
                val chunk = caveChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(1471L + chunkIndex * 2617L))
                // The floor of a different cavern each time: one dry, one flooded, one higher up.
                val origin = DecorationPos(chunk.chunkX * 16 + 8, cavernBottoms[chunkIndex], chunk.chunkZ * 16 + 8)
                known.append(if (SculkDecoration.placeAt(chunk, random, origin, feature)) '1' else '0')
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known placed=$placed checksum=$checksum after=$trailing")
    }
}
