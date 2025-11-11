package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the jungle temple. */
class JungleTempleGeneratorOracle {
    /** A sloped jungle floor, so the mean of the footprint is not the column it started on. */
    private fun surfaceAt(
        x: Int,
        z: Int,
    ): Int = 68 + (x * 3 + z * 2).mod(6)

    /**
     * Caves the heightmap knows nothing about, so what a temple writes below its own floor lands in
     * open air rather than replacing more stone.
     */
    private fun hollow(
        x: Int,
        z: Int,
    ): Boolean = x.mod(31) < 7 && z.mod(29) < 7

    private fun terrainAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val surface = surfaceAt(x, z)
        if (y >= surface) return Blocks.AIR.stateId
        if (hollow(x, z) && y > surface - 18) return Blocks.AIR.stateId
        return Blocks.STONE.stateId
    }

    private fun jungleChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 30..95) {
                    chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z))
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 30..95) {
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
            JungleTempleStructureGenerator(
                // Only the centre column of a chunk answers, and only one block under its surface,
                // so where the probe stands and what height it reads are both observable.
                biomeAt = { x, y, z ->
                    if (x.mod(16) == 8 && z.mod(16) == 8 && y == surfaceAt(x, z) - 1) biome else "minecraft:ocean"
                },
                templeBiomes = setOf("minecraft:jungle", "minecraft:bamboo_jungle"),
                surfaceHeightAt = { x, z -> surfaceAt(x, z) },
            )
        val centre = JungleTempleStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var starts = 0
        for (index in 0 until 36) {
            val chunk = jungleChunk(centre.first + index % 6 - 3, centre.second + index / 6 - 3)
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
        report("jungle", 1234L, "minecraft:jungle")
        report("bamboo", 1234L, "minecraft:bamboo_jungle")
        report("plains", 1234L, "minecraft:plains")
        report("otherSeed", 77L, "minecraft:jungle")
        report("thirdSeed", 9001L, "minecraft:jungle")
        report("fourthSeed", 4321L, "minecraft:jungle")
    }
}
