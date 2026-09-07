#include <basalt/server/world/terrain/StructureReferenceOrder.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::world::terrain {

std::vector<std::pair<std::int32_t, std::int32_t>>
vanillaStructureReferenceChunkOrder(const std::vector<std::pair<std::int32_t, std::int32_t>>& references) {
    std::array<std::optional<std::pair<std::int32_t, std::int32_t>>, 32> table{};
    std::optional<std::pair<std::int32_t, std::int32_t>> zeroReference;
    for (const auto& reference : references) {
        const std::uint64_t key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(reference.first))) |
                                  (static_cast<std::uint64_t>(static_cast<std::uint32_t>(reference.second)) << 32);
        if (key == 0) {
            zeroReference = reference;
            continue;
        }
        std::uint64_t mixed = key * static_cast<std::uint64_t>(-7046029254386353131LL);
        mixed ^= mixed >> 32;
        mixed ^= mixed >> 16;
        std::size_t slot = static_cast<std::size_t>(static_cast<std::uint32_t>(mixed) & 31U);
        while (table[slot].has_value()) {
            slot = (slot + 1) & 31U;
        }
        table[slot] = reference;
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> ordered;
    ordered.reserve(references.size());
    if (zeroReference.has_value()) {
        ordered.push_back(*zeroReference);
    }
    for (std::int32_t slot = 31; slot >= 0; --slot) {
        if (table[static_cast<std::size_t>(slot)].has_value()) {
            ordered.push_back(*table[static_cast<std::size_t>(slot)]);
        }
    }
    return ordered;
}

} // namespace basalt::server::world::terrain
