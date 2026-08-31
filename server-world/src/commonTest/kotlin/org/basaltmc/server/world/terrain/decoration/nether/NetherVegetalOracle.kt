package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.NetherBiomeLookup
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the Nether vegetal decoration step. */
class NetherVegetalOracle {
    private class FixedBiome(private val biome: Biome) : NetherBiomeLookup {
        override fun blockBiomeAt(x: Int, y: Int, z: Int, cache: DensityCache?): Biome = biome
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * Netherrack with a nylium cap and a roof above the cavern, which is what these grow on, plus a
     * pocketed stone slab: the Nether lava spring reuses the overworld rock list, so netherrack on
     * its own never satisfies it.
     */
    private fun netherChunk(chunkX: Int, chunkZ: Int, nylium: String, thin: Boolean = false): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        if (thin) {
            // A mushroom needs a surface and a support below it and nothing else, and it is rare
            // enough to need a thousand chunks: anything more here is paid for a thousand times.
            for (x in 0 until 16) {
                for (z in 0 until 16) chunk.setBlockStateId(x, 40, z, stateId(nylium))
            }
            return chunk
        }
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 18..40) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
                for (y in 20..30) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                if ((x * 3 + z) % 5 == 0) chunk.setBlockStateId(x, 25, z, Blocks.AIR.stateId)
                chunk.setBlockStateId(x, 40, z, stateId(nylium))
                for (y in 60..75) chunk.setBlockStateId(x, y, z, stateId("netherrack"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk, thin: Boolean = false): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in (if (thin) 36 else 0)..(if (thin) 46 else 80)) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path != null && path != "netherrack" && path != "air" && path != "stone" &&
                        path != "crimson_nylium" && path != "warped_nylium"
                    ) {
                        placed++
                    }
                }
            }
        }
        return checksum to placed
    }

    private fun report(
        name: String,
        biome: Biome,
        nylium: String,
        count: Int = 12,
        index: Int? = null,
        thin: Boolean = false,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12, nylium, thin)
            val seed = 8080L + chunkIndex
            if (index == null) {
                NetherVegetalDecoration.decorate(chunk, seed, FixedBiome(biome))
            } else {
                NetherVegetalDecoration.decorateFeature(chunk, seed, FixedBiome(biome), index)
            }
            val (chunkChecksum, chunkPlaced) = fold(chunk, thin)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum")
    }

    private fun reportAt(name: String, feature: String, nylium: String, originY: Int, count: Int = 12) {
        var checksum = -3750763034362895579L
        var placed = 0
        val placedFlags = StringBuilder()
        var trailing = 0
        for (chunkIndex in 0 until count) {
            val chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12, nylium)
            val random = WorldgenRandom(LegacyRandom(4321L + chunkIndex * 5501L))
            val known =
                NetherVegetalDecoration.placeAt(
                    chunk,
                    random,
                    chunk.chunkX * 16 + 8,
                    originY,
                    chunk.chunkZ * 16 + 8,
                    feature,
                )
            placedFlags.append(if (known) '1' else '0')
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            trailing = random.nextInt()
        }
        println("ORACLE $name known=$placedFlags placed=$placed checksum=$checksum after=$trailing")
    }

    @Test
    fun dump() {
        report("crimsonStep", Biomes.CRIMSON_FOREST, "crimson_nylium")
        report("warpedStep", Biomes.WARPED_FOREST, "warped_nylium")
        report("wastesStep", Biomes.NETHER_WASTES, "netherrack")
        for ((index, name) in NetherVegetalDecoration.FEATURE_ORDER.withIndex()) {
            val warped = name.startsWith("warped") || name == "twisting_vines" || name == "nether_sprouts"
            val biome = if (warped) Biomes.WARPED_FOREST else Biomes.CRIMSON_FOREST
            // The two mushrooms are 1/256 and 1/512, so they need far more chunks than the rest.
            val mushroom = name.endsWith("mushroom_normal")
            report(
                "step_$name",
                biome,
                if (warped) "warped_nylium" else "crimson_nylium",
                if (mushroom) 1200 else 12,
                index,
                thin = mushroom,
            )
        }
        reportAt("crimsonFungiAt", "crimson_fungi", "crimson_nylium", 41)
        reportAt("warpedFungiAt", "warped_fungi", "warped_nylium", 41)
        reportAt("crimsonVegetationAt", "crimson_forest_vegetation", "crimson_nylium", 41)
        reportAt("warpedVegetationAt", "warped_forest_vegetation", "warped_nylium", 41)
        reportAt("netherSproutsAt", "nether_sprouts", "warped_nylium", 41)
        reportAt("twistingVinesAt", "twisting_vines", "warped_nylium", 41)
        reportAt("weepingVinesAt", "weeping_vines", "crimson_nylium", 59)
        reportAt("lavaSpringAt", "spring_lava_nether", "crimson_nylium", 25, count = 40)
        reportAt("unknownAt", "not_a_feature", "crimson_nylium", 41)
    }
}
