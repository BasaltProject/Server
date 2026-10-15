#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::arrowButtons {
namespace {

/// parity: `Block.boxZ(6, 4, 8, 16)` is `box(5.
constexpr double kWide = 6.0;
constexpr double kTall = 4.0;
constexpr double kDeep = 2.0;
constexpr double kDeepPressed = 1.0;
constexpr double kBlock = 16.0;

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::array<double, 2> centred(double width) { return {(kBlock - width) / 2.0, (kBlock + width) / 2.0}; }

[[nodiscard]] std::array<double, 2> against(bool high, double depth) {
    return high ? std::array<double, 2>{kBlock - depth, kBlock} : std::array<double, 2>{0.0, depth};
}

} // namespace

bool acceptsArrows(std::string_view path) {
    return path.ends_with("_button") && path != "stone_button" && path != "polished_blackstone_button";
}

std::array<double, 6> outline(std::int32_t stateId) {
    const std::string face = propertyOf(stateId, "face");
    const std::string facing = propertyOf(stateId, "facing");
    const double depth = propertyOf(stateId, "powered") == "true" ? kDeepPressed : kDeep;
    const bool alongX = facing == "east" || facing == "west";
    if (face == "floor" || face == "ceiling") {
        const std::array<double, 2> x = centred(alongX ? kTall : kWide);
        const std::array<double, 2> z = centred(alongX ? kWide : kTall);
        const std::array<double, 2> y = against(face == "ceiling", depth);
        return {x[0], y[0], z[0], x[1], y[1], z[1]};
    }
    const std::array<double, 2> y = centred(kTall);
    if (alongX) {
        const std::array<double, 2> x = against(facing == "west", depth);
        const std::array<double, 2> z = centred(kWide);
        return {x[0], y[0], z[0], x[1], y[1], z[1]};
    }
    const std::array<double, 2> x = centred(kWide);
    const std::array<double, 2> z = against(facing == "north", depth);
    return {x[0], y[0], z[0], x[1], y[1], z[1]};
}

bool reaches(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, double entityX, double entityY,
             double entityZ, double halfWidth, double height) {
    const std::array<double, 6> box = outline(stateId);
    // parity: `AABB.intersects`, which is strict at both ends: touching is not reaching.
    const auto overlaps = [](double lowA, double highA, double lowB, double highB) {
        return lowA < highB && highA > lowB;
    };
    return overlaps(entityX - halfWidth, entityX + halfWidth, x + (box[0] / kBlock), x + (box[3] / kBlock)) &&
           overlaps(entityY, entityY + height, y + (box[1] / kBlock), y + (box[4] / kBlock)) &&
           overlaps(entityZ - halfWidth, entityZ + halfWidth, z + (box[2] / kBlock), z + (box[5] / kBlock));
}

bool heldByArrow(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    for (const entity::ServerEntity* arrow : world.entities()) {
        if (arrow == nullptr || arrow->removed || arrow->type().path() != "arrow") {
            continue;
        }
        const double halfWidth = registry::entityTypeShapes::widthOf(arrow->type()) / 2.0;
        const double height = registry::entityTypeShapes::heightOf(arrow->type());
        if (reaches(x, y, z, stateId, arrow->x, arrow->y, arrow->z, halfWidth, height)) {
            return true;
        }
    }
    return false;
}

} // namespace basalt::server::runtime::world::arrowButtons
