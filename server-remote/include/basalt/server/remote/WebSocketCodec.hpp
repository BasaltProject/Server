#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::remote::websocket {

namespace opcode {
inline constexpr std::uint8_t kContinuation = 0x0;
inline constexpr std::uint8_t kText = 0x1;
inline constexpr std::uint8_t kBinary = 0x2;
inline constexpr std::uint8_t kClose = 0x8;
inline constexpr std::uint8_t kPing = 0x9;
inline constexpr std::uint8_t kPong = 0xA;
} // namespace opcode

struct Frame {
    bool fin = true;
    std::uint8_t opcode = opcode::kText;
    std::string payload;
};

/// RFC 6455 section 4.2.2: base64 of the SHA-1 of the key and the protocol GUID.
[[nodiscard]] std::string acceptKey(std::string_view clientKey);

/// Server frames are never masked.
void encode(std::string& out, std::uint8_t opcode, std::string_view payload);

enum class ReadResult : std::uint8_t { NeedMore, Frame, Malformed, TooLarge };

/// Consumes one complete client frame from the front of `buffer`; client frames must be masked.
[[nodiscard]] ReadResult take(std::string& buffer, Frame& into, std::size_t maxPayload);

} // namespace basalt::server::remote::websocket
