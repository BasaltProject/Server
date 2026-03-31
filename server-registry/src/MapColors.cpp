#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace basalt::server::registry::mapColors {
namespace {

/// parity: `MapColor.Brightness.modifier`, by brightness id.
constexpr std::array<std::uint32_t, 4> kModifiers{180, 220, 255, 135};

[[nodiscard]] const std::vector<std::uint8_t>& byState() {
    static const std::vector<std::uint8_t> table = [] {
        std::vector<std::uint8_t> colors(static_cast<std::size_t>(blocks::stateCount()), 0);
        const auto runs = blockMapColorsTable();
        for (std::size_t run = 0; run < runs.size(); ++run) {
            const auto end = run + 1 < runs.size() ? static_cast<std::size_t>(runs[run + 1].first) : colors.size();
            for (auto state = static_cast<std::size_t>(runs[run].first); state < end && state < colors.size();
                 ++state) {
                colors[state] = static_cast<std::uint8_t>(runs[run].second);
            }
        }
        return colors;
    }();
    return table;
}

} // namespace

std::int32_t of(std::int32_t stateId) {
    const std::vector<std::uint8_t>& table = byState();
    return stateId < 0 || static_cast<std::size_t>(stateId) >= table.size() ? kNone
                                                                            : table[static_cast<std::size_t>(stateId)];
}

std::uint32_t argbOf(std::uint8_t packedColor) {
    const std::size_t id = packedColor >> 2U;
    if (id == kNone || id >= mapColorsTable().size()) {
        return 0;
    }
    const auto rgb = static_cast<std::uint32_t>(mapColorsTable()[id]);
    const std::uint32_t modifier = kModifiers[packedColor & 3U];
    const std::uint32_t red = ((rgb >> 16U) & 0xFFU) * modifier / 255U;
    const std::uint32_t green = ((rgb >> 8U) & 0xFFU) * modifier / 255U;
    const std::uint32_t blue = (rgb & 0xFFU) * modifier / 255U;
    return 0xFF000000U | (red << 16U) | (green << 8U) | blue;
}

} // namespace basalt::server::registry::mapColors
