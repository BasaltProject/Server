#pragma once

#include <basalt/wire/bedrock/BedrockConnection.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::runtime::bedrock {

class Channel {
public:
    virtual ~Channel() = default;

    [[nodiscard]] virtual int protocolVersion() const = 0;

    virtual void sendPacket(wire::bedrock::BedrockPacket packet, std::span<const std::uint8_t> body) = 0;

    virtual void flush() = 0;

    virtual void close() = 0;
};

struct Identity {
    std::string gamertag;
    std::string xuid;
    bool authenticated = false;
    std::string address;
    int protocol = 0;
    std::string skinId;
    std::string resourcePatch;
    std::int32_t skinWidth = 0;
    std::int32_t skinHeight = 0;
    std::vector<std::uint8_t> skinPixels;
};

class Carried {
public:
    virtual ~Carried() = default;

    /// One game packet as the client frames it, a header and a body; safe from any thread.
    virtual void receive(std::span<const std::uint8_t> packet) = 0;

    virtual void end() = 0;
};

} // namespace basalt::server::runtime::bedrock
