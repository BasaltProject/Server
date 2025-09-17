basalt_module(server-world
    SOURCES
        src/Chunk.cpp
        src/ChunkDataCodec.cpp
        src/ChunkSection.cpp
        src/ChunkTypes.cpp
        src/PalettedContainer.cpp
        src/light/Light.cpp
        src/light/LightSpread.cpp
        src/light/LightWindow.cpp
        src/noise/ImprovedNoise.cpp
        src/region/ChunkNbt.cpp
        src/structure/StructureBounds.cpp
        src/noise/NormalNoise.cpp
        src/noise/PerlinNoise.cpp
        src/noise/SimplexNoise.cpp
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
