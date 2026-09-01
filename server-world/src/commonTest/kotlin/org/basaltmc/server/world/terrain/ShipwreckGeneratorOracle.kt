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

/** The frozen Kotlin as the oracle for the two shipwrecks. */
class ShipwreckGeneratorOracle {
    /** The ocean list is a superset of the beached one, so it indexes every distinct hull. */
    private val hulls =
        listOf(
            "shipwreck/with_mast",
            "shipwreck/upsidedown_full",
            "shipwreck/upsidedown_fronthalf",
            "shipwreck/upsidedown_backhalf",
            "shipwreck/sideways_full",
            "shipwreck/sideways_fronthalf",
            "shipwreck/sideways_backhalf",
            "shipwreck/rightsideup_full",
            "shipwreck/rightsideup_fronthalf",
            "shipwreck/rightsideup_backhalf",
            "shipwreck/with_mast_degraded",
            "shipwreck/upsidedown_full_degraded",
            "shipwreck/upsidedown_fronthalf_degraded",
            "shipwreck/upsidedown_backhalf_degraded",
            "shipwreck/sideways_full_degraded",
            "shipwreck/sideways_fronthalf_degraded",
            "shipwreck/sideways_backhalf_degraded",
            "shipwreck/rightsideup_full_degraded",
            "shipwreck/rightsideup_fronthalf_degraded",
            "shipwreck/rightsideup_backhalf_degraded",
        )

    /**
     * A stand-in hull whose size and planking depend on which name it answers to, so the draw that
     * picks one of the twenty is observable. The three markers sit over a chest, a barrel and a
     * plank, which is what tells the container test from a marker that simply fires.
     */
    private fun hull(index: Int): StructureTemplate {
        val sizeX = 20 + index % 3
        val sizeY = 6 + index % 4
        val sizeZ = 28 + index % 5
        val palette =
            listOf(
                "minecraft:oak_planks",
                "minecraft:structure_block",
                "minecraft:chest",
                "minecraft:barrel",
                "minecraft:air",
            ).map { name -> NbtCompound().also { it.putString("Name", name) } }
        val markers =
            mapOf(
                Triple(2, 2, 5) to "map_chest",
                Triple(6, 2, 9) to "treasure_chest",
                Triple(10, 2, 13) to "supply_chest",
            )
        val containers = mapOf(Triple(2, 1, 5) to 2, Triple(6, 1, 9) to 3)
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    val here = Triple(x, y, z)
                    val marker = markers[here]
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt(
                        "state",
                        when {
                            marker != null -> 1
                            containers[here] != null -> containers.getValue(here)
                            // A hollow hull, so most of it is air the processor drops.
                            y > 0 && x in 1 until sizeX - 1 && z in 1 until sizeZ - 1 -> 4
                            else -> 0
                        },
                    )
                    if (marker != null) {
                        val data = NbtCompound()
                        data["mode"] = NbtString("DATA")
                        data["metadata"] = NbtString(marker)
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

    private val templates: (String) -> StructureTemplate? = { name ->
        hulls.indexOf(name).takeIf { it >= 0 }?.let { hull(it) }
    }

    /** Stone up to the beach, so the air a hull carries carves it instead of landing on air. */
    private fun stoneChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 34..66) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 34..78) {
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
            checksum = (
                checksum xor
                    entity.data
                        .getString("LootTable", "")
                        .hashCode()
                        .toLong()
            ) * 1099511628211L
        }
        return Triple(checksum, written, chunk.blockEntities().size)
    }

    private fun report(
        name: String,
        seed: Long,
        ocean: Boolean,
        beach: Boolean,
    ) {
        // Two slopes, so the mean of the footprint differs from its lowest column and from the
        // column the wreck started on.
        val oceanFloorAt = { x: Int, z: Int -> 45 + (x * 3 + z).mod(5) }
        val surfaceHeightAt = { x: Int, z: Int -> 62 + (x + z * 2).mod(7) }
        val generator =
            ShipwreckStructureGenerator(
                // The biome is only the one block under each heightmap, so which heightmap the
                // variant reads, and the block below it, are both observable.
                // Only the centre column of a chunk answers anything but plains, so where the
                // variant probe stands is observable too.
                biomeAt = { x, y, z ->
                    when {
                        x.mod(16) != 8 || z.mod(16) != 8 -> "minecraft:plains"
                        y == oceanFloorAt(x, z) - 1 -> if (ocean) "minecraft:ocean" else "minecraft:plains"
                        y == surfaceHeightAt(x, z) - 1 -> if (beach) "minecraft:beach" else "minecraft:plains"
                        else -> "minecraft:plains"
                    }
                },
                oceanBiomes = setOf("minecraft:ocean", "minecraft:deep_ocean"),
                beachBiomes = setOf("minecraft:beach", "minecraft:snowy_beach"),
                templateOf = templates,
                surfaceHeightAt = surfaceHeightAt,
                oceanFloorAt = oceanFloorAt,
            )
        val centre = ShipwreckStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var starts = 0
        for (index in 0 until 64) {
            val chunk = stoneChunk(centre.first + index % 8 - 4, centre.second + index / 8 - 4)
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
        // Seed 1234 draws the beached one first, so the ocean-only case is the rejection path.
        report("sunk", 1234L, ocean = true, beach = false)
        report("beached", 1234L, ocean = false, beach = true)
        report("both", 1234L, ocean = true, beach = true)
        // Seed 2 draws the sunk one first, which is the other half of both orders.
        report("bothSunk", 2L, ocean = true, beach = true)
        report("beachedAfterReject", 2L, ocean = false, beach = true)
        report("neither", 1234L, ocean = false, beach = false)
        report("otherSeed", 77L, ocean = true, beach = true)
    }
}
