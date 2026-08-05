basalt_module(server-remote
    SOURCES
        src/ApiToken.cpp
        src/AttemptTracker.cpp
        src/Http.cpp
        src/RconCodec.cpp
        src/RemoteService.cpp
        src/Transport.cpp
        src/WebSocketCodec.cpp
    DEPENDS basalt::wire-common basalt::wire-io
    PRIVATE_DEPENDS basalt::mbedtls
)

basalt_test(server-remote-test
    SOURCES test/RemoteCodecTest.cpp
    DEPENDS basalt::server-remote
)
