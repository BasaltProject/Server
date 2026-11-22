basalt_wasm_host_registry(_wasm_imports
    DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm
    PREFIX WasmImports
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/WasmImportsRegistry.cpp
    INCLUDES WasmState.hpp
    NAMESPACE basalt::server::plugins::wasm
    ENTRY "void addAllImports(State& state, std::vector<abi::HostImport>& out)"
    DECLARE "void add@Area@Imports(State& state, std::vector<abi::HostImport>& out)"
    CALL "add@Area@Imports(state, out)")

basalt_module(server-plugins
    SOURCES
        src/PluginEvents.cpp
        src/PluginEventTypes.cpp
        src/PluginHookEventTypes.cpp
        src/PluginMechanics.cpp
        src/PluginChannelHost.cpp
        src/PluginConfig.cpp
        src/PluginHost.cpp
        src/PluginHostWasm.cpp
        src/wasm/WasmPlugin.cpp
        src/wasm/WasmState.cpp
        src/PluginHostRuns.cpp
        src/PluginDatafix.cpp
        src/PluginFiles.cpp
        src/PluginPermissions.cpp
        src/PluginPacketRegistry.cpp
        src/PluginRegistrations.cpp
        src/PluginSurfaces.cpp
        ${_wasm_imports}
    DEPENDS basalt::metamorph-datafix basalt::server-entity basalt::server-registry basalt::server-world basalt::wire-auth basalt::wire-common basalt::wire-io basalt::wire-nbt basalt::wire-plugins basalt::wire-wasm
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

basalt_test(server-plugins-text-test
    SOURCES
        test/PluginTextTest.cpp
    DEPENDS basalt::server-plugins
)

target_compile_definitions(server-plugins-text-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-plugins-wasm-text-test
    SOURCES
        test/WasmTextTest.cpp
    DEPENDS basalt::server-plugins
)

target_compile_definitions(server-plugins-wasm-text-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-plugins-wasm-values-test
    SOURCES
        test/WasmValuesTest.cpp
    DEPENDS basalt::server-plugins
)
