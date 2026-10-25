#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::remote::rcon {

namespace packetType {
inline constexpr std::int32_t kResponse = 0;
inline constexpr std::int32_t kCommand = 2;
inline constexpr std::int32_t kAuth = 3;
} // namespace packetType

inline constexpr std::int32_t kAuthFailedId = -1;
inline constexpr std::size_t kMaxBodyBytes = 4096;
inline constexpr std::size_t kMaxRequestBytes = kMaxBodyBytes + 10;

struct Packet {
    std::int32_t id = 0;
    std::int32_t type = 0;
    std::string body;
};

void encode(std::string& out, const Packet& packet);

/// Splits an output into packets of at most kMaxBodyBytes; an empty output is still one empty packet.
void encodeResponse(std::string& out, std::int32_t id, std::string_view output);

enum class ReadResult : std::uint8_t { NeedMore, Packet, Malformed };

/// Consumes one packet from the front of `buffer` when it is complete.
[[nodiscard]] ReadResult take(std::string& buffer, Packet& into);

} // namespace basalt::server::remote::rcon
