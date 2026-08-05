#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/remote/RconCodec.hpp>
#include <basalt/server/remote/WebSocketCodec.hpp>

#include <doctest.h>

using namespace basalt::server::remote;

TEST_CASE("an rcon packet survives a round trip and long output splits") {
    std::string wire;
    rcon::encode(wire, {7, rcon::packetType::kCommand, "list"});
    rcon::Packet packet;
    CHECK(rcon::take(wire, packet) == rcon::ReadResult::Packet);
    CHECK(packet.id == 7);
    CHECK(packet.type == rcon::packetType::kCommand);
    CHECK(packet.body == "list");
    CHECK(wire.empty());

    std::string many;
    rcon::encodeResponse(many, 9, std::string(rcon::kMaxBodyBytes + 1, 'x'));
    CHECK(rcon::take(many, packet) == rcon::ReadResult::Packet);
    CHECK(packet.body.size() == rcon::kMaxBodyBytes);
    CHECK(rcon::take(many, packet) == rcon::ReadResult::Packet);
    CHECK(packet.body.size() == 1);
}

TEST_CASE("websocket accept key and masked frames match RFC 6455") {
    CHECK(websocket::acceptKey("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

    std::string masked = std::string{"\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58", 11};
    websocket::Frame frame;
    CHECK(websocket::take(masked, frame, 1024) == websocket::ReadResult::Frame);
    CHECK(frame.payload == "Hello");

    std::string unmasked = std::string{"\x81\x05Hello", 7};
    CHECK(websocket::take(unmasked, frame, 1024) == websocket::ReadResult::Malformed);
}
