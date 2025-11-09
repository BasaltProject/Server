package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the igloo and the shaft under it. */
class IglooGeneratorOracle {
    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * A stand-in igloo piece: snow with a chest and a structure block over it, so the marker has a
     * chest below to fill. Nothing here comes from a Mojang structure file.
     */
    private fun piece(
        sizeX: Int,
        sizeY: Int,
        sizeZ: Int,
        withChest: Boolean,
        withLadder: Boolean,
    ): StructureTemplate {
        val entries =
            mutableListOf(
                NbtCompound().also { it.putString("Name", "minecraft:snow_block") },
                NbtCompound().also { it.putString("Name", "minecraft:structure_block") },
                NbtCompound().also { it.putString("Name", "minecraft:chest") },
                NbtCompound().also { it.putString("Name", "minecraft:ladder") },
                NbtCompound().also { it.putString("Name", "minecraft:air") },
            )
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    val chestHere = withChest && x == 1 && y == 0 && z == 1
                    val markerHere = withChest && x == 1 && y == 1 && z == 1
                    val ladderHere = withLadder && x == 1 && y == 0 && z == 1
                    // An open floor under the dome, so the snow that closes a shaftless trapdoor
                    // lands on something other than more snow.
                    val floorHere = !withChest && !withLadder && y == 0 && x in 2..4 && z in 3..6
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt(
                        "state",
                        when {
                            markerHere -> 1
                            chestHere -> 2
                            ladderHere -> 3
                            floorHere -> 4
                            else -> 0
                        },
                    )
                    if (markerHere) {
                        val data = NbtCompound()
                        data["mode"] = NbtString("DATA")
                        data["metadata"] = NbtString("chest")
                        entry["nbt"] = data
                    }
                    blocks += entry
                }
            }
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(sizeY), NbtInt(sizeZ)))
        nbt["palette"] = NbtList(NbtType.COMPOUND, entries)
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        return StructureTemplate().also { it.load(nbt) }
    }

    private val templates: (String) -> StructureTemplate? = { name ->
        when (name) {
            "igloo/top" -> piece(7, 6, 8, withChest = false, withLadder = false)
            "igloo/middle" -> piece(3, 4, 3, withChest = false, withLadder = true)
            "igloo/bottom" -> piece(7, 8, 9, withChest = true, withLadder = false)
            else -> null
        }
    }

    private fun snowChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -16..70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -16..100) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId && id != Blocks.AIR.stateId) written++
                }
            }
        }
        for (entity in chunk.blockEntities()) {
            checksum = (checksum xor entity.data.getLong("LootTableSeed", 0L)) * 1099511628211L
        }
        return Triple(checksum, written, chunk.blockEntities().size)
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
        surface: Int,
        chunks: Int,
    ) {
        val generator =
            IglooStructureGenerator(
                biomeAt = { _, _, _ -> biome },
                iglooBiomes = setOf("minecraft:snowy_plains", "minecraft:snowy_slopes"),
                templateOf = templates,
                // A slope, so which column the entrance settles on is observable.
                surfaceHeightAt = { x, z -> surface + (x + z).mod(4) },
            )
        // Centred on the candidate the grid picks, which is 32 chunks wide.
        val centre = IglooStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var starts = 0
        for (index in 0 until chunks) {
            val chunkX = centre.first + index % 6 - 3
            val chunkZ = centre.second + index / 6 - 3
            val chunk = snowChunk(chunkX, chunkZ)
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
        report("snowy", 1234L, "minecraft:snowy_plains", 71, 36)
        // A biome outside the tag leaves every chunk alone.
        report("plains", 1234L, "minecraft:plains", 71, 36)
        // A seed whose coin flip drew a basement, so the shaft, the laboratory and its chest run.
        report("basement", 3L, "minecraft:snowy_plains", 71, 36)
        // A shallower shaft from a different draw of the eight-deep range.
        report("shallowBasement", 5L, "minecraft:snowy_plains", 71, 36)
        report("otherSeed", 8L, "minecraft:snowy_plains", 71, 36)
    }
}
