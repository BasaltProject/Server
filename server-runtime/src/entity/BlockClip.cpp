#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::blockRay {
namespace {

/// A box inside its block, as min x, y, z then max x, y, z in blocks.
using Box = std::array<double, 6>;

constexpr double kUnits = registry::blockShapes::kUnits;
/// parity: `Traverse` nudges both ends of the line this far past each other.
constexpr double kNudge = -1.0E-7;
/// parity: `AABB.clip`'s slack on a face's edges and the smallest step it counts on an axis.
constexpr double kEdge = 1.0E-7;
/// parity: `EntityCollisionContext.isAbove`, a float 1.0E-5.
constexpr double kAboveSlack = static_cast<double>(1.0E-5F);

constexpr Box kFullBlock{0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
/// parity: `ScaffoldingBlock.SHAPE_STABLE`: the top two sixteenths and the four corner legs.
constexpr std::array<Box, 5> kScaffoldStable{{{0.0, 14.0 / 16.0, 0.0, 1.0, 1.0, 1.0},
                                              {0.0, 0.0, 0.0, 2.0 / 16.0, 1.0, 2.0 / 16.0},
                                              {14.0 / 16.0, 0.0, 0.0, 1.0, 1.0, 2.0 / 16.0},
                                              {0.0, 0.0, 14.0 / 16.0, 2.0 / 16.0, 1.0, 1.0},
                                              {14.0 / 16.0, 0.0, 14.0 / 16.0, 1.0, 1.0, 1.0}}};
/// parity: `ScaffoldingBlock.SHAPE_UNSTABLE_BOTTOM`.
constexpr Box kScaffoldBottom{0.0, 0.0, 0.0, 1.0, 2.0 / 16.0, 1.0};
/// parity: `PowderSnowBlock.FALLING_COLLISION_SHAPE`, nine tenths as a float.
constexpr Box kPowderSnowFalling{0.0, 0.0, 0.0, 1.0, static_cast<double>(0.9F), 1.0};

/// The blocks whose shape is not simply their collision table entry for this clip.
enum class Special : std::uint8_t { None, Empty, Full, Fence, SnowLayers, Scaffolding, PowderSnow };

[[nodiscard]] Special specialOf(std::int32_t stateId, ClipShape kind) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return Special::None;
    }
    const std::string_view path = block->path();
    if (kind == ClipShape::Visual) {
        // parity: `TransparentBlock`, `IronBarsBlock` and `PowderSnowBlock` see through; `SoulSandBlock`
        // and `MudBlock` a whole block; `FenceBlock` its outline; `SnowLayerBlock` all its layers.
        if (path == "glass" || path == "tinted_glass" || path.ends_with("_stained_glass") ||
            path.ends_with("copper_grate") || path == "iron_bars" || path.ends_with("copper_bars") ||
            path == "glass_pane" || path.ends_with("_stained_glass_pane") || path == "powder_snow") {
            return Special::Empty;
        }
        if (path == "soul_sand" || path == "mud") {
            return Special::Full;
        }
        if (path.ends_with("_fence")) {
            return Special::Fence;
        }
        if (path == "snow") {
            return Special::SnowLayers;
        }
    }
    if (path == "scaffolding") {
        return Special::Scaffolding;
    }
    if (path == "powder_snow") {
        return Special::PowderSnow;
    }
    return Special::None;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: `EntityCollisionContext.isAbove`.
[[nodiscard]] bool isAbove(const ClipViewer& viewer, std::int32_t blockY, double shapeTop) {
    return viewer.feetY > static_cast<double>(blockY) + shapeTop - kAboveSlack;
}

/// Hands `visit` every box of the block's shape for this clip, and answers whether any box answered true.
template <typename Visit>
bool anyBox(std::int32_t stateId, std::int32_t blockY, ClipShape kind, const ClipViewer& viewer, Visit&& visit) {
    switch (specialOf(stateId, kind)) {
    case Special::Empty:
        return false;
    case Special::Full:
        return visit(kFullBlock);
    case Special::SnowLayers: {
        // parity: `SnowLayerBlock.SHAPES[layers]`, two sixteenths a layer.
        const std::string layers = propertyOf(stateId, "layers");
        const std::int32_t count = layers.empty() ? 1 : layers[0] - '0';
        return count > 0 && visit(Box{0.0, 0.0, 0.0, 1.0, count * 2.0 / 16.0, 1.0});
    }
    case Special::Scaffolding: {
        // parity: `ScaffoldingBlock.getCollisionShape`, which is also its visual shape.
        if (isAbove(viewer, blockY, 1.0) && !viewer.descending) {
            for (const Box& box : kScaffoldStable) {
                if (visit(box)) {
                    return true;
                }
            }
            return false;
        }
        return propertyOf(stateId, "distance") != "0" && propertyOf(stateId, "bottom") == "true" &&
               isAbove(viewer, blockY, 0.0) && visit(kScaffoldBottom);
    }
    case Special::PowderSnow:
        if (viewer.fallDistance > 2.5) {
            return visit(kPowderSnowFalling);
        }
        return viewer.walksOnPowderSnow && isAbove(viewer, blockY, 1.0) && !viewer.descending && visit(kFullBlock);
    case Special::Fence:
    case Special::None:
        break;
    }
    // parity: a fence's outline is its collision box cut down to the block itself.
    const bool outline = specialOf(stateId, kind) == Special::Fence;
    const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(stateId);
    for (std::size_t at = 0; at + 5 < boxes.size(); at += registry::blockShapes::kCorners) {
        const double top = outline ? std::min(boxes[at + 4], registry::blockShapes::kUnits) : boxes[at + 4];
        if (visit(Box{boxes[at] / kUnits, boxes[at + 1] / kUnits, boxes[at + 2] / kUnits, boxes[at + 3] / kUnits,
                      top / kUnits, boxes[at + 5] / kUnits})) {
            return true;
        }
    }
    return false;
}

/// parity: `AABB.clipPoint`, one face against the line, with the scale that has to be beaten.
[[nodiscard]] bool clipPoint(double& scale, double da, double db, double dc, double point, double minB, double maxB,
                             double minC, double maxC, double fromA, double fromB, double fromC) {
    const double s = (point - fromA) / da;
    const double pb = fromB + (s * db);
    const double pc = fromC + (s * dc);
    if (0.0 < s && s < scale && minB - kEdge < pb && pb < maxB + kEdge && minC - kEdge < pc && pc < maxC + kEdge) {
        scale = s;
        return true;
    }
    return false;
}

/// parity: `AABB.getDirection`: the faces of the box facing the line, each against the scale so far.
[[nodiscard]] bool clipBox(const Box& box, const std::array<double, 3>& from, const std::array<double, 3>& delta,
                           double& scale) {
    const auto [minX, minY, minZ, maxX, maxY, maxZ] = box;
    const auto [dx, dy, dz] = delta;
    bool hit = false;
    if (dx > kEdge) {
        hit = clipPoint(scale, dx, dy, dz, minX, minY, maxY, minZ, maxZ, from[0], from[1], from[2]) || hit;
    } else if (dx < -kEdge) {
        hit = clipPoint(scale, dx, dy, dz, maxX, minY, maxY, minZ, maxZ, from[0], from[1], from[2]) || hit;
    }
    if (dy > kEdge) {
        hit = clipPoint(scale, dy, dz, dx, minY, minZ, maxZ, minX, maxX, from[1], from[2], from[0]) || hit;
    } else if (dy < -kEdge) {
        hit = clipPoint(scale, dy, dz, dx, maxY, minZ, maxZ, minX, maxX, from[1], from[2], from[0]) || hit;
    }
    if (dz > kEdge) {
        hit = clipPoint(scale, dz, dx, dy, minZ, minX, maxX, minY, maxY, from[2], from[0], from[1]) || hit;
    } else if (dz < -kEdge) {
        hit = clipPoint(scale, dz, dx, dy, maxZ, minX, maxX, minY, maxY, from[2], from[0], from[1]) || hit;
    }
    return hit;
}

/// parity: `VoxelShape.clip`: a line starting inside the shape hits it where it starts, else
/// `AABB.clip` over its boxes.
[[nodiscard]] bool shapeClips(std::int32_t stateId, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                              const std::array<double, 3>& from, const std::array<double, 3>& to, ClipShape kind,
                              const ClipViewer& viewer) {
    const std::array<double, 3> delta{to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    if ((delta[0] * delta[0]) + (delta[1] * delta[1]) + (delta[2] * delta[2]) < 1.0E-7) {
        return false;
    }
    // parity: `isFullWide` at the point a thousandth of the way along, which a box holds half-open.
    const double testX = from[0] + (delta[0] * 0.001) - blockX;
    const double testY = from[1] + (delta[1] * 0.001) - blockY;
    const double testZ = from[2] + (delta[2] * 0.001) - blockZ;
    if (anyBox(stateId, blockY, kind, viewer, [&](const Box& box) {
            return box[0] <= testX && testX < box[3] && box[1] <= testY && testY < box[4] && box[2] <= testZ &&
                   testZ < box[5];
        })) {
        return true;
    }
    double scale = 1.0;
    return anyBox(stateId, blockY, kind, viewer, [&](const Box& box) {
        const Box moved{box[0] + blockX, box[1] + blockY, box[2] + blockZ,
                        box[3] + blockX, box[4] + blockY, box[5] + blockZ};
        return clipBox(moved, from, delta, scale);
    });
}

[[nodiscard]] std::int32_t sign(double value) {
    if (value == 0.0) {
        return 0;
    }
    return value > 0.0 ? 1 : -1;
}

[[nodiscard]] double frac(double value) { return value - std::floor(value); }

} // namespace

bool clips(world::WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY, double toZ,
           ClipShape shape, const ClipViewer& viewer) {
    if (fromX == toX && fromY == toY && fromZ == toZ) {
        return false;
    }
    const std::array<double, 3> from{fromX, fromY, fromZ};
    const std::array<double, 3> to{toX, toY, toZ};
    const auto hitsAt = [&](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
        return stateId.has_value() && shapeClips(*stateId, x, y, z, from, to, shape, viewer);
    };
    // parity: `BlockGetter.traverseBlocks`, `Mth.lerp(-1.0E-7, ...)` on both ends.
    const double endX = toX + (kNudge * (fromX - toX));
    const double endY = toY + (kNudge * (fromY - toY));
    const double endZ = toZ + (kNudge * (fromZ - toZ));
    const double startX = fromX + (kNudge * (toX - fromX));
    const double startY = fromY + (kNudge * (toY - fromY));
    const double startZ = fromZ + (kNudge * (toZ - fromZ));
    auto blockX = static_cast<std::int32_t>(std::floor(startX));
    auto blockY = static_cast<std::int32_t>(std::floor(startY));
    auto blockZ = static_cast<std::int32_t>(std::floor(startZ));
    if (hitsAt(blockX, blockY, blockZ)) {
        return true;
    }
    const double dx = endX - startX;
    const double dy = endY - startY;
    const double dz = endZ - startZ;
    const std::int32_t signX = sign(dx);
    const std::int32_t signY = sign(dy);
    const std::int32_t signZ = sign(dz);
    constexpr double kFar = std::numeric_limits<double>::max();
    const double deltaX = signX == 0 ? kFar : signX / dx;
    const double deltaY = signY == 0 ? kFar : signY / dy;
    const double deltaZ = signZ == 0 ? kFar : signZ / dz;
    double tX = deltaX * (signX > 0 ? 1.0 - frac(startX) : frac(startX));
    double tY = deltaY * (signY > 0 ? 1.0 - frac(startY) : frac(startY));
    double tZ = deltaZ * (signZ > 0 ? 1.0 - frac(startZ) : frac(startZ));
    while (tX <= 1.0 || tY <= 1.0 || tZ <= 1.0) {
        if (tX < tY) {
            if (tX < tZ) {
                blockX += signX;
                tX += deltaX;
            } else {
                blockZ += signZ;
                tZ += deltaZ;
            }
        } else if (tY < tZ) {
            blockY += signY;
            tY += deltaY;
        } else {
            blockZ += signZ;
            tZ += deltaZ;
        }
        if (hitsAt(blockX, blockY, blockZ)) {
            return true;
        }
    }
    return false;
}

} // namespace basalt::server::runtime::entity::blockRay
