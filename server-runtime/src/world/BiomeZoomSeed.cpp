#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>
#include <basalt/wire/common/hash/Sha256.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace basalt::server::runtime::world {

std::int64_t obfuscateBiomeSeed(std::int64_t seed) {
    std::array<std::uint8_t, sizeof(std::int64_t)> input{};
    const auto unsignedSeed = static_cast<std::uint64_t>(seed);
    for (std::size_t index = 0; index < input.size(); index++) {
        input[index] = static_cast<std::uint8_t>(unsignedSeed >> (index * 8));
    }
    const std::array<std::uint8_t, 32> digest = wire::common::sha256(input);
    std::uint64_t result = 0;
    for (std::size_t index = 0; index < sizeof(std::int64_t); index++) {
        result |= static_cast<std::uint64_t>(digest[index]) << (index * 8);
    }
    return static_cast<std::int64_t>(result);
}

} // namespace basalt::server::runtime::world
