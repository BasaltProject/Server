package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for where mansions stand and what they lay. */
class WoodlandMansionGeneratorOracle {
    /**
     * A stand-in mansion room: planks with one structure block carrying a marker. No Mojang asset
     * goes near the repository, and what is under test is the placing, not the architecture.
     */
    private fun roomTemplate(metadata: String): StructureTemplate {
        val palette =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().also { it.putString("Name", "minecraft:oak_planks") },
                    NbtCompound().also { it.putString("Name", "minecraft:structure_block") },
                ),
            )
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until 7) {
            for (y in 0 until 6) {
                for (z in 0 until 7) {
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt("state", if (x == 3 && y == 1 && z == 3) 1 else 0)
                    if (x == 3 && y == 1 && z == 3) {
                        val data = NbtCompound()
                        data["mode"] = NbtString("DATA")
                        data["metadata"] = NbtString(metadata)
                        entry["nbt"] = data
                    }
                    blocks += entry
                }
            }
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(7), NbtInt(6), NbtInt(7)))
        nbt["palette"] = palette
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        return StructureTemplate().also { it.load(nbt) }
    }

    /** Every template answers, and the marker depends on the name so the four kinds all appear. */
    private val templates: (String) -> StructureTemplate? = { name ->
        when {
            name.endsWith("entrance") -> roomTemplate("ChestWest")
            name.contains("1x1_a") -> roomTemplate("Mage")
            name.contains("1x2_a") -> roomTemplate("Warrior")
            name.contains("2x2_a") -> roomTemplate("Group of Allays")
            // A chest on the common pieces, so the facing turns with the house; the corridors keep
            // the marker that only leaves air behind.
            name.contains("corridor") || name.contains("carpet") -> roomTemplate("")
            else -> roomTemplate("ChestEast")
        }
    }

    /** Stone under a lake: the foundation has to walk down through the water to reach the rock. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -8..59) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in 60..69) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -8..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId &&
                        id != Blocks.WATER.stateId &&
                        id != Blocks.AIR.stateId
                    ) {
                        written++
                    }
                }
            }
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
        // A gentle slope, so the four corners of the 5x5 probe box disagree and which offsets it
        // uses is observable.
        val surfaceAt = { x: Int, z: Int -> surface + (x + z).mod(8) }
        val generator =
            WoodlandMansionStructureGenerator(
                biomeAt = { _, _, _ -> biome },
                mansionBiomes = setOf("minecraft:dark_forest"),
                templateOf = templates,
                surfaceHeightAt = surfaceAt,
            )
        val centre = WoodlandMansionStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var entities = 0
        var starts = 0
        for (index in 0 until chunks) {
            // Offset so the window reaches seven chunks out on one side, which is the mansion's own
            // reach: a shorter reach would drop those chunks.
            val chunkX = centre.first + index % 10 - 7
            val chunkZ = centre.second + index / 10 - 7
            val chunk = groundChunk(chunkX, chunkZ)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkBlockEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            blockEntities += chunkBlockEntities
            entities += chunk.generatedEntities().size
            starts += chunk.structures.starts().size
        }
        println(
            "ORACLE $name written=$written blockEntities=$blockEntities entities=$entities starts=$starts " +
                "checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("darkForest", 555L, "minecraft:dark_forest", 75, 100)
        // A biome outside the tag refuses the mansion, and the rotation roll still happens.
        report("plains", 555L, "minecraft:plains", 75, 100)
        // Ground just under y 60 refuses it, after the rotation was drawn. Fifty and not forty, so a
        // lowered floor would let this one through.
        report("lowGround", 555L, "minecraft:dark_forest", 50, 100)
        report("otherSeed", 31L, "minecraft:dark_forest", 75, 100)
    }
}
