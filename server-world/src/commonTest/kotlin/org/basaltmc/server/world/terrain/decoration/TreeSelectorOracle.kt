package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the named tree selectors and the huge mushrooms. */
class TreeSelectorOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
        surface: String,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId(surface))
            }
        }
        return chunk
    }

    /** Powder snow over snow blocks, which is the only ground the grove selector accepts. */
    private fun groveChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = groundChunk(chunkX, chunkZ, "snow_block")
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 71..(71 + (x + z) % 3)) chunk.setBlockStateId(x, y, z, stateId("powder_snow"))
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

    private fun changedBlocks(
        chunk: Chunk,
        reference: Chunk,
    ): Int {
        var changed = 0
        for (y in 60..140) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    if (chunk.getBlockStateId(x, y, z) != reference.getBlockStateId(x, y, z)) changed++
                }
            }
        }
        return changed
    }

    private val seeds = longArrayOf(11L, 907L, 31337L, 654321L, 8675309L, 42424242L, 987654321L, 5150L)

    private fun report(
        name: String,
        surface: String,
        originY: Int = 71,
        grove: Boolean = false,
    ) {
        var checksum = -3750763034362895579L
        var changed = 0
        var deferred = 0
        val placedFlags = StringBuilder()
        var trailing = 0
        for ((index, seed) in seeds.withIndex()) {
            val chunkX = index * 5 - 30
            val chunkZ = index * -3 + 12
            val chunk = if (grove) groveChunk(chunkX, chunkZ) else groundChunk(chunkX, chunkZ, surface)
            val reference = if (grove) groveChunk(chunkX, chunkZ) else groundChunk(chunkX, chunkZ, surface)
            val random = WorldgenRandom(LegacyRandom(seed))
            val known =
                TreeDecoration.placeAt(
                    chunk,
                    random,
                    DecorationPos(chunkX * 16 + 8, originY, chunkZ * 16 + 8),
                    name.removeSuffix("_on_sand"),
                )
            placedFlags.append(if (known) '1' else '0')
            changed += changedBlocks(chunk, reference)
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            deferred += chunk.takeGeneratedBlockWrites().size
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name known=$placedFlags changed=$changed deferred=$deferred " +
                "checksum=$checksum after=$trailing",
        )
    }

    @Test
    fun dump() {
        report("trees_plains", "grass_block")
        report("trees_birch_and_oak_leaf_litter", "grass_block")
        report("trees_birch", "grass_block")
        report("birch_tall", "grass_block")
        report("trees_taiga", "grass_block")
        report("trees_snowy", "grass_block")
        report("trees_old_growth_spruce_taiga", "grass_block")
        report("trees_old_growth_pine_taiga", "grass_block")
        report("trees_savanna", "grass_block")
        report("trees_jungle", "grass_block")
        report("trees_sparse_jungle", "grass_block")
        report("trees_windswept_hills", "grass_block")
        report("trees_grove", "snow_block", grove = true)
        report("trees_badlands", "coarse_dirt")
        report("trees_meadow", "grass_block")
        report("trees_water", "grass_block")
        report("trees_flower_forest", "grass_block")
        report("trees_mangrove", "mud")
        report("trees_cherry", "grass_block")
        report("trees_swamp", "grass_block")
        report("pale_garden_vegetation", "grass_block")
        report("dark_forest_vegetation", "grass_block")
        // A birch sapling cannot survive on sand, so every entry rejects and only the draws happen.
        report("trees_birch_on_sand", "sand")
        report("unknown_selector", "grass_block")

        // The two huge mushrooms on their own, so their own shape and rejection are pinned.
        for (kind in HugeMushroomFeature.Kind.entries) {
            var checksum = -3750763034362895579L
            var placedCount = 0
            val random = WorldgenRandom(LegacyRandom(24680L))
            for (index in 0 until 16) {
                val chunk = groundChunk(index, -index, if (index % 4 == 0) "stone" else "mycelium")
                if (HugeMushroomFeature.place(chunk, random, index * 16 + 8, 71, -index * 16 + 8, kind)) {
                    placedCount++
                }
                checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            }
            println("ORACLE mushroom_$kind placed=$placedCount checksum=$checksum after=${random.nextInt()}")
        }
    }
}
