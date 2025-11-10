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
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the three deep jigsaw structures. */
class JigsawStructuresOracle {
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
     * A synthetic piece: a box of stone with a chest and one jigsaw block per connector. Nothing here
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
                NbtCompound().also { it.putString("Name", "minecraft:deepslate_bricks") },
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
                    // One chest per piece, whose loot seed comes off the feature stream: without it
                    // nothing a piece places depends on that stream at all.
                    val chestHere = x == 1 && y == 0 && z == 1
                    entry.putInt("state", if (chestHere) 1 else 0)
                    if (chestHere) {
                        val data = NbtCompound()
                        data["id"] = NbtString("minecraft:chest")
                        data["LootTable"] = NbtString("minecraft:chests/deep")
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

    /**
     * One family of pieces per structure, all the same shape so the three reports differ only in the
     * settings under test. The anchor is the jigsaw an assembly may be told to start on.
     */
    private fun family(
        prefix: String,
        anchor: String,
    ): Map<String, StructureTemplate> =
        mapOf(
            "$prefix/start" to
                piece(
                    9,
                    5,
                    9,
                    listOf(
                        Connector(0, 1, 4, "west_up", anchor, "$prefix/rooms", "room"),
                        Connector(8, 1, 4, "east_up", "east", "$prefix/rooms", "room"),
                        Connector(4, 1, 0, "north_up", "north", "$prefix/rooms", "room"),
                        Connector(4, 1, 8, "south_up", "south", "$prefix/rooms", "room"),
                    ),
                ),
            // Seven links of seven blocks reach further than half the allowed radius but not all of
            // it, so how deep the assembly may go and how far it may reach both bite.
            "$prefix/room" to
                piece(
                    7,
                    5,
                    7,
                    listOf(
                        Connector(0, 1, 3, "west_up", "room", "minecraft:empty", "empty"),
                        Connector(6, 1, 3, "east_up", "room_out", "$prefix/rooms", "room"),
                    ),
                ),
            "$prefix/cell" to
                piece(5, 4, 5, listOf(Connector(0, 1, 2, "west_up", "room", "minecraft:empty", "empty"))),
        )

    private fun single(
        location: String,
        projection: StructureProjection = StructureProjection.RIGID,
        integrity: Float = 1.0f,
    ): StructurePoolElement =
        LegacySinglePoolElement(
            location,
            if (integrity <
                1.0f
            ) {
                StructureProcessorList(emptyList(), BlockRot(integrity, null))
            } else {
                StructureProcessorList.EMPTY
            },
            projection,
        )

    private fun poolsOf(
        prefix: String,
        startPool: String,
    ): Map<String, StructureTemplatePool> =
        mapOf(
            "minecraft:empty" to StructureTemplatePool("minecraft:empty", listOf(EmptyPoolElement to 1)),
            startPool to StructureTemplatePool("minecraft:empty", listOf(single("$prefix/start") to 1)),
            "$prefix/rooms" to
                StructureTemplatePool(
                    "minecraft:empty",
                    listOf(
                        single("$prefix/room") to 3,
                        // The one piece that is neither rigid nor whole: it settles on the chunk's own
                        // heightmap, the beard skips it, and the rot makes the placement draw from
                        // the feature stream.
                        single("$prefix/cell", StructureProjection.TERRAIN_MATCHING, integrity = 0.7f) to 1,
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

    /** The same surface walk the generators do, so a biome can be pinned to it. */
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

    private fun stoneChunk(
        chunkX: Int,
        chunkZ: Int,
        lowY: Int,
        terrainTop: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in lowY..terrainTop) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(
        chunk: Chunk,
        lowY: Int,
        highY: Int,
    ): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in lowY..highY) {
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

    private fun sweep(
        name: String,
        seed: Long,
        centre: Pair<Int, Int>,
        lowY: Int,
        highY: Int,
        terrainTop: Int,
        beard: (Int, Int) -> org.basaltmc.server.world.densityfunction.DensityFunction,
        generate: (Chunk, Int, Int) -> Unit,
        stats: () -> Pair<Int, Long>,
        located: String,
    ) {
        var checksum = -3750763034362895579L
        var written = 0
        var starts = 0
        for (index in 0 until 36) {
            val chunkX = centre.first + index % 6 - 3
            val chunkZ = centre.second + index / 6 - 3
            val chunk = stoneChunk(chunkX, chunkZ, lowY, terrainTop)
            generate(chunk, chunkX, chunkZ)
            val (chunkChecksum, chunkWritten) = fold(chunk, lowY, highY)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            starts += chunk.structures.starts().size
        }
        val beardified = beard(centre.first, centre.second)
        for (y in lowY..highY step 4) {
            for (x in 0 until 16 step 4) {
                for (z in 0 until 16 step 4) {
                    val value = beardified.compute(FunctionContext(centre.first * 16 + x, y, centre.second * 16 + z))
                    checksum = (checksum xor value.toRawBits()) * 1099511628211L
                }
            }
        }
        println(
            "ORACLE $name written=$written starts=$starts checksum=$checksum " +
                "located=$located cached=${stats().first} cachedPieces=${stats().second}",
        )
    }

    private fun reportTrailRuins(
        name: String,
        seed: Long,
        biome: String,
    ) {
        val grid = StructurePlacementGrid(spacing = 34, separation = 8, salt = 83469867)
        val templates = family("minecraft:trail_ruins", "anchor")
        val pools = poolsOf("minecraft:trail_ruins", "minecraft:trail_ruins/tower")
        val loader: TemplateLoader = { templates[it] }
        val generator =
            TrailRuinsStructureGenerator(
                router = router(seed),
                grid = grid,
                biomeAt = { x, y, z ->
                    if (x.mod(16) == 8 && z.mod(16) == 8 && y == surfaceOf(seed, x, z)) biome else "minecraft:ocean"
                },
                ruinBiomes = setOf("minecraft:jungle", "minecraft:taiga"),
                poolLookup = { pools[it] },
                templateLoader = loader,
            )
        val found = generator.locate(seed, 0, 0, maxCells = 6)
        val centre = found ?: grid.candidateChunk(seed, 0, 0)
        sweep(
            name,
            seed,
            centre,
            40,
            110,
            62,
            { x, z -> generator.beardified(DensityFunctions.zero(), x, z, seed) },
            { chunk, x, z -> generator.generateInto(chunk, x, z, seed) },
            { generator.assemblyStats() },
            "${found?.first},${found?.second}",
        )
    }

    private fun reportAncientCity(
        name: String,
        seed: Long,
        biome: String,
    ) {
        val grid = StructurePlacementGrid(spacing = 24, separation = 8, salt = 20083232)
        val templates = family("minecraft:ancient_city", "minecraft:city_anchor")
        val pools = poolsOf("minecraft:ancient_city", "minecraft:ancient_city/city_center")
        val loader: TemplateLoader = { templates[it] }
        val generator =
            AncientCityStructureGenerator(
                biomeAt = { x, y, z -> if (x.mod(16) == 8 && z.mod(16) == 8 && y == -27) biome else "minecraft:ocean" },
                cityBiomes = setOf("minecraft:deep_dark"),
                poolLookup = { pools[it] },
                templateLoader = loader,
            )
        val centre = grid.candidateChunk(seed, 0, 0)
        sweep(
            name,
            seed,
            centre,
            -50,
            0,
            -10,
            { x, z -> generator.beardified(DensityFunctions.zero(), x, z, seed) },
            { chunk, x, z -> generator.generateInto(chunk, x, z, seed) },
            { generator.assemblyStats() },
            "none",
        )
    }

    private fun reportTrialChamber(
        name: String,
        seed: Long,
        biome: String,
    ) {
        val grid = StructurePlacementGrid(spacing = 34, separation = 12, salt = 94251327)
        val templates = family("minecraft:trial_chambers", "anchor")
        val pools = poolsOf("minecraft:trial_chambers", "minecraft:trial_chambers/chamber/end")
        val loader: TemplateLoader = { templates[it] }
        val generator =
            TrialChamberStructureGenerator(
                biomeAt = { x, y, z -> if (x.mod(16) == 8 && z.mod(16) == 8 && y == -40) biome else "minecraft:ocean" },
                poolLookup = { pools[it] },
                templateLoader = loader,
                chamberBiomes = setOf("minecraft:dripstone_caves", "minecraft:lush_caves"),
            )
        val centre = grid.candidateChunk(seed, 0, 0)
        sweep(
            name,
            seed,
            centre,
            -60,
            -5,
            -15,
            { x, z -> generator.beardified(DensityFunctions.zero(), x, z, seed) },
            { chunk, x, z -> generator.generateInto(chunk, x, z, seed) },
            { generator.assemblyStats() },
            "none",
        )
    }

    @Test
    fun dump() {
        reportTrailRuins("ruins", 1234L, "minecraft:jungle")
        reportTrailRuins("ruinsElsewhere", 77L, "minecraft:taiga")
        reportTrailRuins("noRuins", 1234L, "minecraft:ocean")
        reportAncientCity("city", 1234L, "minecraft:deep_dark")
        reportAncientCity("cityElsewhere", 77L, "minecraft:deep_dark")
        reportAncientCity("noCity", 1234L, "minecraft:ocean")
        reportTrialChamber("chamber", 1234L, "minecraft:dripstone_caves")
        reportTrialChamber("chamberElsewhere", 77L, "minecraft:lush_caves")
        reportTrialChamber("noChamber", 1234L, "minecraft:ocean")
    }
}
