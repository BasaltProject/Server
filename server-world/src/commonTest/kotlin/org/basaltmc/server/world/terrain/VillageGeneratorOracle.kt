package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.structure.BlockRot
import org.basaltmc.server.world.structure.EmptyPoolElement
import org.basaltmc.server.world.structure.LegacySinglePoolElement
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePoolElement
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

/** The frozen Kotlin as the oracle for the five villages. */
class VillageGeneratorOracle {
    private val kinds = listOf("plains", "desert", "savanna", "snowy", "taiga")

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
     * A synthetic village piece: a box of planks with a chest and one jigsaw block per connector.
     * Nothing here comes from a Mojang structure file; what is under test is the generator.
     */
    private fun piece(
        sizeX: Int,
        sizeY: Int,
        sizeZ: Int,
        wall: String,
        connectors: List<Connector>,
    ): StructureTemplate {
        val entries =
            mutableListOf(
                NbtCompound().also { it.putString("Name", wall) },
                NbtCompound().also { it.putString("Name", "minecraft:chest") },
            )
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
                    val chestHere = x == 1 && y == 0 && z == 1
                    entry.putInt("state", if (chestHere) 1 else 0)
                    if (chestHere) {
                        val data = NbtCompound()
                        data["id"] = NbtString("minecraft:chest")
                        data["LootTable"] = NbtString("minecraft:chests/village")
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

    /** A different wall block per kind, so which variant was drawn shows in the blocks. */
    private val walls =
        mapOf(
            "plains" to "minecraft:oak_planks",
            "desert" to "minecraft:cut_sandstone",
            "savanna" to "minecraft:acacia_planks",
            "snowy" to "minecraft:spruce_planks",
            "taiga" to "minecraft:cobblestone",
        )

    private val templates: Map<String, StructureTemplate> =
        buildMap {
            for (kind in kinds) {
                val wall = walls.getValue(kind)
                put(
                    "minecraft:village/$kind/town_center",
                    piece(
                        9,
                        5,
                        9,
                        wall,
                        listOf(
                            Connector(0, 1, 4, "west_up", "west", "minecraft:village/$kind/houses", "house"),
                            Connector(8, 1, 4, "east_up", "east", "minecraft:village/$kind/houses", "house"),
                            Connector(4, 1, 0, "north_up", "north", "minecraft:village/$kind/houses", "house"),
                            Connector(4, 1, 8, "south_up", "south", "minecraft:village/$kind/houses", "house"),
                            // Points back into its own box, which is the only shape the expansion
                            // hack grows a piece for.
                            Connector(4, 1, 4, "east_up", "inner", "minecraft:village/$kind/houses", "house"),
                        ),
                    ),
                )
                put(
                    "minecraft:village/$kind/house",
                    piece(
                        7,
                        5,
                        7,
                        wall,
                        listOf(
                            Connector(0, 1, 3, "west_up", "house", "minecraft:empty", "empty"),
                            Connector(6, 1, 3, "east_up", "house_out", "minecraft:village/$kind/houses", "house"),
                        ),
                    ),
                )
                put(
                    "minecraft:village/$kind/hut",
                    piece(
                        5,
                        9,
                        5,
                        wall,
                        listOf(Connector(0, 1, 2, "west_up", "house", "minecraft:empty", "empty")),
                    ),
                )
            }
        }

    private val loader: TemplateLoader = { templates[it] }

    private fun single(
        location: String,
        projection: StructureProjection = StructureProjection.RIGID,
        integrity: Float = 1.0f,
    ): StructurePoolElement =
        LegacySinglePoolElement(
            location,
            if (integrity < 1.0f) {
                StructureProcessorList(emptyList(), BlockRot(integrity, null))
            } else {
                StructureProcessorList.EMPTY
            },
            projection,
        )

    private val pools: Map<String, StructureTemplatePool> =
        buildMap {
            put("minecraft:empty", StructureTemplatePool("minecraft:empty", listOf(EmptyPoolElement to 1)))
            for (kind in kinds) {
                put(
                    "minecraft:village/$kind/town_centers",
                    StructureTemplatePool(
                        "minecraft:empty",
                        listOf(single("minecraft:village/$kind/town_center") to 1),
                    ),
                )
                put(
                    "minecraft:village/$kind/houses",
                    StructureTemplatePool(
                        "minecraft:empty",
                        listOf(
                            single("minecraft:village/$kind/house") to 3,
                            single("minecraft:village/$kind/hut", StructureProjection.TERRAIN_MATCHING, 0.7f) to 1,
                        ),
                    ),
                )
            }
        }

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

    private fun surfaceOf(
        seed: Long,
        x: Int,
        z: Int,
    ): Int {
        val sampler = DensityChunkSampler(router(seed).finalDensity, x shr 4, z shr 4, -64, 384, 4, 8)
        var y = 319
        while (y >= -64 && sampler.sample(x and 15, y, z and 15) <= 0.0) y--
        return maxOf(y + 1, 63)
    }

    private val grid = StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312)

    /** A grid tight enough that several villages fall in one sweep, so the write order shows. */
    private val denseGrid = StructurePlacementGrid(spacing = 6, separation = 2, salt = 10387312)

    private fun stoneChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 40..62) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 40..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.AIR.stateId && id != Blocks.STONE.stateId) written++
                }
            }
        }
        for (entity in chunk.blockEntities()) {
            checksum = (checksum xor entity.data.getLong("LootTableSeed", 0L)) * 1099511628211L
        }
        return checksum to written
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
        mixWith: String? = null,
        dense: Boolean = false,
    ) {
        val grid = if (dense) denseGrid else this.grid
        val generator =
            VillageStructureGenerator(
                router = router(seed),
                grid = grid,
                // Alternating chunk columns get the other biome when one is given, so two variants
                // stand near each other and the order they are written in is observable.
                biomeAt = { x, y, z ->
                    val here = if (mixWith != null && (x shr 4).mod(2) == 1) mixWith else biome
                    if (x.mod(16) == 8 && z.mod(16) == 8 && y == surfaceOf(seed, x, z)) here else "minecraft:ocean"
                },
                maxDepth = 6,
                maxDistanceFromCenter = 80,
                poolLookup = { pools[it] },
                templateLoader = loader,
            )
        val found = generator.locate(seed, 0, 0, structure = null, maxCells = 6)
        val centre = if (found != null) found.chunkX to found.chunkZ else grid.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        for (index in 0 until 36) {
            val chunkX = centre.first + index % 6 - 3
            val chunkZ = centre.second + index / 6 - 3
            val chunk = stoneChunk(chunkX, chunkZ)
            generator.generateInto(chunk, chunkX, chunkZ, seed)
            val (chunkChecksum, chunkWritten) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
        }
        val density = generator.finalDensityForChunk(centre.first, centre.second, seed)
        for (y in 48..80 step 4) {
            for (x in 0 until 16 step 4) {
                for (z in 0 until 16 step 4) {
                    val value = density.compute(FunctionContext(centre.first * 16 + x, y, centre.second * 16 + z))
                    checksum = (checksum xor value.toRawBits()) * 1099511628211L
                }
            }
        }
        // The two command paths, which skip the grid and the biome entirely.
        val placed = generator.assembleAt(Identifier.vanilla("village_taiga"), centre.first, centre.second, seed)
        val pooled =
            generator.assemblePool("minecraft:village/desert/houses", centre.first, centre.second, 3, seed)
        val placedChunk = stoneChunk(centre.first, centre.second)
        if (placed != null) generator.placeInto(placedChunk, placed, seed)
        generator.placeInto(placedChunk, pooled, seed)
        val (commandChecksum, commandWritten) = fold(placedChunk)
        checksum = (checksum xor commandChecksum) * 1099511628211L
        val wanted = generator.locate(seed, 0, 0, Identifier.vanilla("village_snowy"), maxCells = 6)
        val (cached, cachedPieces) = generator.assemblyStats()
        println(
            "ORACLE $name written=$written commandWritten=$commandWritten placed=${placed?.size} " +
                "pooled=${pooled.size} located=${found?.structure?.path},${found?.chunkX},${found?.chunkZ} " +
                "snowy=${wanted?.chunkX},${wanted?.chunkZ} cached=$cached cachedPieces=$cachedPieces " +
                "checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("plains", 1234L, "minecraft:plains")
        report("desert", 1234L, "minecraft:desert")
        report("savanna", 1234L, "minecraft:savanna")
        report("snowy", 1234L, "minecraft:snowy_plains")
        report("taiga", 1234L, "minecraft:taiga")
        report("meadow", 1234L, "minecraft:meadow")
        report("none", 1234L, "minecraft:ocean")
        report("otherSeed", 77L, "minecraft:plains")
        // A tight grid puts several villages in one sweep, two variants among them, which is the
        // only shape in which the order they are written in can be seen.
        report("dense", 1234L, "minecraft:plains", mixWith = "minecraft:desert", dense = true)
        report("denseOther", 77L, "minecraft:taiga", mixWith = "minecraft:savanna", dense = true)
    }
}
