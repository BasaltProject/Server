#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/portal/PortalShape.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace basalt::server::world::portal {
namespace {

[[nodiscard]] std::int32_t defaultStateOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? -1 : found->stateId;
}

[[nodiscard]] std::int32_t obsidian() {
    static const std::int32_t stateId = defaultStateOf("obsidian");
    return stateId;
}

[[nodiscard]] std::int32_t fire() {
    static const std::int32_t stateId = defaultStateOf("fire");
    return stateId;
}

[[nodiscard]] std::int32_t soulFire() {
    static const std::int32_t stateId = defaultStateOf("soul_fire");
    return stateId;
}

/// parity: `PortalShape.isEmpty`: air, any fire, or a portal block that is already there.
[[nodiscard]] bool isEmpty(std::int32_t state) {
    return registry::blocks::isAir(state) || state == fire() || state == soulFire() || PortalShape::isPortal(state);
}

/// How far the empty floor runs before the frame turns a corner. parity: same name in vanilla.
[[nodiscard]] std::int32_t distanceUntilEdgeAboveFrame(const BlockStateView& view, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::int32_t stepX, std::int32_t stepZ) {
    for (std::int32_t distance = 0; distance <= PortalShape::kMaxWidth; ++distance) {
        const std::int32_t atX = x + stepX * distance;
        const std::int32_t atZ = z + stepZ * distance;
        const std::int32_t state = view(atX, y, atZ);
        if (!isEmpty(state)) {
            return state == obsidian() ? distance : 0;
        }
        if (view(atX, y - 1, atZ) != obsidian()) {
            return 0;
        }
    }
    return 0;
}

struct Bottom {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

/// Falls to the floor of the empty column, then walks left until the frame stops holding it.
[[nodiscard]] std::optional<Bottom> calculateBottomLeft(const BlockStateView& view, PortalAxis axis, std::int32_t x,
                                                        std::int32_t y, std::int32_t z, std::int32_t worldMinY) {
    const std::int32_t minY = std::max(worldMinY, y - PortalShape::kMaxHeight);
    std::int32_t bottomY = y;
    while (bottomY > minY && isEmpty(view(x, bottomY - 1, z))) {
        --bottomY;
    }
    const std::int32_t edge = distanceUntilEdgeAboveFrame(view, x, bottomY, z, -rightX(axis), -rightZ(axis)) - 1;
    if (edge < 0) {
        return std::nullopt;
    }
    return Bottom{.x = x + -rightX(axis) * edge, .y = bottomY, .z = z + -rightZ(axis) * edge};
}

[[nodiscard]] std::int32_t calculateWidth(const BlockStateView& view, PortalAxis axis, const Bottom& bottomLeft) {
    const std::int32_t width =
        distanceUntilEdgeAboveFrame(view, bottomLeft.x, bottomLeft.y, bottomLeft.z, rightX(axis), rightZ(axis));
    return width >= PortalShape::kMinWidth && width <= PortalShape::kMaxWidth ? width : 0;
}

[[nodiscard]] std::int32_t calculateHeight(const BlockStateView& view, PortalAxis axis, const Bottom& bottomLeft,
                                           std::int32_t width, std::int32_t& portalBlocks) {
    std::int32_t counted = 0;
    std::int32_t height = PortalShape::kMaxHeight;
    for (std::int32_t row = 0; row < PortalShape::kMaxHeight; ++row) {
        const std::int32_t y = bottomLeft.y + row;
        if (view(bottomLeft.x - rightX(axis), y, bottomLeft.z - rightZ(axis)) != obsidian()) {
            height = row;
            break;
        }
        if (view(bottomLeft.x + rightX(axis) * width, y, bottomLeft.z + rightZ(axis) * width) != obsidian()) {
            height = row;
            break;
        }
        bool blocked = false;
        for (std::int32_t column = 0; column < width; ++column) {
            const std::int32_t state =
                view(bottomLeft.x + rightX(axis) * column, y, bottomLeft.z + rightZ(axis) * column);
            if (!isEmpty(state)) {
                height = row;
                blocked = true;
                break;
            }
            if (PortalShape::isPortal(state)) {
                ++counted;
            }
        }
        if (blocked) {
            break;
        }
    }
    portalBlocks = counted;
    if (height < PortalShape::kMinHeight || height > PortalShape::kMaxHeight) {
        return height;
    }
    for (std::int32_t column = 0; column < width; ++column) {
        const std::int32_t topX = bottomLeft.x + rightX(axis) * column;
        const std::int32_t topZ = bottomLeft.z + rightZ(axis) * column;
        if (view(topX, bottomLeft.y + height, topZ) != obsidian()) {
            return 0;
        }
    }
    return height;
}

} // namespace

std::vector<PortalBlock> PortalShape::interior() const {
    std::vector<PortalBlock> blocks;
    blocks.reserve(static_cast<std::size_t>(std::max(0, width_ * height_)));
    for (std::int32_t row = 0; row < height_; ++row) {
        for (std::int32_t column = 0; column < width_; ++column) {
            blocks.push_back(PortalBlock{.x = bottomLeftX_ + rightX(axis_) * column,
                                         .y = bottomLeftY_ + row,
                                         .z = bottomLeftZ_ + rightZ(axis_) * column});
        }
    }
    return blocks;
}

std::int32_t PortalShape::portalState(const PortalAxis axis) {
    const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(
        wire::common::Identifier::vanilla("nether_portal"), {{"axis", std::string{serializedName(axis)}}});
    if (!resolved.has_value()) {
        throw std::runtime_error{"nether_portal has no " + std::string{serializedName(axis)} + " state"};
    }
    return *resolved;
}

bool PortalShape::isPortal(const std::int32_t state) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(state);
    return block != nullptr && block->path() == "nether_portal";
}

std::optional<PortalAxis> PortalShape::axisOf(const std::int32_t state) {
    if (!isPortal(state)) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(state);
    const auto found = properties.find("axis");
    return found != properties.end() && found->second == "z" ? PortalAxis::Z : PortalAxis::X;
}

PortalShape PortalShape::findAnyShape(const BlockStateView& view, const std::int32_t x, const std::int32_t y,
                                      const std::int32_t z, const std::int32_t minY, const PortalAxis axis) {
    const std::optional<Bottom> bottom = calculateBottomLeft(view, axis, x, y, z, minY);
    if (!bottom.has_value()) {
        return PortalShape{axis, x, y, z, 0, 0, 0};
    }
    const std::int32_t width = calculateWidth(view, axis, *bottom);
    if (width == 0) {
        return PortalShape{axis, bottom->x, bottom->y, bottom->z, 0, 0, 0};
    }
    std::int32_t portalBlocks = 0;
    const std::int32_t height = calculateHeight(view, axis, *bottom, width, portalBlocks);
    return PortalShape{axis, bottom->x, bottom->y, bottom->z, width, height, portalBlocks};
}

std::optional<PortalShape> PortalShape::find(const BlockStateView& view, const std::int32_t x, const std::int32_t y,
                                             const std::int32_t z, const std::int32_t minY, const PortalAxis preferred,
                                             const std::function<bool(const PortalShape&)>& accept) {
    const PortalShape first = findAnyShape(view, x, y, z, minY, preferred);
    if (accept(first)) {
        return first;
    }
    const PortalShape second = findAnyShape(view, x, y, z, minY, other(preferred));
    if (accept(second)) {
        return second;
    }
    return std::nullopt;
}

std::optional<PortalShape> PortalShape::findEmpty(const BlockStateView& view, const std::int32_t x,
                                                  const std::int32_t y, const std::int32_t z, const std::int32_t minY,
                                                  const PortalAxis preferred) {
    return find(view, x, y, z, minY, preferred,
                [](const PortalShape& shape) { return shape.isValid() && shape.portalBlocks() == 0; });
}

} // namespace basalt::server::world::portal
