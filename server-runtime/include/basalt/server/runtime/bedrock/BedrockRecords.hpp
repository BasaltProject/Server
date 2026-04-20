#pragma once

#include <basalt/wire/bedrock/BedrockPacketIds.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::bedrock {

class BedrockRecords {
public:
    using Post = std::function<void(wire::bedrock::BedrockPacket, std::vector<std::uint8_t>)>;

    BedrockRecords(int protocol, Post post) : protocol_(protocol), post_(std::move(post)) {}

    void started(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view javaSound);

    void stopped(std::int32_t x, std::int32_t y, std::int32_t z);

private:
    using Position = std::array<std::int32_t, 3>;

    int protocol_;
    Post post_;
    std::map<Position, std::int64_t> handles_;
    std::map<Position, std::string> names_;
    std::int64_t nextHandle_ = 0;
};

} // namespace basalt::server::runtime::bedrock
