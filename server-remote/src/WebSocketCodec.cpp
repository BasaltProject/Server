#include <basalt/server/remote/WebSocketCodec.hpp>
#include <basalt/wire/common/Base64.hpp>

#include <array>
#include <cstdint>
#include <mbedtls/sha1.h>
#include <span>

namespace basalt::server::remote::websocket {

std::string acceptKey(std::string_view clientKey) {
    constexpr std::string_view kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string joined{clientKey};
    joined += kGuid;
    std::array<std::uint8_t, 20> digest{};
    mbedtls_sha1(reinterpret_cast<const unsigned char*>(joined.data()), joined.size(), digest.data());
    return wire::common::base64::encode(std::span<const std::uint8_t>{digest});
}

void encode(std::string& out, std::uint8_t opcode, std::string_view payload) {
    out.push_back(static_cast<char>(0x80U | opcode));
    if (payload.size() < 126) {
        out.push_back(static_cast<char>(payload.size()));
    } else if (payload.size() <= 0xFFFFU) {
        out.push_back(126);
        out.push_back(static_cast<char>(payload.size() >> 8));
        out.push_back(static_cast<char>(payload.size() & 0xFFU));
    } else {
        out.push_back(127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            out.push_back(static_cast<char>((static_cast<std::uint64_t>(payload.size()) >> shift) & 0xFFU));
        }
    }
    out += payload;
}

ReadResult take(std::string& buffer, Frame& into, std::size_t maxPayload) {
    if (buffer.size() < 2) {
        return ReadResult::NeedMore;
    }
    const auto byte = [&buffer](std::size_t at) { return static_cast<std::uint8_t>(buffer[at]); };
    if ((byte(0) & 0x70U) != 0 || (byte(1) & 0x80U) == 0) {
        return ReadResult::Malformed;
    }
    std::uint64_t length = byte(1) & 0x7FU;
    std::size_t header = 2;
    if (length == 126) {
        if (buffer.size() < 4) {
            return ReadResult::NeedMore;
        }
        length = static_cast<std::uint64_t>(byte(2)) << 8 | byte(3);
        header = 4;
    } else if (length == 127) {
        if (buffer.size() < 10) {
            return ReadResult::NeedMore;
        }
        length = 0;
        for (std::size_t i = 2; i < 10; ++i) {
            length = length << 8 | byte(i);
        }
        header = 10;
    }
    const std::uint8_t op = byte(0) & 0x0FU;
    if (op >= opcode::kClose && (length > 125 || (byte(0) & 0x80U) == 0)) {
        return ReadResult::Malformed;
    }
    if (length > maxPayload) {
        return ReadResult::TooLarge;
    }
    const std::size_t total = header + 4 + static_cast<std::size_t>(length);
    if (buffer.size() < total) {
        return ReadResult::NeedMore;
    }
    into.fin = (byte(0) & 0x80U) != 0;
    into.opcode = op;
    into.payload.resize(static_cast<std::size_t>(length));
    for (std::size_t i = 0; i < into.payload.size(); ++i) {
        into.payload[i] = static_cast<char>(byte(header + 4 + i) ^ byte(header + (i & 3U)));
    }
    buffer.erase(0, total);
    return ReadResult::Frame;
}

} // namespace basalt::server::remote::websocket
