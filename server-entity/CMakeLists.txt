basalt_module(server-entity
    SOURCES
        src/Breeding.cpp
        src/Combat.cpp
        src/Collision.cpp
        src/Geometry.cpp
        src/Leash.cpp
        src/LevelValue.cpp
        src/MovementSystem.cpp
        src/Projectiles.cpp
        src/World.cpp
    DEPENDS basalt::wire-common
)

basalt_test(server-entity-test
    SOURCES
        test/EntityTest.cpp
    DEPENDS basalt::server-entity
)
