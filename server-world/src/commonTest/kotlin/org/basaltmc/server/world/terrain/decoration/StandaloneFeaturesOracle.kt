package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the shape features `/place feature` runs directly. */
class StandaloneFeaturesOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private enum class Ground { SOLID, CAVE, CORRIDOR }

    /** Solid rock of one kind, optionally hollowed: a wide cave, or a narrow corridor. */
    private fun rockChunk(
        chunkX: Int,
        chunkZ: Int,
        rock: String,
        ground: Ground,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0..60) chunk.setBlockStateId(x, y, z, stateId(rock))
            }
        }
        when (ground) {
            Ground.SOLID -> Unit
            Ground.CAVE ->
                for (x in 0 until 16) {
                    for (z in 0 until 16) {
                        for (y in 30..35) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                    }
                }
            // A monster room needs a solid floor and ceiling with a handful of open wall squares,
            // which only a corridor through solid rock gives it.
            Ground.CORRIDOR ->
                for (x in 0 until 16) {
                    for (z in 7..8) {
                        for (y in 31..32) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                    }
                }
        }
        return chunk
    }

    private fun checksumOf(chunk: Chunk): Long {
        var checksum = -3750763034362895579L
        for (y in 0..64) {
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
        originY: Int,
        rock: String = "stone",
        ground: Ground = Ground.CAVE,
        count: Int = 20,
        localX: Int = 8,
        localZ: Int = 8,
        withNeighbourTerrain: Boolean = false,
    ) {
        var checksum = -3750763034362895579L
        var deferred = 0
        var entities = 0
        var ticks = 0
        val placedFlags = StringBuilder()
        var trailing = 0
        for (index in 0 until count) {
            val chunk = rockChunk(index * 5 - 30, index * -3 + 12, rock, ground)
            val random = WorldgenRandom(LegacyRandom(613L + index * 4409L))
            val origin = DecorationPos(chunk.chunkX * 16 + localX, originY, chunk.chunkZ * 16 + localZ)
            // Outside the chunk the decoration view reads as air, which makes the shape checks
            // reject at an edge. Real generation hands them the neighbour's terrain instead.
            val placed =
                if (withNeighbourTerrain) {
                    chunk.withExternalDecorationBlockState({ _, y, _ ->
                        if (y in 0..60) stateId(rock) else Blocks.AIR.stateId
                    }) {
                        StandaloneFeatures.placeAt(chunk, random, origin, name)
                    }
                } else {
                    StandaloneFeatures.placeAt(chunk, random, origin, name)
                }
            placedFlags.append(if (placed) '1' else '0')
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            deferred += chunk.takeGeneratedBlockWrites().size
            entities += chunk.blockEntityCount
            ticks += chunk.scheduledTickCount + chunk.takeGeneratedBlockTicks().size
            trailing = random.nextInt()
        }
        println(
            "ORACLE $label placed=$placedFlags deferred=$deferred entities=$entities ticks=$ticks " +
                "checksum=$checksum after=$trailing",
        )
    }

    @Test
    fun dump() {
        report("springClosed", "spring_closed", 25, rock = "netherrack", ground = Ground.SOLID)
        report("springClosedDouble", "spring_closed_double", 25, rock = "netherrack", ground = Ground.SOLID)
        report("monsterRoom", "monster_room", 31, ground = Ground.CORRIDOR)
        report("monsterRoomDeep", "monster_room_deep", 31, ground = Ground.CORRIDOR)
        report(
            "monsterRoomEdge",
            "monster_room",
            31,
            ground = Ground.CORRIDOR,
            localX = 1,
            localZ = 8,
            withNeighbourTerrain = true,
        )
        report("endPlatform", "end_platform", 32)
        report("voidStartPlatform", "void_start_platform", 32)
        report("endGatewayReturn", "end_gateway_return", 32)
        report("lakeLavaSurface", "lake_lava_surface", 20, ground = Ground.SOLID)
        report("lakeLavaUnderground", "lake_lava_underground", 20, ground = Ground.SOLID)
        report(
            "lakeLavaEdge",
            "lake_lava_surface",
            20,
            ground = Ground.SOLID,
            localX = 2,
            localZ = 13,
            withNeighbourTerrain = true,
        )
        report("caveVines", "cave_vines", 34)
        report("classicVines", "classic_vines_cave_feature", 35)
        report("lushVegetation", "lush_caves_vegetation", 32)
        report("lushCeilingVegetation", "lush_caves_ceiling_vegetation", 34)
        report("lushClay", "lush_caves_clay", 32)
        report("rootedAzaleaTree", "rooted_azalea_tree", 31)
        report("sulfurPool", "sulfur_pool", 20, ground = Ground.SOLID)
        // Solid rock all the way: the monster room's shape checks all reject.
        report("monsterRoomSolid", "monster_room", 31, ground = Ground.SOLID)
        report("unknownFeature", "unknown_feature", 32)
    }
}
