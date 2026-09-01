basalt_module(server-assets
    SOURCES
        src/AssetCache.cpp
        src/EulaFile.cpp
        src/LangAssets.cpp
        src/ServerJarAssets.cpp
        src/VersionMetadata.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-text
    PRIVATE_DEPENDS basalt::mbedtls
)

basalt_test(server-assets-test
    SOURCES
        test/AssetsTest.cpp
    DEPENDS basalt::server-assets
)

# The only test here that writes files, so it is told where inside the build tree to put them.
target_compile_definitions(server-assets-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)
