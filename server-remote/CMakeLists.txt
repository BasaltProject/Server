basalt_module(server-remote
    SOURCES
        src/ApiToken.cpp
        src/AttemptTracker.cpp
        src/Http.cpp
        src/ManagementDto.cpp
        src/ManagementLists.cpp
        src/ManagementNotifier.cpp
        src/ManagementRpc.cpp
        src/ManagementSchema.cpp
        src/ManagementServerMethods.cpp
        src/ManagementSettings.cpp
        src/RconCodec.cpp
        src/RemoteJson.cpp
        src/RemoteManagement.cpp
        src/RemoteService.cpp
        src/Transport.cpp
        src/WebSocketCodec.cpp
    DEPENDS basalt::wire-common basalt::wire-io
    PRIVATE_DEPENDS basalt::mbedtls
)

basalt_test(server-remote-test
    SOURCES test/RemoteCodecTest.cpp test/ManagementRpcTest.cpp
    DEPENDS basalt::server-remote
)
