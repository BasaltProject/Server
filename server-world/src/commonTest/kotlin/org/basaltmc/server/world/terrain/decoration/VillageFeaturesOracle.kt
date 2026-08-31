package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the features a village pool references. */
class VillageFeaturesOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Village ground: grass, farmland, sand and the dirt path a pile only half settles on. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                for (y in 60..69) chunk.setBlockStateId(x, y, z, stateId("dirt"))
                val top =
                    when ((worldX * 3 + worldZ * 5).mod(7)) {
                        0 -> stateId("dirt_path")
                        1 -> stateId("sand")
                        2 -> stateId("farmland")
                        3 -> stateId("stone")
                        else -> Blocks.GRASS_BLOCK.stateId
                    }
                chunk.setBlockStateId(x, 70, z, top)
            }
        }
        return chunk
    }

    private val ignored = setOf("dirt", "grass_block", "sand", "dirt_path", "farmland", "stone", "air")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 60..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path != null && path !in ignored) placed++
                }
            }
        }
        return checksum to placed
    }

    @Test
    fun dump() {
        val features =
            listOf(
                "minecraft:pile_hay",
                "minecraft:pile_melon",
                "minecraft:pile_snow",
                "minecraft:pile_ice",
                "minecraft:pile_pumpkin",
                "minecraft:acacia",
                "minecraft:oak",
                "minecraft:spruce",
                "minecraft:pine",
                "minecraft:flower_plain",
                "minecraft:patch_cactus",
                "minecraft:patch_taiga_grass",
                "minecraft:patch_berry_bush",
                "minecraft:not_a_feature",
            )
        for (feature in features) {
            var checksum = -3750763034362895579L
            var placed = 0
            var deferred = 0
            var trailing = 0
            for (chunkIndex in 0 until 4) {
                val chunk = groundChunk(chunkIndex * 2 - 3, chunkIndex + 1)
                val random = WorldgenRandom(LegacyRandom(1801L + chunkIndex * 2711L))
                VillageFeatures.place(feature, chunk, chunk.chunkX * 16 + 8, 71, chunk.chunkZ * 16 + 8, random)
                val (chunkChecksum, chunkPlaced) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                placed += chunkPlaced
                deferred += chunk.takeGeneratedBlockWrites().size
                trailing = random.nextInt()
            }
            println("ORACLE $feature placed=$placed deferred=$deferred checksum=$checksum after=$trailing")
        }
    }
}
