package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.structure.LegacySinglePoolElement
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructureProcessorList
import org.basaltmc.server.world.structure.StructureProjection
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class NetherStructureGeneratorTest {
    @Test
    fun structureReferencesFollowVanillaFastutilIterationOrder() {
        val insertionOrder = listOf(0 to 0, 1 to 2, -3 to 4, 5 to -6, 9 to 9, -8 to -8)

        assertEquals(
            listOf(0 to 0, 9 to 9, -3 to 4, 1 to 2, -8 to -8, 5 to -6),
            vanillaStructureReferenceChunkOrder(insertionOrder),
        )
    }

    @Test
    fun bastionsUseWeightedNetherComplexGridAndExcludeBasaltDeltas() {
        val template = oneBlockTemplate("blackstone")
        val startPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:bastion_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to 1,
                ),
            )
        val router = NetherNoiseRouter(ConstantDensity(0.4), ConstantDensity(0.0), LayerDensity(64))
        val crimsonForest = NetherBiomeSource(router.temperature, router.vegetation)
        val grid = StructurePlacementGrid(spacing = 27, separation = 4, salt = 30084232)
        val generated =
            (0L..256L).firstNotNullOf { seed ->
                val (chunkX, chunkZ) = grid.candidateChunk(seed, 0, 0)
                val chunk = Chunk(chunkX, chunkZ, ChunkHeight.NETHER)
                generator(router, crimsonForest, template, startPool).generateInto(chunk, seed)
                if (count(chunk, "blackstone") > 0) Triple(seed, chunkX to chunkZ, chunk) else null
            }
        val basaltDeltas = NetherBiomeSource(ConstantDensity(-0.5), ConstantDensity(0.0))
        val filtered = Chunk(generated.second.first, generated.second.second, ChunkHeight.NETHER)
        generator(router, basaltDeltas, template, startPool).generateInto(filtered, generated.first)

        assertTrue(count(generated.third, "blackstone") > 0)
        assertEquals(0, count(filtered, "blackstone"))
    }

    @Test
    fun bastionContainersReceiveTheSurfaceStructureDecorationLootSeed() {
        val template = chestTemplate()
        val startPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:bastion_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                        ignoreAir = false,
                    ) to 1,
                ),
            )
        val router = NetherNoiseRouter(ConstantDensity(0.4), ConstantDensity(0.0), LayerDensity(64))
        val crimsonForest = NetherBiomeSource(router.temperature, router.vegetation)
        val grid = StructurePlacementGrid(spacing = 27, separation = 4, salt = 30084232)
        val generated =
            (0L..256L).firstNotNullOf { seed ->
                val (chunkX, chunkZ) = grid.candidateChunk(seed, 0, 0)
                val chunk = Chunk(chunkX, chunkZ, ChunkHeight.NETHER)
                generator(router, crimsonForest, template, startPool).generateInto(chunk, seed)
                chunk.blockEntities().singleOrNull()?.let { Triple(seed, chunkX to chunkZ, it) }
            }
        val oracle = WorldgenRandom(XoroshiroRandom(generated.first))
        val decorationSeed =
            oracle.setDecorationSeed(
                generated.first,
                generated.second.first * 16,
                generated.second.second * 16,
            )
        oracle.setFeatureSeed(decorationSeed, 0, 4)

        assertEquals(Identifier.vanilla("chest"), generated.third.type)
        assertEquals("test:chests/bastion", generated.third.data.getString("LootTable"))
        assertEquals(oracle.nextLong(), generated.third.data.getLong("LootTableSeed"))
    }

    @Test
    fun netherFossilsUsePlacementGridBaseColumnAndSoulSandValleyFilter() {
        val template = oneBlockTemplate("bone_block")
        val router = NetherNoiseRouter(ConstantDensity(0.0), ConstantDensity(-0.5), LayerDensity(64))
        val soulSandValley = NetherBiomeSource(router.temperature, router.vegetation)
        val grid = StructurePlacementGrid(spacing = 2, separation = 1, salt = 14357921)
        val generated =
            (0L..256L).firstNotNullOf { seed ->
                val (chunkX, chunkZ) = grid.candidateChunk(seed, 0, 0)
                val chunk = Chunk(chunkX, chunkZ, ChunkHeight.NETHER)
                NetherStructureGenerator(
                    router,
                    soulSandValley,
                    templateLoader = { if (it.startsWith("minecraft:nether_fossils/")) template else null },
                ).generateInto(chunk, seed)
                if (count(chunk, "bone_block") > 0) Triple(seed, chunkX to chunkZ, chunk) else null
            }
        val netherWastes = NetherBiomeSource(ConstantDensity(0.0), ConstantDensity(0.0))
        val filtered = Chunk(generated.second.first, generated.second.second, ChunkHeight.NETHER)
        NetherStructureGenerator(
            router,
            netherWastes,
            templateLoader = { if (it.startsWith("minecraft:nether_fossils/")) template else null },
        ).generateInto(filtered, generated.first)

        assertTrue(count(generated.third, "bone_block") > 0)
        assertEquals(0, count(filtered, "bone_block"))
    }

    @Test
    fun netherFossilBeardThinIsAppliedBeforeTerrainCellInterpolation() {
        val template = emptyTemplate()
        val router = NetherNoiseRouter(ConstantDensity(0.0), ConstantDensity(-0.5), LayerDensity(64, 0.1))
        val soulSandValley = NetherBiomeSource(router.temperature, router.vegetation)
        val grid = StructurePlacementGrid(spacing = 2, separation = 1, salt = 14357921)
        val generated =
            (0L..256L).firstNotNullOf { seed ->
                val (chunkX, chunkZ) = grid.candidateChunk(seed, 0, 0)
                val generator =
                    NetherStructureGenerator(
                        router,
                        soulSandValley,
                        templateLoader = { if (it.startsWith("minecraft:nether_fossils/")) template else null },
                    )
                val density = generator.finalDensityForChunk(chunkX, chunkZ, seed)
                val point =
                    (0..12 step 4).firstNotNullOfOrNull { localX ->
                        (0..12 step 4)
                            .firstOrNull { localZ ->
                                density.compute(FunctionContext(chunkX * 16 + localX, 64, chunkZ * 16 + localZ)) < 0.0
                            }?.let { localZ -> localX to localZ }
                    } ?: return@firstNotNullOf null
                val untouched = Chunk(chunkX, chunkZ, ChunkHeight.NETHER)
                generator.generateInto(untouched, seed)
                if (!Blocks.isAir(untouched.getBlockStateId(point.first, 64, point.second))) {
                    return@firstNotNullOf null
                }
                val raw = DimensionTerrainGenerator.nether(router, soulSandValley, chunkX, chunkZ, seed = seed)
                if (raw.getBlockStateId(point.first, 64, point.second) != Blocks.NETHERRACK.stateId) {
                    return@firstNotNullOf null
                }
                FossilBeardCase(seed, chunkX, chunkZ, point.first, point.second, generator)
            }

        val adapted =
            DimensionTerrainGenerator.nether(
                router,
                soulSandValley,
                generated.chunkX,
                generated.chunkZ,
                seed = generated.seed,
                structureGenerator = generated.generator,
            )

        assertTrue(
            Blocks.isAir(adapted.getBlockStateId(generated.localX, 64, generated.localZ)),
            "beard_thin must enter the coarse density grid before terrain fill",
        )
    }

    @Test
    fun netherFossilsRunBeforeFortressesInTheUndergroundDecorationStep() {
        val template = layeredTemplate(32, 25, 4, "bone_block")
        val router = NetherNoiseRouter(ConstantDensity(0.0), ConstantDensity(-0.5), LayerDensity(64))
        val soulSandValley = NetherBiomeSource(router.temperature, router.vegetation)
        val generator =
            NetherStructureGenerator(
                router,
                soulSandValley,
                templateLoader = { if (it.startsWith("minecraft:nether_fossils/")) template else null },
            )
        val chunk = Chunk(2, 8, ChunkHeight.NETHER)

        generator.generateInto(chunk, 5L)

        assertEquals(stateId("nether_bricks"), chunk.getBlockStateId(5, 71, 10))
    }

    @Test
    fun fortressesUseTheWeightedNetherComplexGridInEveryNetherBiome() {
        val template = oneBlockTemplate("blackstone")
        val startPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:bastion_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to 1,
                ),
            )
        val router = NetherNoiseRouter(ConstantDensity(0.4), ConstantDensity(0.0), LayerDensity(64))
        val basaltDeltas = NetherBiomeSource(ConstantDensity(-0.5), ConstantDensity(0.0))
        val grid = StructurePlacementGrid(spacing = 27, separation = 4, salt = 30084232)
        val generated =
            (0L..256L).firstNotNullOf { seed ->
                val (chunkX, chunkZ) = grid.candidateChunk(seed, 0, 0)
                val chunk = Chunk(chunkX, chunkZ, ChunkHeight.NETHER)
                generator(router, basaltDeltas, template, startPool).generateInto(chunk, seed)
                if (count(chunk, "nether_bricks") > 0) Triple(seed, chunkX to chunkZ, chunk) else null
            }

        assertEquals("basalt_deltas", basaltDeltas.biomeAt(0, 64, 0).id.path)
        assertTrue(count(generated.third, "nether_bricks") > 0)
        assertEquals(0, count(generated.third, "blackstone"))
    }

    @Test
    fun fortressStartsPlaceIntoReferencedNeighborChunks() {
        val template = oneBlockTemplate("blackstone")
        val startPool =
            StructureTemplatePool(
                "minecraft:empty",
                listOf(
                    LegacySinglePoolElement(
                        "test:bastion_piece",
                        StructureProcessorList.EMPTY,
                        StructureProjection.RIGID,
                    ) to 1,
                ),
            )
        val router = NetherNoiseRouter(ConstantDensity(0.4), ConstantDensity(0.0), LayerDensity(64))
        val biomeSource = NetherBiomeSource(ConstantDensity(0.0), ConstantDensity(0.0))
        val grid = StructurePlacementGrid(spacing = 27, separation = 4, salt = 30084232)
        val selected =
            (0L..256L).firstNotNullOf { seed ->
                val candidate = grid.candidateChunk(seed, 0, 0)
                val random = WorldgenRandom(LegacyRandom(0L))
                random.setLargeFeatureSeed(seed, candidate.first, candidate.second)
                if (random.nextInt(5) < 2) seed to candidate else null
            }
        val neighbor = Chunk(selected.second.first + 1, selected.second.second, ChunkHeight.NETHER)

        generator(router, biomeSource, template, startPool).generateInto(neighbor, selected.first)

        assertTrue(count(neighbor, "nether_bricks") > 0)
        assertEquals(0, count(neighbor, "blackstone"))
    }

    private fun oneBlockTemplate(path: String): StructureTemplate {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(1), NbtInt(1), NbtInt(1)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(NbtCompound().apply { putString("Name", "minecraft:$path") }),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().apply {
                        set("pos", NbtList(NbtType.INT, listOf(NbtInt(0), NbtInt(0), NbtInt(0))))
                        putInt("state", 0)
                    },
                ),
            )
        return StructureTemplate().apply { load(root) }
    }

    private fun chestTemplate(): StructureTemplate {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(1), NbtInt(1), NbtInt(1)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(NbtCompound().apply { putString("Name", "minecraft:chest") }),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().apply {
                        set("pos", NbtList(NbtType.INT, listOf(NbtInt(0), NbtInt(0), NbtInt(0))))
                        putInt("state", 0)
                        this["nbt"] =
                            NbtCompound().apply {
                                putString("id", "minecraft:chest")
                                putString("LootTable", "test:chests/bastion")
                            }
                    },
                ),
            )
        return StructureTemplate().apply { load(root) }
    }

    private fun emptyTemplate(): StructureTemplate {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(1), NbtInt(1), NbtInt(1)))
        root["palette"] = NbtList(NbtType.COMPOUND, emptyList())
        root["blocks"] = NbtList(NbtType.COMPOUND, emptyList())
        return StructureTemplate().apply { load(root) }
    }

    private fun layeredTemplate(
        size: Int,
        height: Int,
        layerSpacing: Int,
        path: String,
    ): StructureTemplate {
        val root = NbtCompound()
        root["size"] = NbtList(NbtType.INT, listOf(NbtInt(size), NbtInt(height), NbtInt(size)))
        root["palette"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(NbtCompound().apply { putString("Name", "minecraft:$path") }),
            )
        root["blocks"] =
            NbtList(
                NbtType.COMPOUND,
                buildList {
                    for (y in 0 until height step layerSpacing) {
                        for (x in 0 until size) {
                            for (z in 0 until size) {
                                add(
                                    NbtCompound().apply {
                                        set("pos", NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z))))
                                        putInt("state", 0)
                                    },
                                )
                            }
                        }
                    }
                },
            )
        return StructureTemplate().apply { load(root) }
    }

    private fun generator(
        router: NetherNoiseRouter,
        biomeSource: NetherBiomeSource,
        template: StructureTemplate,
        startPool: StructureTemplatePool,
    ): NetherStructureGenerator =
        NetherStructureGenerator(
            router,
            biomeSource,
            templateLoader = { if (it == "test:bastion_piece") template else null },
            poolLookup = { if (it == "minecraft:bastion/starts") startPool else null },
        )

    private fun count(
        chunk: Chunk,
        path: String,
    ): Int {
        val stateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
                ?: error("missing vanilla block minecraft:$path")
        var count = 0
        for (y in 0..127) {
            for (z in 0 until 16) {
                for (x in 0 until 16) if (chunk.getBlockStateId(x, y, z) == stateId) count++
            }
        }
        return count
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }

    private class LayerDensity(
        private val topY: Int,
        private val solidValue: Double = 1.0,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = if (context.blockY <= topY) solidValue else -1.0

        override fun minValue(): Double = -1.0

        override fun maxValue(): Double = solidValue
    }

    private data class FossilBeardCase(
        val seed: Long,
        val chunkX: Int,
        val chunkZ: Int,
        val localX: Int,
        val localZ: Int,
        val generator: NetherStructureGenerator,
    )
}
