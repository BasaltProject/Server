package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.noise.NoiseParametersJson
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.terrain.DimensionTerrainGenerator
import org.basaltmc.server.world.terrain.EndCityStructureGenerator
import org.basaltmc.server.world.terrain.NetherBiomeSource
import org.basaltmc.server.world.terrain.NetherStructureGenerator
import org.basaltmc.server.world.terrain.NoiseRouterData
import org.basaltmc.server.world.terrain.RealTerrainGenerator
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import org.basaltmc.server.world.terrain.VillageStructureGenerator

/** Builds one correctly seeded normal-terrain generator for each loaded world. */
public class VanillaWorldGeneratorFactory(
    private val serverJar: ServerJarAssets,
) {
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
    public fun create(
        declaration: WorldDeclaration,
        metadata: WorldMetadata,
    ): (chunkX: Int, chunkZ: Int) -> Chunk =
        when (declaration.dimension) {
            WorldDimension.OVERWORLD -> overworld(metadata.seed)
            WorldDimension.NETHER -> nether(metadata.seed)
            WorldDimension.END -> end(metadata.seed)
        }

    private fun overworld(seed: Long): (Int, Int) -> Chunk {
        val positional = XoroshiroRandom(seed).forkPositional()
        val noises = RealNoiseParameters(serverJar, positional)
        // parity: RandomState wires BlendedNoise from random.fromHashOf("minecraft:terrain").
        val base3d = BlendedNoise(positional.fromHashOf("minecraft:terrain"), 0.25, 0.125, 80.0, 160.0, 8.0)
        val router = NoiseRouterData.overworld(noises::get, base3d)
        val surface = RealSurfaceEnvironment(serverJar, positional, router)
        val villageLoader = VillageDataLoader(serverJar).also(loaders::add)
        val village =
            VillageStructureGenerator(
                router,
                StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312),
                biomeAt = surface::biomeAt,
                maxDepth = 6,
                maxDistanceFromCenter = 80,
                poolLookup = villageLoader.poolLookup,
                templateLoader = villageLoader.templateLoader,
            ).also(villages::add)
        return { chunkX, chunkZ ->
            RealTerrainGenerator.generate(router, surface, chunkX, chunkZ, seed, village)
        }
    }

    private fun nether(seed: Long): (Int, Int) -> Chunk {
        // parity: noise_settings/nether.json has legacy_random_source=true.
        val base3d = BlendedNoise(LegacyRandom(seed), 0.25, 0.375, 80.0, 60.0, 8.0)
        val router =
            NoiseRouterData.nether(
                noise = { id -> legacyNetherNoise(id, seed + if (id == "nether/vegetation") 1L else 0L) },
                base3d = base3d,
            )
        val biomes = NetherBiomeSource(router.temperature, router.vegetation)
        val surface = NetherSurfaceEnvironment(serverJar, LegacyRandom(seed).forkPositional(), router, biomes)
        val structureLoader = VillageDataLoader(serverJar).also(loaders::add)
        val structures =
            NetherStructureGenerator(
                router,
                biomes,
                structureLoader.templateLoader,
                structureLoader.poolLookup,
            )
        return { chunkX, chunkZ ->
            DimensionTerrainGenerator.nether(
                router,
                biomes,
                chunkX,
                chunkZ,
                surfaceEnvironment = surface,
                seed = seed,
                structureGenerator = structures,
            )
        }
    }

    private fun end(seed: Long): (Int, Int) -> Chunk {
        // parity: noise_settings/end.json has legacy_random_source=true.
        val base3d = BlendedNoise(LegacyRandom(seed), 0.25, 0.25, 80.0, 160.0, 4.0)
        val router = NoiseRouterData.end(base3d, seed)
        val biomes = TheEndBiomeSource(router.erosion)
        val structureLoader = VillageDataLoader(serverJar).also(loaders::add)
        val structures = EndCityStructureGenerator(router, biomes, structureLoader.templateLoader)
        return { chunkX, chunkZ ->
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
