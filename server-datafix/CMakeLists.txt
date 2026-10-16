basalt_module(server-datafix
    SOURCES
        src/DataFixer.cpp
        src/FixSupport.cpp
        src/Fixes4000.cpp
        src/Fixes4300.cpp
        src/Fixes4996.cpp
        src/ItemWalk.cpp
        src/TextFixes.cpp
    DEPENDS basalt::wire-common basalt::wire-nbt
)

basalt_test(server-datafix-test
    SOURCES
        test/DataFixerTest.cpp
    DEPENDS basalt::server-datafix
)
