basalt_module(server-random
    SOURCES
        src/PositionalRandomFactory.cpp
        src/RandomSource.cpp
        src/RandomSupport.cpp
        src/WorldgenSeeds.cpp
    DEPENDS basalt::wire-common
)

basalt_test(server-random-test
    SOURCES
        test/RandomTest.cpp
    DEPENDS basalt::server-random
)
