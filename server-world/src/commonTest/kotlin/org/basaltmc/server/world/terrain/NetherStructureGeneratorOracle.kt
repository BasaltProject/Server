package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.structure.EmptyPoolElement
import org.basaltmc.server.world.structure.LegacySinglePoolElement
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructureProcessorList
import org.basaltmc.server.world.structure.StructureProjection
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.server.world.structure.TemplateLoader
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the Nether: fortresses, bastions, fossils and portals. */
class NetherStructureGeneratorOracle {
    private val orientations = listOf("north_up", "south_up", "west_up", "east_up", "up_north", "down_north")

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * A stand-in piece. Fossils are bone, ruined portals are obsidian with a chest and a slab the
     * ager can rot, and bastion pieces carry a jigsaw. Nothing here comes from a Mojang file.
     */
    private fun piece(
        index: Int,
        kind: String,
    ): StructureTemplate {
        val sizeX = 4 + index % 3
        val sizeY = 4 + index % 4
        val sizeZ = 4 + index % 2
        val palette =
            listOf(
                "minecraft:bone_block",
                "minecraft:obsidian",
                "minecraft:chest",
                "minecraft:stone_brick_slab",
                "minecraft:gold_block",
                "minecraft:air",
                "minecraft:jigsaw",
            ).map { name ->
                NbtCompound().also {
                    it.putString("Name", name)
                    if (name == "minecraft:jigsaw") {
                        val properties = NbtCompound()
                        properties["orientation"] = NbtString("east_up")
                        it["Properties"] = properties
                    }
                }
            }
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    val chestHere = kind == "portal" && x == 1 && y == 1 && z == 1
                    // The portal gets one too: its second pass writes the final state even where
                    // the processors refused the block.
                    val jigsawHere = (kind == "bastion" || kind == "portal") && x == sizeX - 1 && y == 1 && z == 1
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt(
                        "state",
                        when {
                            chestHere -> 2
                            jigsawHere -> 6
                            // A sparse rib cage, so the dried ghast has air to land in.
                            kind == "fossil" -> if ((x + y + z) % 2 == 0) 0 else 5
                            kind == "portal" && (x + y + z) % 4 == 0 -> 3
                            kind == "portal" && (x + y + z) % 4 == 1 -> 4
                            kind == "portal" -> 1
                            else -> 1
                        },
                    )
                    if (chestHere) {
                        val data = NbtCompound()
                        data["id"] = NbtString("minecraft:chest")
                        data["LootTable"] = NbtString("minecraft:chests/ruined_portal")
                        entry["nbt"] = data
                    }
                    if (jigsawHere) {
                        val data = NbtCompound()
                        data["name"] = NbtString("side")
                        data["pool"] = NbtString("minecraft:bastion/wings")
                        data["target"] = NbtString("wing")
                        data["final_state"] =
                            NbtString(
                                if (kind == "portal") "minecraft:netherrack" else "minecraft:air",
                            )
                        data["joint"] = NbtString("rollable")
                        data["placement_priority"] = NbtInt(0)
                        data["selection_priority"] = NbtInt(0)
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
        buildMap {
            for (index in 1..14) put("minecraft:nether_fossils/fossil_$index", piece(index, "fossil"))
            for (index in 1..10) put("minecraft:ruined_portal/portal_$index", piece(index, "portal"))
            for (index in 1..3) put("minecraft:ruined_portal/giant_portal_$index", piece(index + 10, "portal"))
            put("minecraft:bastion/start", piece(1, "bastion"))
            put("minecraft:bastion/wing", piece(2, "bastion"))
        }

    private val loader: TemplateLoader = { templates[it.toString()] }

    private val pools: Map<String, StructureTemplatePool> =
        mapOf(
            "minecraft:empty" to StructureTemplatePool("minecraft:empty", listOf(EmptyPoolElement to 1)),
            "minecraft:bastion/starts" to
                StructureTemplatePool(
                    "minecraft:empty",
                    listOf(
                        LegacySinglePoolElement(
                            "minecraft:bastion/start",
                            StructureProcessorList.EMPTY,
                            StructureProjection.RIGID,
                        ) to 1,
                    ),
                ),
            "minecraft:bastion/wings" to
                StructureTemplatePool(
                    "minecraft:empty",
                    listOf(
                        LegacySinglePoolElement(
                            "minecraft:bastion/wing",
                            StructureProcessorList.EMPTY,
                            StructureProjection.RIGID,
                        ) to 1,
                    ),
                ),
        )

    /** The generator keeps its grids to itself, so the sweep rebuilds them to centre on. */
    private val complexGrid = StructurePlacementGrid(spacing = 27, separation = 4, salt = 30084232)
    private val portalGrid = StructurePlacementGrid(spacing = 40, separation = 15, salt = 34222645)

    /**
     * A rough Nether floor that meets the lava sea, cheap enough that the portal replay finishes.
     * Low on purpose: a portal has to be able to land in lava and in a carved cave.
     */
    private fun router(
        seed: Long,
        temperature: Double,
        vegetation: Double,
    ): NetherNoiseRouter {
        val noise = NormalNoise.create(XoroshiroRandom(seed), -6, doubleArrayOf(1.0, 1.0))
        val density: DensityFunction =
            DensityFunctions.add(
                DensityFunctions.yClampedGradient(0, 64, 1.0, -1.0),
                DensityFunctions.mul(DensityFunctions.constant(3.0), DensityFunctions.noise(noise, 0.4, 0.4)),
            )
        return NetherNoiseRouter(
            DensityFunctions.constant(temperature),
            DensityFunctions.constant(vegetation),
            density,
        )
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 0 until 128) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.AIR.stateId) written++
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
        for (tick in chunk.scheduledFluidTicks()) {
            checksum = (checksum xor tick.localX.toLong() xor (tick.y.toLong() shl 8)) * 1099511628211L
        }
        return Triple(checksum, written, chunk.structures.starts().size)
    }

    private fun report(
        name: String,
        seed: Long,
        temperature: Double,
        vegetation: Double,
        around: String,
    ) {
        val centre =
            when (around) {
                "complex" -> complexGrid.candidateChunk(seed, 0, 0)
                "portal" -> portalGrid.candidateChunk(seed, 0, 0)
                else -> 0 to 0
            }
        val aroundChunkX = centre.first
        val aroundChunkZ = centre.second
        val netherRouter = router(seed, temperature, vegetation)
        val biomeSource = NetherBiomeSource(netherRouter.temperature, netherRouter.vegetation)
        val generator = NetherStructureGenerator(netherRouter, biomeSource, loader) { pools[it] }
        var checksum = -3750763034362895579L
        var written = 0
        var starts = 0
        var blockEntities = 0
        for (index in 0 until 25) {
            val chunkX = aroundChunkX + index % 5 - 2
            val chunkZ = aroundChunkZ + index / 5 - 2
            val chunk = Chunk(chunkX, chunkZ, ChunkHeight.NETHER)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkStarts) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            starts += chunkStarts
            blockEntities += chunk.blockEntities().size
        }
        println(
            "ORACLE $name written=$written starts=$starts blockEntities=$blockEntities " +
                "biome=${biomeSource.biomeAt(0, 64, 0).id.path} checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        // The four bastion biomes and the one that only fortresses take, chosen by the two noises.
        // The fossils sit on their own two-chunk grid, so the origin already covers them.
        report("fossils", 1234L, -0.6, -0.6, "origin")
        // The fortress and the bastion share one cell, and which of the two it holds is a draw.
        report("complexWastes", 1234L, 0.0, 0.0, "complex")
        report("complexDeltas", 1234L, -0.6, 0.0, "complex")
        report("complexOther", 77L, 0.0, 0.0, "complex")
        report("complexThird", 9001L, 0.0, 0.0, "complex")
        report("complexFourth", 5L, 0.0, 0.0, "complex")
        report("complexFifth", 13L, 0.0, 0.0, "complex")
        report("complexSixth", 21L, 0.0, 0.0, "complex")
        // Basalt deltas takes a fortress but never a bastion, which is the one biome gate here.
        report("complexFourthDeltas", 5L, -0.6, 0.0, "complex")
        report("complexFifthDeltas", 13L, -0.6, 0.0, "complex")
        report("complexSixthDeltas", 21L, -0.6, 0.0, "complex")
        // The ruined portal replays one global placement from its own grid.
        report("portal", 1234L, 0.0, 0.0, "portal")
        report("portalOther", 77L, 0.4, 0.0, "portal")
    }
}
