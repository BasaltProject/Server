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

/** The frozen Kotlin as the oracle for the amethyst geodes and the large dripstones. */
class CaveLocalModificationOracle {
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

    private val cavernBottoms = listOf(-52)
    private val cavernHeight = 28

    /**
     * The block this terrain has at one position, shared by the chunk fill and the reader for the
     * neighbours: a geode is 33 blocks across, so most of one lands outside its own chunk.
     */
    private fun terrainAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        if (y < -64 || y >= 32) return null
        for (bottom in cavernBottoms) {
            if (y in bottom until bottom + cavernHeight) {
                // Lava on the floor of one cavern, which is where a dripstone refuses to root.
                if (y == bottom && bottom == -52 && (x + z) % 5 == 0) return stateId("lava")
                return Blocks.AIR.stateId
            }
        }
        if (y in -64..-61) return stateId("bedrock")
        // Single wet and molten blocks rather than a body of either: a geode is refused outright by
        // two invalid sample points, so a flooded cavern next door means it never generates at all.
        if ((x * 7 + y * 3 + z).mod(29) == 0) return Blocks.WATER.stateId
        if ((x * 5 + y * 11 + z).mod(31) == 0) return stateId("lava")
        return if ((x * 3 + z).mod(11) == 0) stateId("deepslate") else Blocks.STONE.stateId
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

    private val counted =
        setOf(
            "amethyst_block",
            "budding_amethyst",
            "calcite",
            "smooth_basalt",
            "small_amethyst_bud",
            "medium_amethyst_bud",
            "large_amethyst_bud",
            "amethyst_cluster",
            "dripstone_block",
        )

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
        count: Int = 4,
        outside: Boolean = false,
        base: Long = 5507L,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        var ticks = 0
        for (chunkIndex in 0 until count) {
            val chunk = caveChunk(chunkIndex * 3 - 11, chunkIndex * -2 + 4)
            val external = if (outside) ::terrainAt else null
            val surface = if (outside) { _: Int, _: Int -> 32 } else null
            if (index == null) {
                CaveLocalModificationDecoration.decorate(
                    chunk,
                    base + chunkIndex,
                    FakeEnvironment(biome),
                    surface,
                    external,
                )
            } else {
                CaveLocalModificationDecoration.decorateFeatures(
                    chunk,
                    base + chunkIndex,
                    FakeEnvironment(biome),
                    index..index,
                    surface,
                    external,
                )
            }
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
            ticks += chunk.scheduledFluidTicks().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred ticks=$ticks checksum=$checksum")
    }

    @Test
    fun dump() {
        report("bothSteps", "minecraft:dripstone_caves", count = 24)
        report("geodeOutside", "minecraft:plains", index = 2, count = 64, outside = true, base = 6607L)
        // Not an overworld biome, so the geode is rejected after its position is drawn.
        // The same draws as above with the biome refused, so the geodes that fired there do nothing here.
        report("netherGeode", "minecraft:nether_wastes", index = 2, count = 64, base = 6607L)
        report("dripstoneIndex", "minecraft:dripstone_caves", index = 3, count = 8)
        report("dripstoneOutside", "minecraft:dripstone_caves", index = 3, count = 8, outside = true)
        // Any other biome leaves the dripstone step drawing positions and placing nothing.
        report("plainsDripstone", "minecraft:plains", index = 3, count = 8)

        var checksum = -3750763034362895579L
        var placed = 0
        var ticks = 0
        val known = StringBuilder()
        var trailing = 0
        val features = CaveLocalModificationDecoration.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            for (chunkIndex in 0 until 3) {
                val chunk = caveChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(6151L + chunkIndex * 3319L))
                known.append(
                    if (CaveLocalModificationDecoration.placeAt(
                            chunk,
                            random,
                            chunk.chunkX * 16 + 8,
                            cavernBottoms[0] + 4,
                            chunk.chunkZ * 16 + 8,
                            feature,
                        )
                    ) {
                        '1'
                    } else {
                        '0'
                    },
                )
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                ticks += chunk.scheduledFluidTicks().size
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known placed=$placed ticks=$ticks checksum=$checksum after=$trailing")
    }
}
