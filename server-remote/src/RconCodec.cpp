#include <basalt/server/remote/RconCodec.hpp>

#include <algorithm>
#include <cstring>

namespace basalt::server::remote::rcon {
namespace {

void putInt(std::string& out, std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<char>((bits >> shift) & 0xFFU));
    }
}

[[nodiscard]] std::int32_t getInt(const char* at) {
    std::uint32_t bits = 0;
    for (int i = 0; i < 4; ++i) {
        bits |= static_cast<std::uint32_t>(static_cast<unsigned char>(at[i])) << (8 * i);
    }
    return static_cast<std::int32_t>(bits);
}

} // namespace

void encode(std::string& out, const Packet& packet) {
    putInt(out, static_cast<std::int32_t>(packet.body.size() + 10));
    putInt(out, packet.id);
    putInt(out, packet.type);
    out += packet.body;
    out.push_back('\0');
    out.push_back('\0');
}

void encodeResponse(std::string& out, std::int32_t id, std::string_view output) {
    std::size_t at = 0;
    do {
        const std::size_t count = std::min(kMaxBodyBytes, output.size() - at);
        encode(out, Packet{id, packetType::kResponse, std::string{output.substr(at, count)}});
        at += count;
    } while (at < output.size());
}

ReadResult take(std::string& buffer, Packet& into) {
    if (buffer.size() < 4) {
        return ReadResult::NeedMore;
    }
    const std::int32_t length = getInt(buffer.data());
    if (length < 10 || static_cast<std::size_t>(length) > kMaxRequestBytes) {
        return ReadResult::Malformed;
    }
    const std::size_t total = 4 + static_cast<std::size_t>(length);
    if (buffer.size() < total) {
        return ReadResult::NeedMore;
    }
    if (buffer[total - 1] != '\0' || buffer[total - 2] != '\0') {
        return ReadResult::Malformed;
    }
    into.id = getInt(buffer.data() + 4);
    into.type = getInt(buffer.data() + 8);
    into.body.assign(buffer.data() + 12, total - 14);
    buffer.erase(0, total);
    return ReadResult::Packet;
}

} // namespace basalt::server::remote::rcon
