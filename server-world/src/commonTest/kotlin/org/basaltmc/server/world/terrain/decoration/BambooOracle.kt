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

/** The frozen Kotlin as the oracle for the bamboo clumps. */
class BambooOracle {
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

    /** The block this terrain has at one position, shared with the reader for the neighbours. */
    private fun terrainAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        if (y < -64 || y >= 100) return null
        val surface = 68 + (x * 3 + z * 7).mod(3)
        // A low ceiling over some columns, so a clump can stop at two blocks and never reach the
        // three the leafy top needs.
        if (y == surface + 3 && (x * 11 + z * 5).mod(6) == 2) return stateId("stone")
        if (y > surface) return Blocks.AIR.stateId
        if (y == surface) {
            // Sand and gravel hold bamboo but do not take podzol, which is the one difference
            // between the two support sets.
            return when ((x * 5 + z).mod(7)) {
                0 -> stateId("sand")
                1 -> stateId("gravel")
                2 -> stateId("stone")
                else -> Blocks.GRASS_BLOCK.stateId
            }
        }
        return stateId("dirt")
    }

    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60 until 100) {
                    chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z)!!)
                }
            }
        }
        return chunk
    }

    private val counted = setOf("bamboo", "podzol")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 60 until 100) {
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
        index: Int?,
        count: Int = 8,
        outside: Boolean = false,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = groundChunk(chunkIndex * 3 - 4, chunkIndex * -2 + 6)
            val external = if (outside) ::terrainAt else null
            if (index == null) {
                BambooDecoration.decorate(chunk, 1279L + chunkIndex, FakeEnvironment(biome), external)
            } else {
                BambooDecoration.decorateFeature(index, chunk, 1279L + chunkIndex, FakeEnvironment(biome), external)
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
        // No biome carries both entries, so the index loop is exercised through the light one.
        report("decorateBoth", "minecraft:jungle", null, 24)
        report("dense", "minecraft:bamboo_jungle", 2)
        report("denseOutside", "minecraft:bamboo_jungle", 2, outside = true)
        // Neither index matches, so both draw their positions and place nothing.
        report("plains", "minecraft:plains", null, 24)

        var checksum = -3750763034362895579L
        var placed = 0
        val known = StringBuilder()
        var trailing = 0
        val features = BambooDecoration.PLACED_IDS.toList().sorted() + "not_a_feature"
        for (feature in features) {
            for (chunkIndex in 0 until 3) {
                val chunk = groundChunk(chunkIndex, -chunkIndex)
                val random = WorldgenRandom(LegacyRandom(683L + chunkIndex * 1213L))
                val worldX = chunk.chunkX * 16 + 8
                val worldZ = chunk.chunkZ * 16 + 8
                // The first air over the rolling surface, which is where a clump can actually root.
                val origin = DecorationPos(worldX, 69 + (worldX * 3 + worldZ * 7).mod(3), worldZ)
                known.append(if (BambooDecoration.placeAt(chunk, random, origin, feature)) '1' else '0')
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                trailing = random.nextInt()
            }
        }
        println("ORACLE placeAt known=$known placed=$placed checksum=$checksum after=$trailing")

        for (name in listOf("light", "dense")) {
            var stepChecksum = -3750763034362895579L
            var stepPlaced = 0
            var stepDeferred = 0
            var after = 0
            val step = if (name == "light") BambooDecoration.LIGHT else BambooDecoration.DENSE
            for (chunkIndex in 0 until 12) {
                val chunk = groundChunk(chunkIndex * 2 - 1, chunkIndex + 2)
                val random = WorldgenRandom(XoroshiroRandom(3457L + chunkIndex))
                step.decorate(chunk, random)
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                stepChecksum = (stepChecksum xor chunkChecksum) * 1099511628211L
                stepPlaced += chunkPlaced
                stepDeferred += chunk.takeGeneratedBlockWrites().size
                after = random.nextInt()
            }
            println(
                "ORACLE vegetal_$name placed=$stepPlaced deferred=$stepDeferred checksum=$stepChecksum after=$after",
            )
        }
    }
}
