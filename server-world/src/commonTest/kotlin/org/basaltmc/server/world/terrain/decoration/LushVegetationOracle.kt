package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the lush cave moss and clay patches. */
class LushVegetationOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** A cave: solid stone with a hollow band, so both the floor and the ceiling are reachable. */
    private fun caveChunk(chunkX: Int, chunkZ: Int, flooded: Boolean): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0..40) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                val floor = 20 + (x + z) % 3
                for (y in floor..(floor + 6)) {
                    chunk.setBlockStateId(x, y, z, if (flooded) Blocks.WATER.stateId else Blocks.AIR.stateId)
                }
            }
        }
        return chunk
    }

    private fun counts(chunk: Chunk): IntArray {
        var moss = 0
        var clay = 0
        var plants = 0
        var dripleaf = 0
        var water = 0
        for (y in 0..45) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    when (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path) {
                        "moss_block" -> moss++
                        "clay" -> clay++
                        "moss_carpet", "short_grass", "tall_grass", "azalea", "flowering_azalea" -> plants++
                        "small_dripleaf", "big_dripleaf", "big_dripleaf_stem" -> dripleaf++
                        "cave_vines", "cave_vines_plant" -> plants++
                        "water" -> water++
                    }
                }
            }
        }
        return intArrayOf(moss, clay, plants, dripleaf, water)
    }

    private fun checksumOf(chunk: Chunk): Long {
        var checksum = -3750763034362895579L
        for (y in 0..45) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        return checksum
    }

    private fun report(
        name: String,
        originY: Int,
        flooded: Boolean = false,
        count: Int = 24,
        body: (Chunk, WorldgenRandom, DecorationPos) -> Boolean,
    ) {
        var checksum = -3750763034362895579L
        val totals = intArrayOf(0, 0, 0, 0, 0)
        val placedFlags = StringBuilder()
        var trailing = 0
        for (index in 0 until count) {
            val chunk = caveChunk(index * 5 - 30, index * -3 + 12, flooded)
            val random = WorldgenRandom(LegacyRandom(97L + index * 6151L))
            val origin = DecorationPos(chunk.chunkX * 16 + 8, originY, chunk.chunkZ * 16 + 8)
            placedFlags.append(if (body(chunk, random, origin)) '1' else '0')
            val chunkCounts = counts(chunk)
            for (i in totals.indices) totals[i] += chunkCounts[i]
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name placed=$placedFlags moss=${totals[0]} clay=${totals[1]} plants=${totals[2]} " +
                "dripleaf=${totals[3]} water=${totals[4]} checksum=$checksum after=$trailing",
        )
    }

    @Test
    fun dump() {
        report("floorMoss", 22) { c, r, o -> LushVegetationPatchFeature.placeFloorMoss(c, r, o) }
        report("ceilingMoss", 25) { c, r, o -> LushVegetationPatchFeature.placeCeilingMoss(c, r, o) }
        report("clay", 22) { c, r, o -> LushVegetationPatchFeature.placeClay(c, r, o) }
        report("clayDeep", 21, count = 48) { c, r, o -> LushVegetationPatchFeature.placeClay(c, r, o) }
        report("dripleafAt", 23) { c, r, o -> LushVegetationPatchFeature.placeDripleafAt(c, r, o) }
        // Solid stone all the way: the ground walk never finds air, so nothing is placed.
        report("solid", 5) { c, r, o -> LushVegetationPatchFeature.placeFloorMoss(c, r, o) }
    }
}
