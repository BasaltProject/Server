#pragma once

#include "WorldBlockUpdates.hpp"

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::plugins::itemUse {

struct Click {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t face = 1;
    float yaw = 0.0F;
    float pitch = 0.0F;
    float clickX = 0.5F;
    float clickY = 0.5F;
    float clickZ = 0.5F;
    bool inside = false;
};

/// parity: `BlockItem.place` with no player holding the item; returns blocks put down.
std::int32_t placeBlockItem(const blockUpdates::Target& target, std::string_view item, const Click& click);

} // namespace basalt::server::runtime::plugins::itemUse
