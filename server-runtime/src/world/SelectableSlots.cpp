#include <basalt/server/runtime/world/SelectableSlots.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::world::selectableSlots {
namespace {

/// parity: `getSection`, which counts in sixteenths of a block however many slots there are.
[[nodiscard]] std::int32_t section(float along, std::int32_t sections) {
    const float pixel = along * 16.0F;
    const float size = 16.0F / static_cast<float>(sections);
    return std::clamp(static_cast<std::int32_t>(std::floor(pixel / size)), 0, sections - 1);
}

} // namespace

std::optional<std::int32_t> hitSlot(std::string_view facing, std::string_view face, float clickX, float clickY,
                                    float clickZ, std::int32_t rows, std::int32_t columns) {
    if (face != facing) {
        return std::nullopt;
    }
    float across = 0.0F;
    if (face == "north") {
        across = 1.0F - clickX;
    } else if (face == "south") {
        across = clickX;
    } else if (face == "west") {
        across = clickZ;
    } else if (face == "east") {
        across = 1.0F - clickZ;
    } else {
        // Up and down are not faces a shelf looks out of, so nothing was clicked.
        return std::nullopt;
    }
    const std::int32_t row = section(1.0F - clickY, rows);
    const std::int32_t column = section(across, columns);
    return column + row * columns;
}

} // namespace basalt::server::runtime::world::selectableSlots
