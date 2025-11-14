basalt_module(server-lua
    SOURCES
        src/LuaArgs.cpp
        src/LuaCall.cpp
        src/LuaFailure.cpp
        src/LuaSandbox.cpp
    DEPENDS basalt::wire-common
    PRIVATE_DEPENDS basalt::lua
)

basalt_test(server-lua-call-test
    SOURCES
        test/LuaCallTest.cpp
    DEPENDS basalt::server-lua
)

basalt_test(server-lua-test
    SOURCES
        test/LuaTest.cpp
    DEPENDS basalt::server-lua
)
