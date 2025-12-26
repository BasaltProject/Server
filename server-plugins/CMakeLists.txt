basalt_module(server-plugins
    SOURCES
        src/PluginCommandSpec.cpp
        src/PluginManifest.cpp
        src/PluginEvents.cpp
        src/PluginEventTypes.cpp
        src/PluginDispatch.cpp
        src/PluginApi.cpp
        src/PluginChannelHost.cpp
        src/PluginEventView.cpp
        src/PluginPacketView.cpp
        src/PluginStore.cpp
        src/PluginFolder.cpp
        src/PluginJson.cpp
        src/PluginOrder.cpp
        src/PluginConfig.cpp
        src/PluginHost.cpp
        src/PluginHostRuns.cpp
        src/PluginMenuObject.cpp
        src/PluginObject.cpp
        src/PluginObjects.cpp
        src/PluginWorldObjects.cpp
        src/PluginPackage.cpp
        src/PluginPacketRegistry.cpp
        src/PluginRegistrations.cpp
        src/PluginSurfaces.cpp
        src/PluginServiceRegistry.cpp
        src/PluginText.cpp
        src/PluginToml.cpp
        src/PluginValues.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::server-lua
    # The event view builds a metatable and holds a registry reference, which the argument facade
    # deliberately does not expose. `lua.h` stays out of every header here and lives in that one
    # `.cpp`, which is the same rule the rest of the project follows for a fat dependency.
    PRIVATE_DEPENDS basalt::lua
)

basalt_test(server-plugins-object-test
    SOURCES
        test/PluginObjectTest.cpp
    DEPENDS basalt::server-plugins
)

basalt_test(server-plugins-event-types-test
    SOURCES
        test/PluginEventTypesTest.cpp
    DEPENDS basalt::server-plugins
)

basalt_test(server-plugins-event-view-test
    SOURCES
        test/PluginEventViewTest.cpp
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

basalt_test(server-plugins-json-test
    SOURCES
        test/PluginJsonTest.cpp
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
