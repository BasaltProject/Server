package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for whole trees built from the real configurations. */
class TreeFeatureOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Grass over stone, with one obstacle column so the free-height clip can bite. */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId("grass_block"))
            }
        }
        for (y in 71..80) chunk.setBlockStateId(14, y, 2, Blocks.STONE.stateId)
        return chunk
    }

    /**
     * The same ground with a neighbouring trunk already standing: a creaking heart needs a log with
     * logs on all six sides, which one tree on its own can never produce.
     */
    private fun creakingChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = groundChunk(chunkX, chunkZ)
        for (x in 6..7) {
            for (z in 7..8) {
                for (y in 71..80) chunk.setBlockStateId(x, y, z, stateId("pale_oak_log"))
            }
        }
        return chunk
    }

    /** Mud under shallow water, which is the only ground a mangrove will take. */
    private fun swampChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 66) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in 66..70) chunk.setBlockStateId(x, y, z, stateId("mud"))
                for (y in 71..72) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): IntArray {
        var logs = 0
        var leaves = 0
        var decorations = 0
        for (y in 60..140) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (TreeBlockRules.isLogState(id)) logs++
                    val path = Blocks.byStateId(id)?.block?.path ?: continue
                    if (path.endsWith("leaves")) leaves++
                    if (path == "vine" || path == "cocoa" || path == "podzol" || path == "bee_nest" ||
                        path == "leaf_litter" || path == "pale_hanging_moss" || path == "creaking_heart" ||
                        path == "mangrove_propagule" || path == "pale_moss_block" || path == "pale_moss_carpet"
                    ) {
                        decorations++
                    }
                }
            }
        }
        return intArrayOf(logs, leaves, decorations)
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

    private val seeds = longArrayOf(11L, 907L, 31337L, 654321L, 8675309L, 42424242L, 987654321L, 5150L)

    private fun report(
        name: String,
        config: TreeConfiguration,
        originY: Int = 71,
        swamp: Boolean = false,
        neighbourTrunk: Boolean = false,
        localX: Int = 8,
        localZ: Int = 8,
    ) {
        var checksum = -3750763034362895579L
        var logs = 0
        var leaves = 0
        var decorations = 0
        var deferred = 0
        var entities = 0
        val placedFlags = StringBuilder()
        var trailing = 0
        for ((index, seed) in seeds.withIndex()) {
            val chunk =
                when {
                    swamp -> swampChunk(index * 5 - 30, index * -3 + 12)
                    neighbourTrunk -> creakingChunk(index * 5 - 30, index * -3 + 12)
                    else -> groundChunk(index * 5 - 30, index * -3 + 12)
                }
            val random = WorldgenRandom(LegacyRandom(seed))
            val placed =
                TreeFeature.place(
                    chunk,
                    random,
                    chunk.chunkX * 16 + localX,
                    originY,
                    chunk.chunkZ * 16 + localZ,
                    config,
                )
            placedFlags.append(if (placed) '1' else '0')
            val counts = fold(chunk)
            logs += counts[0]
            leaves += counts[1]
            decorations += counts[2]
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            deferred += chunk.takeGeneratedBlockWrites().size
            entities += chunk.blockEntityCount + chunk.takeGeneratedBlockEntityWrites().size
            trailing = random.nextInt()
        }
        println(
            "ORACLE $name placed=$placedFlags logs=$logs leaves=$leaves decorations=$decorations " +
                "deferred=$deferred entities=$entities checksum=$checksum after=$trailing",
        )
    }

    @Test
    fun dump() {
        report("oak", TreeConfigs.OAK)
        report("oakEdge", TreeConfigs.OAK, localX = 1, localZ = 14)
        report("swampOak", TreeConfigs.SWAMP_OAK)
        report("birch", TreeConfigs.BIRCH)
        report("fancyOak", TreeConfigs.FANCY_OAK)
        report("jungleTree", TreeConfigs.JUNGLE_TREE)
        report("jungleBush", TreeConfigs.JUNGLE_BUSH)
        report("megaJungle", TreeConfigs.MEGA_JUNGLE)
        report("spruce", TreeConfigs.SPRUCE)
        report("pine", TreeConfigs.PINE)
        report("acacia", TreeConfigs.ACACIA)
        report("darkOak", TreeConfigs.DARK_OAK)
        report("paleOakCreaking", TreeConfigs.PALE_OAK_CREAKING)
        report("paleOakCreakingNeighbour", TreeConfigs.PALE_OAK_CREAKING, neighbourTrunk = true)
        report("megaSpruce", TreeConfigs.MEGA_SPRUCE)
        report("megaPine", TreeConfigs.MEGA_PINE)
        report("cherry", TreeConfigs.CHERRY)
        report("azalea", TreeConfigs.AZALEA)
        report("oakLeafLitter", TreeConfigs.OAK_LEAF_LITTER)
        report("fancyOakBees", TreeConfigs.FANCY_OAK_BEES)
        report("mangrove", TreeConfigs.MANGROVE, originY = 71, swamp = true)
        report("tallMangrove", TreeConfigs.TALL_MANGROVE, originY = 71, swamp = true)
        // Blocked: the trunk cannot clear the stone column beside it.
        report("blocked", TreeConfigs.OAK, localX = 14, localZ = 3)
    }
}
