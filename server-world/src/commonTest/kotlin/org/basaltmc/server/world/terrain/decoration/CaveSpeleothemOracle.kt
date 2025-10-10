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

/** The frozen Kotlin as the oracle for the sulfur and dripstone speleothems. */
class CaveSpeleothemOracle {
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

    private val cavernBottoms = listOf(-40, -10, 20, 50, 80)
    private val cavernHeight = 8

    /**
     * Stone with flat caverns through it, and sulfur lining half of them: the sulfur spikes only
     * grow from sulfur or cinnabar, so a chunk of plain stone would place nothing at all.
     */
    private fun caveChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        val sulfur = stateId("sulfur")
        val cinnabar = stateId("cinnabar")
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 120) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (bottom in cavernBottoms) {
                    for (y in bottom until bottom + cavernHeight) {
                        chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                    }
                    if ((x + z) % 2 == 0) {
                        for (offset in 1..3) {
                            chunk.setBlockStateId(x, bottom - offset, z, sulfur)
                            chunk.setBlockStateId(x, bottom + cavernHeight - 1 + offset, z, cinnabar)
                        }
                    }
                }
            }
        }
        return chunk
    }

    private val counted =
        setOf("pointed_dripstone", "dripstone_block", "sulfur_spike", "water")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in -64 until 120) {
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

    /** The same terrain read from outside the chunk, so a cluster reaching past its edge lands. */
    private fun neighbourTerrain(
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        if (y < -64 || y >= 120) return null
        for (bottom in cavernBottoms) {
            if (y in bottom until bottom + cavernHeight) return Blocks.AIR.stateId
            if ((x + z) % 2 == 0) {
                if (y in bottom - 3 until bottom) return stateId("sulfur")
                if (y in bottom + cavernHeight until bottom + cavernHeight + 3) return stateId("cinnabar")
            }
        }
        return Blocks.STONE.stateId
    }

    private fun report(
        name: String,
        biome: String,
        index: Int? = null,
        count: Int = 4,
        outside: Boolean = false,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = caveChunk(chunkIndex * 3 - 7, chunkIndex * -2 + 5)
            val external = if (outside) ::neighbourTerrain else null
            if (index == null) {
                CaveSpeleothemDecoration.decorate(chunk, 8821L + chunkIndex, FakeEnvironment(biome), external)
            } else {
                CaveSpeleothemDecoration.decorateFeatures(
                    chunk,
                    8821L + chunkIndex,
                    FakeEnvironment(biome),
                    index..index,
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
        report("dripstoneStep", "minecraft:dripstone_caves")
        report("sulfurStep", "minecraft:sulfur_caves")
        // Neither biome matches, so every attempt has to be rejected before it reaches terrain.
        report("plainsStep", "minecraft:plains")
        // With the neighbouring terrain readable, the part of a cluster past the chunk edge lands
        // instead of being dropped against air.
        report("dripstoneOutside", "minecraft:dripstone_caves", outside = true)
        report("sulfurOutside", "minecraft:sulfur_caves", outside = true)
        for (index in 2..5) {
            val biome = if (index == 2 || index == 3) "minecraft:sulfur_caves" else "minecraft:dripstone_caves"
            report("index_$index", biome, index)
        }

        var checksum = -3750763034362895579L
        var placed = 0
        val known = StringBuilder()
        var trailing = 0
        val features = CaveSpeleothemDecoration.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            for (chunkIndex in 0 until 3) {
                val chunk = caveChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(977L + chunkIndex * 4441L))
                // The lining alternates by column, so walking z picks a sulfur column and a stone
                // one in turn: one origin for every feature would leave two of them with no base.
                val origin =
                    DecorationPos(chunk.chunkX * 16 + 8, cavernBottoms[1], chunk.chunkZ * 16 + 8 + chunkIndex)
                known.append(
                    if (CaveSpeleothemDecoration.placeAt(chunk, random, origin, feature)) '1' else '0',
                )
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known placed=$placed checksum=$checksum after=$trailing")
    }
}
