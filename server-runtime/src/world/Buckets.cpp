#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world::buckets {
namespace {

/// parity: `Player.blockInteractionRange`, the 4.5 blocks a hand reaches, 5 in creative.
constexpr double kReach = 4.5;
constexpr double kCreativeReach = 5.0;
constexpr double kStep = 0.05;
constexpr double kPiOver180 = 0.017453292519943295;

struct Hit {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t faceX = 0;
    std::int32_t faceY = 0;
    std::int32_t faceZ = 0;

    [[nodiscard]] Hit offset() const {
        return Hit{.x = x + faceX, .y = y + faceY, .z = z + faceZ, .faceX = faceX, .faceY = faceY, .faceZ = faceZ};
    }
};

/// parity: `BlockState.canBeReplaced(Fluid)`: whatever gives way to a placed block, plus anything
/// that is not solid, which is what lets a bucket be emptied into water or lava.
[[nodiscard]] bool replaceable(std::int32_t stateId) {
    return registry::blockShapes::isReplaceable(stateId) || !registry::blockShapes::isSolidState(stateId);
}

/// Steps along the look vector until it meets something; no shapes here to clip against.
[[nodiscard]] std::optional<Hit> trace(WorldState& world, double eyeX, double eyeY, double eyeZ, float yaw, float pitch,
                                       double reach, bool stopAtFluid) {
    const double yawRadians = static_cast<double>(yaw) * kPiOver180;
    const double pitchRadians = static_cast<double>(pitch) * kPiOver180;
    const double dirX = -std::sin(yawRadians) * std::cos(pitchRadians);
    const double dirY = -std::sin(pitchRadians);
    const double dirZ = std::cos(yawRadians) * std::cos(pitchRadians);

    auto previousX = static_cast<std::int32_t>(std::floor(eyeX));
    auto previousY = static_cast<std::int32_t>(std::floor(eyeY));
    auto previousZ = static_cast<std::int32_t>(std::floor(eyeZ));
    double travelled = 0.0;
    while (travelled <= reach) {
        const auto x = static_cast<std::int32_t>(std::floor(eyeX + (dirX * travelled)));
        const auto y = static_cast<std::int32_t>(std::floor(eyeY + (dirY * travelled)));
        const auto z = static_cast<std::int32_t>(std::floor(eyeZ + (dirZ * travelled)));
        if (x != previousX || y != previousY || z != previousZ) {
            const std::optional<std::int32_t> state = world.blockState(x, y, z);
            if (!state.has_value()) {
                return std::nullopt;
            }
            const std::optional<FluidKind> fluid = fluidFlow::kindOf(*state);
            const bool solid = !registry::blocks::isAir(*state) && !fluid.has_value() &&
                               server::world::heightmapMatches(server::world::HeightmapType::MotionBlocking, *state);
            if (solid || (stopAtFluid && fluid.has_value())) {
                return Hit{
                    .x = x, .y = y, .z = z, .faceX = previousX - x, .faceY = previousY - y, .faceZ = previousZ - z};
            }
            previousX = x;
            previousY = y;
            previousZ = z;
        }
        travelled += kStep;
    }
    return std::nullopt;
}

/// parity: `BucketItem.emptyContents`: onto the face it hit, or into the block itself if that can
/// hold it.
[[nodiscard]] std::optional<Result> pour(const fluidFlow::Context& context, const Hit& hit, FluidKind content,
                                         bool creative) {
    const std::optional<std::int32_t> clicked = context.world.blockState(hit.x, hit.y, hit.z);
    if (!clicked.has_value()) {
        return std::nullopt;
    }
    const Hit target = replaceable(*clicked) ? hit : hit.offset();
    const std::optional<std::int32_t> existing = context.world.blockState(target.x, target.y, target.z);
    if (!existing.has_value() || !replaceable(*existing)) {
        return std::nullopt;
    }

    const std::int32_t source = baseStateId(content);
    if (!context.world.setBlockState(target.x, target.y, target.z, source)) {
        return std::nullopt;
    }
    Result result;
    result.changes.push_back(
        FluidChange{.x = target.x, .y = target.y, .z = target.z, .stateId = source, .washedAway = *existing});
    for (const FluidChange& change : fluidFlow::onBlockChanged(context, target.x, target.y, target.z)) {
        result.changes.push_back(change);
    }
    if (!creative) {
        result.replacement = emptyBucket();
    }
    return result;
}

/// parity: `BucketPickup.pickupBlock` on `LiquidBlock`: only a source goes into the bucket.
[[nodiscard]] std::optional<Result> fill(const fluidFlow::Context& context, const Hit& hit) {
    const std::optional<std::int32_t> state = context.world.blockState(hit.x, hit.y, hit.z);
    if (!state.has_value()) {
        return std::nullopt;
    }
    const std::optional<FluidKind> kind = fluidFlow::kindOf(*state);
    if (!kind.has_value() || *state != baseStateId(*kind)) {
        return std::nullopt;
    }
    const std::int32_t empty = registry::blocks::air().stateId;
    if (!context.world.setBlockState(hit.x, hit.y, hit.z, empty)) {
        return std::nullopt;
    }
    Result result;
    result.changes.push_back(FluidChange{.x = hit.x, .y = hit.y, .z = hit.z, .stateId = empty, .washedAway = *state});
    for (const FluidChange& change : fluidFlow::onBlockChanged(context, hit.x, hit.y, hit.z)) {
        result.changes.push_back(change);
    }
    result.replacement = *kind == FluidKind::Water ? waterBucket() : lavaBucket();
    return result;
}

} // namespace

const wire::common::Identifier& emptyBucket() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("bucket");
    return instance;
}

const wire::common::Identifier& waterBucket() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("water_bucket");
    return instance;
}

const wire::common::Identifier& lavaBucket() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("lava_bucket");
    return instance;
}

std::optional<FluidKind> contentOf(const wire::common::Identifier& item) {
    if (item == waterBucket()) {
        return FluidKind::Water;
    }
    return item == lavaBucket() ? std::optional{FluidKind::Lava} : std::nullopt;
}

bool isBucket(const wire::common::Identifier& item) {
    return item == emptyBucket() || item == waterBucket() || item == lavaBucket();
}

std::optional<Result> use(const fluidFlow::Context& context, const wire::common::Identifier& item, double eyeX,
                          double eyeY, double eyeZ, float yaw, float pitch, bool creative) {
    const std::optional<FluidKind> content = contentOf(item);
    const double reach = creative ? kCreativeReach : kReach;
    const std::optional<Hit> hit = trace(context.world, eyeX, eyeY, eyeZ, yaw, pitch, reach, !content.has_value());
    if (!hit.has_value()) {
        return std::nullopt;
    }
    return content.has_value() ? pour(context, *hit, *content, creative) : fill(context, *hit);
}

std::optional<Result> useOn(const fluidFlow::Context& context, const wire::common::Identifier& item, std::int32_t x,
                            std::int32_t y, std::int32_t z, wire::protocol::play::Direction face, bool creative) {
    const Hit hit{.x = x,
                  .y = y,
                  .z = z,
                  .faceX = wire::protocol::play::normalX(face),
                  .faceY = wire::protocol::play::normalY(face),
                  .faceZ = wire::protocol::play::normalZ(face)};
    const std::optional<FluidKind> content = contentOf(item);
    return content.has_value() ? pour(context, hit, *content, creative) : fill(context, hit);
}

} // namespace basalt::server::runtime::world::buckets
