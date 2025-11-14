basalt_module(server-plugins
    SOURCES
        src/PluginCommandSpec.cpp
        src/PluginManifest.cpp
        src/PluginEvents.cpp
        src/PluginEventTypes.cpp
        src/PluginFolder.cpp
        src/PluginOrder.cpp
        src/PluginPackage.cpp
        src/PluginText.cpp
        src/PluginToml.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::server-lua
)

basalt_test(server-plugins-event-types-test
    SOURCES
        test/PluginEventTypesTest.cpp
    DEPENDS basalt::server-plugins
)

basalt_test(server-plugins-events-test
    SOURCES
        test/PluginEventsTest.cpp
    DEPENDS basalt::server-plugins
)

basalt_test(server-plugins-package-test
    SOURCES
        test/PluginPackageTest.cpp
    DEPENDS basalt::server-plugins
)

target_compile_definitions(server-plugins-package-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-plugins-text-test
    SOURCES
        test/PluginTextTest.cpp
    DEPENDS basalt::server-plugins
)

target_compile_definitions(server-plugins-text-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
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
