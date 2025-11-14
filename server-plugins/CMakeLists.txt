basalt_module(server-plugins
    SOURCES
        src/PluginManifest.cpp
    DEPENDS basalt::wire-common basalt::server-lua
)

basalt_test(server-plugins-test
    SOURCES
        test/PluginManifestTest.cpp
    DEPENDS basalt::server-plugins
)
