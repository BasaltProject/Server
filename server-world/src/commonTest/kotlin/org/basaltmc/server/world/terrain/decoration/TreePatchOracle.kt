package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the global tree placement step. */
class TreePatchOracle {
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

    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
        surface: String,
        water: Boolean,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId(surface))
                if (water) for (y in 71..72) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun checksumOf(chunk: Chunk): Long {
        var checksum = -3750763034362895579L
        for (y in 60..140) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        return checksum
    }

    private fun report(
        label: String,
        name: String,
        biome: String,
        surface: String = "grass_block",
        water: Boolean = false,
        count: Int = 12,
    ) {
        var checksum = -3750763034362895579L
        var deferred = 0
        var attempts = 0
        val origins = StringBuilder()
        for (index in 0 until count) {
            val chunk = groundChunk(index * 5 - 30, index * -3 + 12, surface, water)
            TreePatchDecoration.decorateFeature(chunk, 4242L + index, FakeEnvironment(biome), name) { _, position ->
                attempts++
                origins
                    .append(position.x)
                    .append(':')
                    .append(position.y)
                    .append(':')
                    .append(position.z)
                    .append(' ')
            }
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $label attempts=$attempts deferred=$deferred checksum=$checksum origins=$origins")
    }

    @Test
    fun dump() {
        // Counts are per feature: only the rare ones pay for the extra chunks they need to fire.
        report("plains", "trees_plains", "minecraft:plains", count = 60)
        report("birch", "trees_birch", "minecraft:birch_forest")
        report("forest", "trees_birch_and_oak_leaf_litter", "minecraft:forest")
        report("taiga", "trees_taiga", "minecraft:taiga")
        report("jungle", "trees_jungle", "minecraft:jungle", count = 4)
        report("bamboo", "bamboo_vegetation", "minecraft:bamboo_jungle", count = 4)
        report("darkForest", "dark_forest_vegetation", "minecraft:dark_forest", count = 4)
        report("paleGarden", "pale_garden_vegetation", "minecraft:pale_garden", count = 4)
        report("savanna", "trees_savanna", "minecraft:savanna")
        report("meadow", "trees_meadow", "minecraft:meadow", count = 400)
        report("swamp", "trees_swamp", "minecraft:swamp", water = true)
        report("mangrove", "trees_mangrove", "minecraft:mangrove_swamp", surface = "mud", water = true, count = 4)
        // trees_water allows no water above the ocean floor, so it only ever fires on a dry shore.
        report("water", "trees_water", "minecraft:river", count = 60)
        report("cherry", "trees_cherry", "minecraft:cherry_grove")
        // The biome filter rejects every origin, so only the placement draws happen.
        report("wrongBiome", "trees_plains", "minecraft:desert")
    }
}
