package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the warm and cold ocean ruins. */
class OceanRuinGeneratorOracle {
    /** Every template the generator can ask for, in the order that indexes the stand-ins. */
    private val names =
        (1..8).map { "underwater_ruin/warm_$it" } +
            (1..8).map { "underwater_ruin/brick_$it" } +
            (1..8).map { "underwater_ruin/cracked_$it" } +
            (1..8).map { "underwater_ruin/mossy_$it" } +
            listOf(1, 2, 3, 8).map { "underwater_ruin/big_brick_$it" } +
            listOf(1, 2, 3, 8).map { "underwater_ruin/big_cracked_$it" } +
            listOf(1, 2, 3, 8).map { "underwater_ruin/big_mossy_$it" } +
            (4..7).map { "underwater_ruin/big_warm_$it" }

    /**
     * A stand-in ruin sized by which name it answers to, so the draw that picks one is observable.
     * Every block rots, which is what makes the three integrities tell each other apart.
     */
    private fun ruin(index: Int): StructureTemplate {
        // The last sixteen names are the large ruins, and a large one is wide enough to reach the
        // third ring of chunks the walk visits.
        val large = index >= 32
        val sizeX = (if (large) 14 else 5) + index % 5
        val sizeY = 3 + index % 4
        val sizeZ = (if (large) 15 else 6) + index % 6
        val palette =
            listOf("minecraft:stone_bricks", "minecraft:mossy_stone_bricks", "minecraft:air")
                .map { name -> NbtCompound().also { it.putString("Name", name) } }
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    // A hollow shell, so the rot draw runs on the walls and the inside is air.
                    val wall = x == 0 || z == 0 || y == 0 || x == sizeX - 1 || z == sizeZ - 1
                    entry.putInt(
                        "state",
                        if (!wall) {
                            2
                        } else if ((x + y + z) % 3 == 0) {
                            1
                        } else {
                            0
                        },
                    )
                    blocks += entry
                }
            }
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(sizeY), NbtInt(sizeZ)))
        nbt["palette"] = NbtList(NbtType.COMPOUND, palette)
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        return StructureTemplate().also { it.load(nbt) }
    }

    private val templates: (String) -> StructureTemplate? = { name ->
        names.indexOf(name).takeIf { it >= 0 }?.let { ruin(it) }
    }

    private fun block(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * A pitted sea bed. The wide pits sink a whole footprint; the one-block strip cuts across one,
     * so how many of its columns count as hollow lands near the width it is compared against.
     */
    private fun inPit(
        x: Int,
        z: Int,
    ): Boolean = (x.mod(37) < 12 && z.mod(41) < 12) || (x.mod(23) < 9 && z.mod(29) == 5)

    private fun bedTop(
        x: Int,
        z: Int,
    ): Int {
        val floor = 45 + (x * 2 + z).mod(4)
        return if (inPit(x, z)) floor - 7 else floor
    }

    private fun terrainAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val floor = bedTop(x, z)
        return when {
            y <= floor -> Blocks.STONE.stateId
            // A pit is capped with ice, which the walk down has to treat as open just like water.
            inPit(x, z) && y <= floor + 3 -> block("packed_ice")
            y <= 62 -> block("water")
            else -> Blocks.AIR.stateId
        }
    }

    private fun seaChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 20..70) {
                    chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z))
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 20..70) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != terrainAt(chunk.chunkX * 16 + x, y, chunk.chunkZ * 16 + z)) written++
                }
            }
        }
        return checksum to written
    }

    private fun report(
        name: String,
        seed: Long,
        cold: Boolean,
        warm: Boolean,
    ) {
        val generator =
            OceanRuinStructureGenerator(
                // Only the centre column of a chunk answers anything but plains, so where the
                // variant probe stands is observable too.
                biomeAt = { x, y, z ->
                    when {
                        x.mod(16) != 8 || z.mod(16) != 8 -> "minecraft:plains"
                        y != bedTop(x, z) -> "minecraft:plains"
                        cold -> "minecraft:cold_ocean"
                        warm -> "minecraft:warm_ocean"
                        else -> "minecraft:plains"
                    }
                },
                coldBiomes = setOf("minecraft:cold_ocean", "minecraft:frozen_ocean"),
                warmBiomes = setOf("minecraft:warm_ocean", "minecraft:lukewarm_ocean"),
                templateOf = templates,
                oceanFloorAt = { x, z -> bedTop(x, z) + 1 },
            )
        val centre = OceanRuinStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var starts = 0
        var pieces = 0
        for (index in 0 until 64) {
            val chunk = seaChunk(centre.first + index % 8 - 4, centre.second + index / 8 - 4)
            chunk.withExternalDecorationBlockState({ x, y, z -> terrainAt(x, y, z) }) {
                generator.generateInto(chunk, seed)
            }
            val (chunkChecksum, chunkWritten) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            starts += chunk.structures.starts().size
            pieces += chunk.structures.starts().sumOf { it.pieces.size }
            for (start in chunk.structures.starts()) {
                for (box in start.pieces + start.fullBounds) {
                    for (bound in listOf(box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ)) {
                        checksum = (checksum xor bound.toLong()) * 1099511628211L
                    }
                }
            }
        }
        println("ORACLE $name written=$written starts=$starts pieces=$pieces checksum=$checksum")
    }

    @Test
    fun dump() {
        report("cold", 1234L, cold = true, warm = false)
        report("warm", 1234L, cold = false, warm = true)
        report("both", 1234L, cold = true, warm = true)
        report("neither", 1234L, cold = false, warm = false)
        // A seed whose ruin drew large and then drew its cluster of small ones around it.
        report("cluster", 7L, cold = true, warm = true)
        report("clusterWarm", 7L, cold = false, warm = true)
        report("otherSeed", 3L, cold = true, warm = true)
    }
}
