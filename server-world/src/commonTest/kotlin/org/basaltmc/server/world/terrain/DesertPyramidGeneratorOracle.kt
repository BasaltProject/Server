package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the desert pyramid. */
class DesertPyramidGeneratorOracle {
    /** A sloped desert, so the lowest column of the footprint is not the one it started on. */
    private fun surfaceAt(
        x: Int,
        z: Int,
    ): Int = 66 + (x * 2 + z).mod(5)

    /**
     * Caves the heightmap knows nothing about, so the columns a pyramid pours down its own footprint
     * have somewhere to fall. Without them the ground right under the base is solid and the pour
     * writes nothing at all.
     */
    private fun hollow(
        x: Int,
        z: Int,
    ): Boolean = x.mod(29) < 6 && z.mod(31) < 6

    private fun terrainAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val surface = surfaceAt(x, z)
        if (y >= surface) return Blocks.AIR.stateId
        if (hollow(x, z) && y > surface - 20) return Blocks.AIR.stateId
        return Blocks.STONE.stateId
    }

    private fun desertChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 30..90) {
                    chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z))
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 30..90) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != terrainAt(chunk.chunkX * 16 + x, y, chunk.chunkZ * 16 + z)) written++
                }
            }
        }
        for (entity in chunk.blockEntities()) {
            checksum = (checksum xor entity.data.getLong("LootTableSeed", 0L)) * 1099511628211L
            checksum = (
                checksum xor
                    entity.type.path
                        .hashCode()
                        .toLong()
            ) * 1099511628211L
        }
        for (start in chunk.structures.starts()) {
            for (bound in listOf(
                start.fullBounds.minX,
                start.fullBounds.minY,
                start.fullBounds.minZ,
                start.fullBounds.maxX,
                start.fullBounds.maxY,
                start.fullBounds.maxZ,
            )) {
                checksum = (checksum xor bound.toLong()) * 1099511628211L
            }
        }
        return Triple(checksum, written, chunk.blockEntities().size)
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
    ) {
        val generator =
            DesertPyramidStructureGenerator(
                // Only the centre column of a chunk answers, and only one block under its surface,
                // so where the probe stands and what height it reads are both observable.
                biomeAt = { x, y, z ->
                    if (x.mod(16) == 8 && z.mod(16) == 8 && y == surfaceAt(x, z) - 1) biome else "minecraft:ocean"
                },
                surfaceHeightAt = { x, z -> surfaceAt(x, z) },
            )
        val centre = DesertPyramidStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var starts = 0
        for (index in 0 until 36) {
            val chunk = desertChunk(centre.first + index % 6 - 3, centre.second + index / 6 - 3)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkBlockEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            blockEntities += chunkBlockEntities
            starts += chunk.structures.starts().size
        }
        println("ORACLE $name written=$written blockEntities=$blockEntities starts=$starts checksum=$checksum")
    }

    @Test
    fun dump() {
        report("desert", 1234L, "minecraft:desert")
        report("plains", 1234L, "minecraft:plains")
        report("otherSeed", 77L, "minecraft:desert")
        report("thirdSeed", 9001L, "minecraft:desert")
        report("fourthSeed", 4321L, "minecraft:desert")
    }
}
