basalt_module(server-entity
    SOURCES
        src/MovementSystem.cpp
        src/World.cpp
    DEPENDS basalt::wire-common
)

basalt_test(server-entity-test
    SOURCES
        test/EntityTest.cpp
    DEPENDS basalt::server-entity
)
