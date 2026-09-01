package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.structure.EmptyPoolElement
import org.basaltmc.server.world.structure.LegacySinglePoolElement
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructureProcessorList
import org.basaltmc.server.world.structure.StructureProjection
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.server.world.structure.TemplateLoader
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the pillager outpost. */
class PillagerOutpostGeneratorOracle {
    private data class Connector(
        val x: Int,
        val y: Int,
        val z: Int,
        val orientation: String,
        val name: String,
        val pool: String,
        val target: String,
    )

    private val orientations = listOf("north_up", "south_up", "west_up", "east_up", "up_north", "down_north")

    /**
     * A synthetic outpost piece: a box of stone with one jigsaw block per connector. Nothing here
     * comes from a Mojang structure file; what is under test is the generator, not the buildings.
     */
    private fun piece(
        sizeX: Int,
        sizeY: Int,
        sizeZ: Int,
        connectors: List<Connector>,
    ): StructureTemplate {
        val entries =
            mutableListOf(
                NbtCompound().also { it.putString("Name", "minecraft:cobblestone") },
                NbtCompound().also { it.putString("Name", "minecraft:chest") },
            )
        val chestState = 1
        for (orientation in orientations) {
            val properties = NbtCompound()
            properties["orientation"] = NbtString(orientation)
            entries +=
                NbtCompound().also {
                    it.putString("Name", "minecraft:jigsaw")
                    it["Properties"] = properties
                }
        }
        val blocks = mutableListOf<NbtCompound>()
        val taken = connectors.map { Triple(it.x, it.y, it.z) }.toSet()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    if (Triple(x, y, z) in taken) continue
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    // One chest per piece, whose loot seed comes off the feature stream: without it
                    // nothing a piece places depends on that stream at all.
                    val chestHere = x == 1 && y == 0 && z == 1
                    entry.putInt("state", if (chestHere) chestState else 0)
                    if (chestHere) {
                        val data = NbtCompound()
                        data["id"] = NbtString("minecraft:chest")
                        data["LootTable"] = NbtString("minecraft:chests/pillager_outpost")
                        entry["nbt"] = data
                    }
                    blocks += entry
                }
            }
        }
        for (connector in connectors) {
            val entry = NbtCompound()
            entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(connector.x), NbtInt(connector.y), NbtInt(connector.z)))
            entry.putInt("state", 2 + orientations.indexOf(connector.orientation))
            val data = NbtCompound()
            data["name"] = NbtString(connector.name)
            data["pool"] = NbtString(connector.pool)
            data["target"] = NbtString(connector.target)
            data["final_state"] = NbtString("minecraft:air")
            data["joint"] = NbtString("rollable")
            data["placement_priority"] = NbtInt(0)
            data["selection_priority"] = NbtInt(0)
            entry["nbt"] = data
            blocks += entry
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(sizeY), NbtInt(sizeZ)))
        nbt["palette"] = NbtList(NbtType.COMPOUND, entries)
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        return StructureTemplate().also { it.load(nbt) }
    }

    private val templates: Map<String, StructureTemplate> =
        mapOf(
            // Four arms, so the outpost crosses its chunk's border on both axes.
            "minecraft:pillager_outpost/base_plate" to
                piece(
                    9,
                    1,
                    9,
                    listOf(
                        Connector(4, 0, 4, "up_north", "plate", "minecraft:pillager_outpost/towers", "tower"),
                        Connector(0, 0, 4, "west_up", "west", "minecraft:pillager_outpost/features", "feature"),
                        Connector(8, 0, 4, "east_up", "east", "minecraft:pillager_outpost/features", "feature"),
                        Connector(4, 0, 0, "north_up", "north", "minecraft:pillager_outpost/features", "feature"),
                        Connector(4, 0, 8, "south_up", "south", "minecraft:pillager_outpost/features", "feature"),
                    ),
                ),
            "minecraft:pillager_outpost/watchtower" to
                piece(
                    7,
                    11,
                    7,
                    listOf(
                        Connector(3, 0, 3, "down_north", "tower", "minecraft:empty", "empty"),
                        Connector(6, 4, 3, "east_up", "arm", "minecraft:pillager_outpost/features", "feature"),
                        // Points back into its own box, which is the only shape the expansion hack
                        // grows a piece for.
                        Connector(3, 5, 3, "east_up", "inner", "minecraft:pillager_outpost/features", "feature"),
                    ),
                ),
            // Each tent hangs another one off itself, so how deep the assembly is allowed to go and
            // how far it may reach both decide where it stops.
            "minecraft:pillager_outpost/tent" to
                piece(
                    9,
                    13,
                    9,
                    listOf(
                        Connector(0, 1, 4, "west_up", "feature", "minecraft:empty", "empty"),
                        Connector(8, 1, 4, "east_up", "tent_out", "minecraft:pillager_outpost/posts", "post"),
                    ),
                ),
            "minecraft:pillager_outpost/cage" to
                piece(5, 4, 5, listOf(Connector(0, 1, 2, "west_up", "feature", "minecraft:empty", "empty"))),
            // Seven links of seven blocks reach further than half the allowed radius but not all
            // of it, so how deep the assembly may go and how far it may reach both bite.
            "minecraft:pillager_outpost/post" to
                piece(
                    7,
                    3,
                    7,
                    listOf(
                        Connector(0, 1, 3, "west_up", "post", "minecraft:empty", "empty"),
                        Connector(6, 1, 3, "east_up", "post_out", "minecraft:pillager_outpost/posts", "post"),
                    ),
                ),
        )

    private val loader: TemplateLoader = { name -> templates[name] }

    private fun single(
        location: String,
        projection: StructureProjection = StructureProjection.RIGID,
        integrity: Float = 1.0f,
    ): org.basaltmc.server.world.structure.StructurePoolElement =
        LegacySinglePoolElement(
            location,
            if (integrity < 1.0f) {
                StructureProcessorList(
                    emptyList(),
                    org.basaltmc.server.world.structure
                        .BlockRot(integrity, null),
                )
            } else {
                StructureProcessorList.EMPTY
            },
            projection,
        )

    private val pools: Map<String, StructureTemplatePool> =
        mapOf(
            "minecraft:empty" to StructureTemplatePool("minecraft:empty", listOf(EmptyPoolElement to 1)),
            "minecraft:pillager_outpost/base_plates" to
                StructureTemplatePool(
                    "minecraft:empty",
                    listOf(single("minecraft:pillager_outpost/base_plate") to 1),
                ),
            "minecraft:pillager_outpost/towers" to
                StructureTemplatePool(
                    "minecraft:empty",
                    listOf(single("minecraft:pillager_outpost/watchtower") to 1),
                ),
            "minecraft:pillager_outpost/posts" to
                StructureTemplatePool(
                    "minecraft:empty",
                    // Every link settles on the terrain, so a height is read wherever the chain
                    // got to and not only where it started.
                    listOf(single("minecraft:pillager_outpost/post", StructureProjection.TERRAIN_MATCHING) to 1),
                ),
            "minecraft:pillager_outpost/features" to
                StructureTemplatePool(
                    "minecraft:empty",
                    listOf(
                        single("minecraft:pillager_outpost/tent") to 3,
                        // The one piece that is neither rigid nor whole: the beard skips it and the
                        // rot makes the placement draw from the feature stream.
                        single(
                            "minecraft:pillager_outpost/cage",
                            StructureProjection.TERRAIN_MATCHING,
                            integrity = 0.7f,
                        ) to 1,
                    ),
                ),
        )

    /** A rolling surface around y 60, so which column a height is read from is observable. */
    private fun router(seed: Long): OverworldNoiseRouter {
        val zero = DensityFunctions.zero()
        val noise = NormalNoise.create(XoroshiroRandom(seed), -6, doubleArrayOf(1.0, 1.0))
        val density =
            DensityFunctions.add(
                DensityFunctions.yClampedGradient(50, 78, 1.0, -1.0),
                DensityFunctions.mul(DensityFunctions.constant(2.0), DensityFunctions.noise(noise, 1.0, 0.0)),
            )
        return OverworldNoiseRouter(
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            density,
            zero,
            zero,
            zero,
        )
    }

    private val grid = StructurePlacementGrid(spacing = 32, separation = 8, salt = 165745296, frequency = 0.2f)

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 40..110) {
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
        }
        for (start in chunk.structures.starts()) {
            for (box in start.pieces + start.fullBounds) {
                for (bound in listOf(box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ)) {
                    checksum = (checksum xor bound.toLong()) * 1099511628211L
                }
            }
            checksum = (checksum xor start.chunkX.toLong() xor (start.chunkZ.toLong() shl 8)) * 1099511628211L
        }
        return checksum to written
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
    ) {
        val generator =
            PillagerOutpostStructureGenerator(
                router = router(seed),
                grid = grid,
                // Only the biome right at the surface answers, so the height the probe reads is
                // observable through the biome test as well.
                // Only the centre column of a chunk answers at all, and only right at the surface,
                // so where the probe stands and what height it reads are both observable.
                biomeAt = { x, y, z ->
                    if (x.mod(16) == 8 && z.mod(16) == 8 && y == surfaceOf(seed, x, z)) biome else "minecraft:ocean"
                },
                outpostBiomes = setOf("minecraft:plains", "minecraft:taiga"),
                poolLookup = { pools[it] },
                templateLoader = loader,
            )
        // Centred on the outpost the search finds, so the sweep always covers a real one when the
        // biome allows any at all.
        val found = generator.locate(seed, 0, 0, maxCells = 6)
        val centre = found ?: grid.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var starts = 0
        for (index in 0 until 36) {
            val chunkX = centre.first + index % 6 - 3
            val chunkZ = centre.second + index / 6 - 3
            val chunk = Chunk(chunkX, chunkZ)
            generator.generateInto(chunk, chunkX, chunkZ, seed)
            val (chunkChecksum, chunkWritten) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            starts += chunk.structures.starts().size
        }
        // The beard of the chunk the outpost starts in, sampled over its own cell grid.
        val beard = generator.beardified(DensityFunctions.zero(), centre.first, centre.second, seed)
        for (y in 48..80 step 4) {
            for (x in 0 until 16 step 4) {
                for (z in 0 until 16 step 4) {
                    val value = beard.compute(FunctionContext(centre.first * 16 + x, y, centre.second * 16 + z))
                    checksum = (checksum xor value.toRawBits()) * 1099511628211L
                }
            }
        }
        val (cached, cachedPieces) = generator.assemblyStats()
        println(
            "ORACLE $name written=$written starts=$starts checksum=$checksum " +
                "located=${found?.first},${found?.second} cached=$cached cachedPieces=$cachedPieces",
        )
    }

    /** The same surface walk the generator does, so the biome can be pinned to it. */
    private fun surfaceOf(
        seed: Long,
        x: Int,
        z: Int,
    ): Int {
        val sampler =
            DensityChunkSampler(router(seed).finalDensity, x shr 4, z shr 4, -64, 384, 4, 8)
        var y = 319
        while (y >= -64 && sampler.sample(x and 15, y, z and 15) <= 0.0) y--
        return maxOf(y + 1, 63)
    }

    @Test
    fun dump() {
        report("plains", 1234L, "minecraft:plains")
        report("ocean", 1234L, "minecraft:ocean")
        report("otherSeed", 77L, "minecraft:taiga")
        report("thirdSeed", 9001L, "minecraft:plains")
    }
}
