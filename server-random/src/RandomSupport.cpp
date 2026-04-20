#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/wire/common/hash/Md5.hpp>

#include <cstdint>

namespace basalt::server::random {
namespace {
std::int64_t bytesToLongBigEndian(const std::array<std::uint8_t, 16>& bytes, std::size_t offset) noexcept {
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        result = (result << 8) | bytes[offset + i];
    }
    return static_cast<std::int64_t>(result);
}
} // namespace

Seed128bit seedFromHashOf(std::string_view input) {
    const std::array<std::uint8_t, 16> hash = wire::common::md5(input);
    return Seed128bit{bytesToLongBigEndian(hash, 0), bytesToLongBigEndian(hash, 8)};
}

std::int32_t javaStringHashCode(std::string_view value) noexcept {
    std::uint32_t hash = 0;
    const auto unit = [&hash](std::uint32_t code) { hash = 31U * hash + code; };

    for (std::size_t i = 0; i < value.size();) {
        const auto lead = static_cast<std::uint8_t>(value[i]);
        std::uint32_t code = lead;
        std::size_t extra = 0;
        if (lead >= 0xF0) {
            code = lead & 0x07U;
            extra = 3;
        } else if (lead >= 0xE0) {
            code = lead & 0x0FU;
            extra = 2;
        } else if (lead >= 0xC0) {
            code = lead & 0x1FU;
            extra = 1;
        }
        if (i + extra >= value.size()) {
            extra = 0;
            code = lead;
        }
        for (std::size_t k = 1; k <= extra; ++k) {
            code = (code << 6) | (static_cast<std::uint8_t>(value[i + k]) & 0x3FU);
        }
        i += extra + 1;

        if (code > 0xFFFF) {
            const std::uint32_t adjusted = code - 0x10000U;
            unit(0xD800U + (adjusted >> 10));
            unit(0xDC00U + (adjusted & 0x3FFU));
        } else {
            unit(code);
        }
    }
    return static_cast<std::int32_t>(hash);
}
} // namespace basalt::server::random
