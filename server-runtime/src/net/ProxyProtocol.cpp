#include <basalt/server/runtime/net/ProxyProtocol.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::net::proxyProtocol {
namespace {

constexpr std::array<std::uint8_t, 12> kV2Signature{0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D,
                                                    0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A};
constexpr std::string_view kV1Prefix = "PROXY ";
constexpr std::size_t kV1MaxLength = 107;
constexpr std::size_t kV2MaxPayload = 536;

using ReadSome = std::function<std::size_t(std::span<std::uint8_t>)>;

[[nodiscard]] bool readExactly(const ReadSome& readSome, std::span<std::uint8_t> into) {
    std::size_t have = 0;
    while (have < into.size()) {
        const std::size_t got = readSome(into.subspan(have));
        if (got == 0) {
            return false;
        }
        have += got;
    }
    return true;
}

[[nodiscard]] std::string formatIpv6(std::span<const std::uint8_t> bytes) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string text;
    for (std::size_t group = 0; group < 8; ++group) {
        if (group != 0) {
            text += ':';
        }
        const unsigned value = (static_cast<unsigned>(bytes[group * 2]) << 8) | bytes[group * 2 + 1];
        std::string part;
        for (int shift = 12; shift >= 0; shift -= 4) {
            const unsigned nibble = (value >> shift) & 0xF;
            if (!part.empty() || nibble != 0 || shift == 0) {
                part += kHex[nibble];
            }
        }
        text += part;
    }
    return text;
}

[[nodiscard]] std::expected<Parsed, std::string> parseV1(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t space = std::min(line.find(' ', start), line.size());
        fields.push_back(line.substr(start, space - start));
        start = space + 1;
    }
    if (fields.size() >= 2 && fields[1] == "UNKNOWN") {
        return Parsed{};
    }
    if (fields.size() != 6 || (fields[1] != "TCP4" && fields[1] != "TCP6")) {
        return std::unexpected("malformed PROXY v1 header");
    }
    unsigned port = 0;
    const std::string_view text = fields[4];
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (error != std::errc{} || end != text.data() + text.size() || port > 65535 || fields[2].empty()) {
        return std::unexpected("malformed PROXY v1 header");
    }
    return Parsed{Origin{std::string{fields[2]}, static_cast<std::uint16_t>(port)}};
}

[[nodiscard]] std::expected<Parsed, std::string> parseV2(const ReadSome& readSome) {
    std::array<std::uint8_t, 4> fixed{};
    if (!readExactly(readSome, fixed)) {
        return std::unexpected("truncated PROXY v2 header");
    }
    const std::uint8_t versionCommand = fixed[0];
    const std::uint8_t family = fixed[1];
    const std::size_t length = (static_cast<std::size_t>(fixed[2]) << 8) | fixed[3];
    if ((versionCommand >> 4) != 2 || length > kV2MaxPayload) {
        return std::unexpected("unsupported PROXY v2 header");
    }
    std::vector<std::uint8_t> payload(length);
    if (!readExactly(readSome, payload)) {
        return std::unexpected("truncated PROXY v2 header");
    }
    if ((versionCommand & 0x0F) == 0) {
        return Parsed{};
    }
    if ((versionCommand & 0x0F) != 1) {
        return std::unexpected("unsupported PROXY v2 command");
    }
    const std::uint8_t addressFamily = family >> 4;
    if (addressFamily == 1 && length >= 12) {
        const std::string text = std::to_string(payload[0]) + "." + std::to_string(payload[1]) + "." +
                                 std::to_string(payload[2]) + "." + std::to_string(payload[3]);
        return Parsed{Origin{text, static_cast<std::uint16_t>((payload[8] << 8) | payload[9])}};
    }
    if (addressFamily == 2 && length >= 36) {
        return Parsed{Origin{formatIpv6(std::span{payload}.first(16)),
                             static_cast<std::uint16_t>((payload[32] << 8) | payload[33])}};
    }
    return Parsed{};
}

} // namespace

std::expected<Parsed, std::string> readHeader(const ReadSome& readSome) {
    std::array<std::uint8_t, 12> head{};
    if (!readExactly(readSome, head)) {
        return std::unexpected("connection closed before the PROXY header");
    }
    if (std::memcmp(head.data(), kV2Signature.data(), head.size()) == 0) {
        return parseV2(readSome);
    }
    if (std::string_view{reinterpret_cast<const char*>(head.data()), kV1Prefix.size()} != kV1Prefix) {
        return std::unexpected("connection did not start with a PROXY header");
    }
    std::string line{reinterpret_cast<const char*>(head.data()), head.size()};
    while (line.size() < kV1MaxLength && !line.ends_with("\r\n")) {
        std::array<std::uint8_t, 1> one{};
        if (!readExactly(readSome, one)) {
            return std::unexpected("truncated PROXY v1 header");
        }
        line += static_cast<char>(one[0]);
    }
    if (!line.ends_with("\r\n")) {
        return std::unexpected("PROXY v1 header too long");
    }
    line.resize(line.size() - 2);
    return parseV1(line);
}

} // namespace basalt::server::runtime::net::proxyProtocol
