basalt_module(server-world
    SOURCES
        src/ChunkSection.cpp
        src/ChunkTypes.cpp
        src/PalettedContainer.cpp
        src/noise/ImprovedNoise.cpp
        src/noise/NormalNoise.cpp
        src/noise/PerlinNoise.cpp
        src/noise/SimplexNoise.cpp
    DEPENDS basalt::server-random basalt::server-registry basalt::wire-common
)

basalt_test(server-world-noise-test
    SOURCES
        test/NoiseTest.cpp
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
