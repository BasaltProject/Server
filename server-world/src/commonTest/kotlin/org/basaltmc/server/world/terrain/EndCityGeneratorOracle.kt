package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.TemplateLoader
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the End city. */
class EndCityGeneratorOracle {
    /** Every template the recursion can ask for, in the order that sizes the stand-ins. */
    private val names =
        listOf(
            "base_floor",
            "base_roof",
            "second_floor_1",
            "second_floor_2",
            "second_roof",
            "third_floor_1",
            "third_floor_2",
            "third_roof",
            "tower_base",
            "tower_piece",
            "tower_top",
            "bridge_end",
            "bridge_piece",
            "bridge_steep_stairs",
            "bridge_gentle_stairs",
            "ship",
            "fat_tower_base",
            "fat_tower_middle",
            "fat_tower_top",
        )

    /**
     * A stand-in city piece: purpur with a chest under a `Chest` marker and the two entity markers
     * the jar reads. Nothing here comes from a Mojang structure file.
     */
    private fun piece(index: Int): StructureTemplate {
        val sizeX = 9 + index % 4
        val sizeY = 5 + index % 5
        val sizeZ = 9 + index % 3
        val palette =
            listOf("minecraft:purpur_block", "minecraft:chest", "minecraft:structure_block", "minecraft:air")
                .map { name -> NbtCompound().also { it.putString("Name", name) } }
        val markers =
            mapOf(
                Triple(2, 2, 2) to "Chest",
                Triple(4, 2, 4) to "Sentry",
                Triple(6, 2, 6) to "Elytra",
            )
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    val here = Triple(x, y, z)
                    val marker = markers[here]
                    val chestHere = x == 2 && y == 1 && z == 2
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt(
                        "state",
                        when {
                            marker != null -> 2
                            chestHere -> 1
                            // A hollow shell, so `overwrite` decides whether the air is written.
                            y > 0 && x in 1 until sizeX - 1 && z in 1 until sizeZ - 1 -> 3
                            else -> 0
                        },
                    )
                    if (marker != null) {
                        val data = NbtCompound()
                        data["mode"] = NbtString("DATA")
                        data["metadata"] = NbtString(marker)
                        entry["nbt"] = data
                    }
                    if (chestHere) {
                        val data = NbtCompound()
                        data["id"] = NbtString("minecraft:chest")
                        entry["nbt"] = data
                    }
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

    private val templates: Map<String, StructureTemplate> =
        names.withIndex().associate { (index, name) -> "minecraft:end_city/$name" to piece(index) }

    private val loader: TemplateLoader = { templates[it.toString()] }

    private fun base3d(): DensityFunction = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

    private fun router(): EndNoiseRouter = NoiseRouterData.end(base3d(), 0x5DEECE66DL)

    private fun endChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 40..70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 40..140) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val base = if (y in 40..70) Blocks.STONE.stateId else Blocks.AIR.stateId
                    if (id != base) written++
                }
            }
        }
        for (entity in chunk.blockEntities()) {
            checksum = (checksum xor entity.data.getLong("LootTableSeed", 0L)) * 1099511628211L
            checksum = (
                checksum xor
                    entity.data
                        .getString("LootTable", "")
                        .hashCode()
                        .toLong()
            ) * 1099511628211L
        }
        for (entity in chunk.generatedEntities()) {
            checksum = (
                checksum xor
                    entity.type.path
                        .hashCode()
                        .toLong()
            ) * 1099511628211L
            for (value in listOf(entity.x, entity.y, entity.z)) {
                checksum = (checksum xor value.toRawBits()) * 1099511628211L
            }
            checksum = (checksum xor entity.yaw.toRawBits().toLong()) * 1099511628211L
        }
        return Triple(checksum, written, chunk.generatedEntities().size)
    }

    private fun report(
        name: String,
        seed: Long,
        aroundChunkX: Int,
        aroundChunkZ: Int,
        surface: Int,
    ) {
        val end = router()
        val generator =
            EndCityStructureGenerator(
                router = end,
                biomeSource = TheEndBiomeSource(end.erosion),
                templateLoader = loader,
                // A sloped shelf, so which of the four corners the start takes is observable.
                surfaceHeightOverride = { x, z -> surface + (x + z).mod(4) },
            )
        val centre = EndCityStructureGenerator.END_CITY_GRID.candidateChunk(seed, aroundChunkX, aroundChunkZ)
        var checksum = -3750763034362895579L
        var written = 0
        var entities = 0
        var pieces = 0
        // Twelve chunks across, so a candidate can sit further from a chunk than half the reference
        // range: with a narrower sweep, halving that range would change nothing.
        for (index in 0 until 144) {
            val chunk = endChunk(centre.first + index % 12 - 6, centre.second + index / 12 - 6)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            entities += chunkEntities
        }
        pieces = generator.cityAt(seed, centre.first, centre.second).size
        println("ORACLE $name written=$written entities=$entities pieces=$pieces checksum=$checksum")
    }

    @Test
    fun dump() {
        report("highlands", 1234L, 80, 60, 70)
        report("midlands", 1234L, 60, 60, 70)
        // Too low for a city to start at all, whatever the biome says.
        report("low", 1234L, 80, 60, 40)
        // The central island is `the_end`, which no city takes.
        report("centre", 1234L, 0, 0, 70)
        report("otherSeed", 1234L, 120, 120, 70)
        report("thirdSeed", 1234L, 120, 140, 70)
        // Small end islands, which no city takes either.
        report("islands", 1234L, 3200, 3200, 70)
    }
}
