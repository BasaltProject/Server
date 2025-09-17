basalt_module(server-world
    SOURCES
        src/Chunk.cpp
        src/ChunkDataCodec.cpp
        src/ChunkSection.cpp
        src/ChunkTypes.cpp
        src/PalettedContainer.cpp
        src/climate/Climate.cpp
        src/climate/OverworldBiomes.cpp
        src/densityfunction/DensityCache.cpp
        src/densityfunction/DensityFunction.cpp
        src/light/Light.cpp
        src/light/LightSpread.cpp
        src/light/LightWindow.cpp
        src/noise/ImprovedNoise.cpp
        src/region/ChunkNbt.cpp
        src/structure/StructureBounds.cpp
        src/terrain/BiomeClimateData.cpp
    src/terrain/BiomeTemperature.cpp
    src/terrain/BiomeZoomer.cpp
    src/terrain/TerrainProvider.cpp
    src/terrain/NoiseRouterData.cpp
    src/terrain/OverworldAquifer.cpp
        src/noise/NormalNoise.cpp
        src/noise/PerlinNoise.cpp
        src/noise/SimplexNoise.cpp
    src/noise/PerlinSimplexNoise.cpp
    DEPENDS basalt::server-random basalt::server-registry basalt::wire-common basalt::wire-protocol
)

basalt_test(server-world-noise-test
    SOURCES
        test/NoiseTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-light-test
    SOURCES
        test/LightTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-biome-test
    SOURCES
        test/BiomeTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-aquifer-test
    SOURCES
        test/AquiferTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-noise-router-test
    SOURCES
        test/NoiseRouterTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-terrain-test
    SOURCES
        test/TerrainTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-climate-test
    SOURCES
        test/ClimateTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-density-test
    SOURCES
        test/DensityTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-spline-test
    SOURCES
        test/SplineTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-region-test
    SOURCES
        test/RegionTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-chunk-test
    SOURCES
        test/ChunkTest.cpp
    DEPENDS basalt::server-world
)

basalt_bench(server-world-noise-bench
    SOURCES
        bench/NoiseBench.cpp
    DEPENDS basalt::server-world
)
