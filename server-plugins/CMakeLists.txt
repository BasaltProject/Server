basalt_module(server-plugins
    SOURCES
        src/PluginCommandSpec.cpp
        src/PluginManifest.cpp
        src/PluginOrder.cpp
        src/PluginToml.cpp
    DEPENDS basalt::wire-common basalt::server-lua
)

basalt_test(server-plugins-toml-test
    SOURCES
        test/PluginTomlTest.cpp
    DEPENDS basalt::server-plugins
)

basalt_test(server-plugins-command-test
    SOURCES
        test/PluginCommandSpecTest.cpp
    DEPENDS basalt::server-plugins
)

basalt_test(server-plugins-order-test
    SOURCES
        test/PluginOrderTest.cpp
    DEPENDS basalt::server-plugins
)

basalt_test(server-plugins-test
    SOURCES
        test/PluginManifestTest.cpp
    DEPENDS basalt::server-plugins
)
