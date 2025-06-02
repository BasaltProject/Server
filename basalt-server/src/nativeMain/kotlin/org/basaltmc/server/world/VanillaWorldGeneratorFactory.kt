package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.server.config.WorldDeclaration
import org.basaltmc.server.config.WorldDimension
import org.basaltmc.world.Chunk
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.noise.NoiseParametersJson
import org.basaltmc.world.noise.NormalNoise
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.terrain.DimensionTerrainGenerator
import org.basaltmc.world.terrain.NetherBiomeSource
import org.basaltmc.world.terrain.NoiseRouterData
import org.basaltmc.world.terrain.RealTerrainGenerator
import org.basaltmc.world.terrain.TheEndBiomeSource
import org.basaltmc.world.terrain.VillageStructureGenerator

/** Builds one correctly seeded normal-terrain generator for each loaded world. */
public class VanillaWorldGeneratorFactory(
    private val serverJar: ServerJarAssets,
) {
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
        val villageLoader = VillageDataLoader(serverJar)
        val village =
            VillageStructureGenerator(
                router,
                StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312),
                biomeAt = surface::biomeAt,
                maxDepth = 6,
                maxDistanceFromCenter = 80,
                poolLookup = villageLoader.poolLookup,
                templateLoader = villageLoader.templateLoader,
            )
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
        return { chunkX, chunkZ ->
            DimensionTerrainGenerator.nether(
                router,
                biomes,
                chunkX,
                chunkZ,
                surfaceEnvironment = surface,
                seed = seed,
            )
        }
    }

    private fun end(seed: Long): (Int, Int) -> Chunk {
        // parity: noise_settings/end.json has legacy_random_source=true.
        val base3d = BlendedNoise(LegacyRandom(seed), 0.25, 0.25, 80.0, 160.0, 4.0)
        val router = NoiseRouterData.end(base3d, seed)
        val biomes = TheEndBiomeSource(router.erosion)
        return { chunkX, chunkZ -> DimensionTerrainGenerator.end(router, biomes, chunkX, chunkZ) }
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
