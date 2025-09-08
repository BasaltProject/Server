package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.noise.NoiseParametersJson
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.terrain.AncientCityStructureGenerator
import org.basaltmc.server.world.terrain.DimensionTerrainGenerator
import org.basaltmc.server.world.terrain.EndCityStructureGenerator
import org.basaltmc.server.world.terrain.MineshaftStructureGenerator
import org.basaltmc.server.world.terrain.NeighbourTerrainCache
import org.basaltmc.server.world.terrain.NetherBiomeSource
import org.basaltmc.server.world.terrain.NetherStructureGenerator
import org.basaltmc.server.world.terrain.NoiseColumnHeights
import org.basaltmc.server.world.terrain.NoiseRouterData
import org.basaltmc.server.world.terrain.PillagerOutpostStructureGenerator
import org.basaltmc.server.world.terrain.RealTerrainGenerator
import org.basaltmc.server.world.terrain.RuinedPortalStructureGenerator
import org.basaltmc.server.world.terrain.SwampHutStructureGenerator
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import org.basaltmc.server.world.terrain.TrailRuinsStructureGenerator
import org.basaltmc.server.world.terrain.TrialChamberStructureGenerator
import org.basaltmc.server.world.terrain.VillageStructureGenerator
import org.basaltmc.wire.common.Identifier

/** Builds one correctly seeded normal-terrain generator for each loaded world. */
public class VanillaWorldGeneratorFactory(
    private val serverJar: ServerJarAssets,
) {
    private val spawnEquipment = SpawnEquipmentFinalizer(serverJar)

    // Tracked so the timings report can put a size on the worldgen caches, which live inside the
    // per-world closures and would otherwise be unreachable from any diagnostic.
    private val loaders = mutableListOf<VillageDataLoader>()
    private val villages = mutableListOf<VillageStructureGenerator>()

    /** What the structure caches hold across every world built by this factory. */
    public data class WorldgenCacheStats(
        val templates: Int,
        val templateBlocks: Long,
        val assemblies: Int,
        val assemblyPieces: Long,
    )

    public fun cacheStats(): WorldgenCacheStats {
        var templates = 0
        var templateBlocks = 0L
        var assemblies = 0
        var assemblyPieces = 0L
        for (loader in loaders) {
            val (count, blocks) = loader.templateStats()
            templates += count
            templateBlocks += blocks
        }
        for (village in villages) {
            val (count, pieces) = village.assemblyStats()
            assemblies += count
            assemblyPieces += pieces
        }
        return WorldgenCacheStats(templates, templateBlocks, assemblies, assemblyPieces)
    }

    /**
     * A world's terrain, and the structure generator behind it when there is one worth naming.
     *
     * The generator is handed back rather than kept here because the commands that work on one
     * structure (`/place structure`, `/locate structure`) need the very object generation uses: a
     * second one built from the same seed would still be a second one.
     */
    public class Generators(
        public val generate: (chunkX: Int, chunkZ: Int, gameTime: Long, difficulty: String) -> Chunk,
        public val villages: VillageStructureGenerator? = null,
    )

    public fun create(
        declaration: WorldDeclaration,
        metadata: WorldMetadata,
    ): Generators =
        when (declaration.dimension) {
            WorldDimension.OVERWORLD -> overworld(metadata.seed, metadata.gameRules[GameRuleRegistry.SPAWN_MOBS])
            WorldDimension.NETHER -> Generators(nether(metadata.seed, metadata.gameRules[GameRuleRegistry.SPAWN_MOBS]))
            WorldDimension.END -> Generators(end(metadata.seed))
        }

    private fun overworld(
        seed: Long,
        spawnMobs: Boolean,
    ): Generators {
        val biomeZoomSeed = obfuscateBiomeSeed(seed)
        val positional = XoroshiroRandom(seed).forkPositional()
        val noises = RealNoiseParameters(serverJar, positional)
        // parity: RandomState wires BlendedNoise from random.fromHashOf("minecraft:terrain").
        val base3d = BlendedNoise(positional.fromHashOf("minecraft:terrain"), 0.25, 0.125, 80.0, 160.0, 8.0)
        val router = NoiseRouterData.overworld(noises::get, base3d)
        val surface = RealSurfaceEnvironment(serverJar, positional, router, biomeZoomSeed)
        val villageLoader = VillageDataLoader(serverJar).also(loaders::add)
        val villageGrid = StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312)
        val village =
            VillageStructureGenerator(
                router,
                villageGrid,
                biomeAt = { x, y, z -> surface.noiseBiomeAt(x shr 2, y shr 2, z shr 2) },
                maxDepth = 6,
                maxDistanceFromCenter = 80,
                poolLookup = villageLoader.poolLookup,
                templateLoader = villageLoader.templateLoader,
            ).also(villages::add)
        val biomeSpawns = RealBiomeSpawns(serverJar)
        // The biomes a mineshaft may start in come from the jar's own tags, expanded through the
        // references they are almost entirely written in.
        val biomeTags = BiomeTagLoader(serverJar)
        val mineshafts =
            MineshaftStructureGenerator(
                biomeAt = { x, y, z -> surface.biomeAt(x, y, z) },
                normalBiomes = biomeTags.biomesIn("minecraft:has_structure/mineshaft"),
                mesaBiomes = biomeTags.biomesIn("minecraft:has_structure/mineshaft_mesa"),
            )
        // A portal has to know the ground height at its four corners before any chunk exists.
        val columns = NoiseColumnHeights(router)
        val ruinedPortals =
            RuinedPortalStructureGenerator(
                variants = RuinedPortalVariants.overworld(biomeTags),
                templateOf = { name -> villageLoader.templateLoader(name) },
                biomeAt = { x, y, z -> surface.biomeAt(x, y, z) },
                surfaceHeightAt = { x, z -> columns.surfaceHeight(x, z) },
                isSolidAt = { x, y, z -> columns.isSolid(x, y, z) },
            )
        val swampHuts =
            SwampHutStructureGenerator(
                biomeAt = { x, y, z -> surface.biomeAt(x, y, z) },
                surfaceHeightAt = { x, z -> columns.surfaceHeight(x, z) },
            )
        val pillagerOutposts =
            PillagerOutpostStructureGenerator(
                router,
                // An outpost takes a fifth of its candidates and keeps ten chunks clear of any
                // village, which is why it needs the villages' own grid to answer at all.
                StructurePlacementGrid(
                    spacing = 32,
                    separation = 8,
                    salt = 165745296,
                    frequency = 0.2f,
                    frequencyReduction = StructurePlacementGrid.FrequencyReduction.LEGACY_TYPE_1,
                    exclusionZone = StructurePlacementGrid.ExclusionZone(chunkCount = 10) { villageGrid },
                ),
                biomeAt = { x, y, z -> surface.noiseBiomeAt(x shr 2, y shr 2, z shr 2) },
                outpostBiomes = biomeTags.biomesIn("minecraft:has_structure/pillager_outpost"),
                poolLookup = villageLoader.poolLookup,
                templateLoader = villageLoader.templateLoader,
            )
        val trailRuins =
            TrailRuinsStructureGenerator(
                router,
                StructurePlacementGrid(spacing = 34, separation = 8, salt = 83469867),
                biomeAt = { x, y, z -> surface.noiseBiomeAt(x shr 2, y shr 2, z shr 2) },
                ruinBiomes = biomeTags.biomesIn("minecraft:has_structure/trail_ruins"),
                poolLookup = villageLoader.poolLookup,
                templateLoader = villageLoader.templateLoader,
            )
        val ancientCities =
            AncientCityStructureGenerator(
                biomeAt = { x, y, z -> surface.biomeAt(x, y, z) },
                poolLookup = villageLoader.poolLookup,
                templateLoader = villageLoader.templateLoader,
                cityBiomes = biomeTags.biomesIn("minecraft:has_structure/ancient_city"),
            )
        val trialChambers =
            TrialChamberStructureGenerator(
                biomeAt = { x, y, z -> surface.biomeAt(x, y, z) },
                poolLookup = villageLoader.poolLookup,
                templateLoader = villageLoader.templateLoader,
                chamberBiomes = biomeTags.biomesIn("minecraft:has_structure/trial_chambers"),
            )
        // Step 1's bodies read past the chunk edge, so decoration needs the neighbours' undecorated
        // terrain. One cache per world: a neighbour generated for one chunk is read by the next.
        val neighbours =
            NeighbourTerrainCache { neighbourX, neighbourZ ->
                RealTerrainGenerator
                    .generateTerrain(
                        router,
                        surface,
                        neighbourX,
                        neighbourZ,
                        seed,
                        village,
                        mineshafts,
                        ruinedPortals,
                        swampHuts,
                        pillagerOutposts,
                        trailRuins,
                        ancientCities,
                        trialChambers,
                    ).chunk
            }
        return Generators(
            generate = {
                chunkX,
                chunkZ,
                gameTime,
                difficulty,
                ->
                RealTerrainGenerator
                    .generate(
                        router,
                        surface,
                        chunkX,
                        chunkZ,
                        seed,
                        village,
                        neighbours,
                        mineshafts,
                        ruinedPortals,
                        swampHuts,
                        pillagerOutposts,
                        trailRuins,
                        ancientCities,
                        trialChambers,
                        villageLoader.templateLoader,
                    ).also { chunk ->
                        if (spawnMobs) {
                            val biome =
                                Identifier.parse(surface.biomeAt(chunkX shl 4, chunk.verticalBounds.maxY, chunkZ shl 4))
                            val random = WorldgenRandom(LegacyRandom(0L))
                            random.setDecorationSeed(seed, chunkX shl 4, chunkZ shl 4)
                            OriginalMobSpawner.spawnInto(
                                chunk,
                                biomeSpawns.listFor(biome, "creature"),
                                biomeSpawns.creatureProbabilityFor(biome),
                                random,
                                gameTime = gameTime,
                                difficulty = difficulty,
                                equipmentFinalizer = spawnEquipment,
                            )
                        }
                    }
            },
            villages = village,
        )
    }

    private fun nether(
        seed: Long,
        spawnMobs: Boolean,
    ): (Int, Int, Long, String) -> Chunk {
        val biomeZoomSeed = obfuscateBiomeSeed(seed)
        // parity: noise_settings/nether.json has legacy_random_source=true.
        val base3d = BlendedNoise(LegacyRandom(seed), 0.25, 0.375, 80.0, 60.0, 8.0)
        val router =
            NoiseRouterData.nether(
                noise = { id -> legacyNetherNoise(id, seed + if (id == "nether/vegetation") 1L else 0L) },
                base3d = base3d,
            )
        val biomes = NetherBiomeSource(router.temperature, router.vegetation, biomeZoomSeed)
        val surface =
            NetherSurfaceEnvironment(serverJar, LegacyRandom(seed).forkPositional(), router, biomes, biomeZoomSeed)
        val structureLoader = VillageDataLoader(serverJar).also(loaders::add)
        val structures =
            NetherStructureGenerator(
                router,
                biomes,
                structureLoader.templateLoader,
                structureLoader.poolLookup,
            )
        val biomeSpawns = RealBiomeSpawns(serverJar)
        // The biomes a mineshaft may start in come from the jar's own tags, expanded through the
        // references they are almost entirely written in.
        val biomeTags = BiomeTagLoader(serverJar)
        val mineshafts =
            MineshaftStructureGenerator(
                biomeAt = { x, y, z -> surface.biomeAt(x, y, z) },
                normalBiomes = biomeTags.biomesIn("minecraft:has_structure/mineshaft"),
                mesaBiomes = biomeTags.biomesIn("minecraft:has_structure/mineshaft_mesa"),
            )
        return { chunkX, chunkZ, gameTime, difficulty ->
            val chunk =
                DimensionTerrainGenerator.nether(
                    router,
                    biomes,
                    chunkX,
                    chunkZ,
                    surfaceEnvironment = surface,
                    seed = seed,
                    structureGenerator = structures,
                )
            if (spawnMobs) {
                val biome = biomes.biomeAt(chunkX shl 4, chunk.verticalBounds.maxY, chunkZ shl 4).id
                val random = WorldgenRandom(LegacyRandom(0L))
                random.setDecorationSeed(seed, chunkX shl 4, chunkZ shl 4)
                OriginalMobSpawner.spawnInto(
                    chunk,
                    biomeSpawns.listFor(biome, "creature"),
                    biomeSpawns.creatureProbabilityFor(biome),
                    random,
                    hasCeiling = true,
                    gameTime = gameTime,
                    difficulty = difficulty,
                    equipmentFinalizer = spawnEquipment,
                )
            }
            chunk
        }
    }

    private fun end(seed: Long): (Int, Int, Long, String) -> Chunk {
        val biomeZoomSeed = obfuscateBiomeSeed(seed)
        // parity: noise_settings/end.json has legacy_random_source=true.
        val base3d = BlendedNoise(LegacyRandom(seed), 0.25, 0.25, 80.0, 160.0, 4.0)
        val router = NoiseRouterData.end(base3d, seed)
        val biomes = TheEndBiomeSource(router.erosion, biomeZoomSeed)
        val structureLoader = VillageDataLoader(serverJar).also(loaders::add)
        val structures = EndCityStructureGenerator(router, biomes, structureLoader.templateLoader)
        return { chunkX, chunkZ, _, _ ->
            DimensionTerrainGenerator.end(router, biomes, chunkX, chunkZ, seed, structures)
        }
    }

    private fun legacyNetherNoise(
        id: String,
        seed: Long,
    ): NormalNoise {
        val parameters = NoiseParametersJson.parse(serverJar.readText("data/minecraft/worldgen/noise/$id.json"))
        return NormalNoise.createLegacyNetherBiome(
            LegacyRandom(seed),
            parameters.firstOctave,
            parameters.amplitudes,
        )
    }
}
