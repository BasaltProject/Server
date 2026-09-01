basalt_module(server-lua
    SOURCES
        src/LuaFailure.cpp
        src/LuaSandbox.cpp
    DEPENDS basalt::wire-common
    PRIVATE_DEPENDS basalt::lua
)

basalt_test(server-lua-test
    SOURCES
        test/LuaTest.cpp
    DEPENDS basalt::server-lua
)
