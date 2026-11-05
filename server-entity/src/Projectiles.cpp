#include <basalt/server/entity/Projectiles.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>

namespace basalt::server::entity::projectile {
namespace {
namespace mth = wire::common::mth;

constexpr double kRadiansToDegrees = 57.29577951308232;
constexpr float kMaxMargin = 0.3F;

struct AtanTables {
    std::array<double, 257> asin{};
    std::array<double, 257> cos{};

    AtanTables() {
        for (std::size_t index = 0; index < asin.size(); ++index) {
            asin[index] = std::asin(static_cast<double>(index) / 256.0);
            cos[index] = std::cos(asin[index]);
        }
    }
};

float javaRound(float value) { return std::floor(value + 0.5F); }

constexpr std::array<std::array<int, 3>, 6> kSteps{
    {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
} // namespace

double atan2(double y, double x) {
    static const AtanTables kTables;
    const double squared = (x * x) + (y * y);
    if (std::isnan(squared)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const bool negY = y < 0.0;
    if (negY) {
        y = -y;
    }
    const bool negX = x < 0.0;
    if (negX) {
        x = -x;
    }
    const bool steep = y > x;
    if (steep) {
        std::swap(x, y);
    }
    const double inverse = mth::fastInvSqrt(squared);
    x *= inverse;
    y *= inverse;
    constexpr double kFracBias = 17592186044416.0;
    const double biased = kFracBias + y;
    const auto index = static_cast<std::size_t>(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(biased)));
    const double sinPhi = biased - kFracBias;
    const double residual = (y * kTables.cos[index]) - (x * sinPhi);
    double theta = kTables.asin[index] + ((6.0 + (residual * residual)) * residual * 0.16666666666666666);
    if (steep) {
        theta = (std::numbers::pi / 2) - theta;
    }
    if (negX) {
        theta = std::numbers::pi - theta;
    }
    return negY ? -theta : theta;
}

std::optional<Rotation> rotateTowardsMovement(double motionX, double motionY, double motionZ, float xRotO, float yRotO,
                                              float xRot, float yRot, float speed) {
    if ((motionX * motionX) + (motionY * motionY) + (motionZ * motionZ) == 0.0) {
        return std::nullopt;
    }
    const auto yaw = static_cast<float>(atan2(motionZ, motionX) * kRadiansToDegrees) + 90.0F;
    const auto pitch =
        static_cast<float>(atan2(std::sqrt((motionX * motionX) + (motionZ * motionZ)), motionY) * kRadiansToDegrees) -
        90.0F;
    Rotation out{xRotO, yRotO, xRot, yRot};
    out.xRotO += 360.0F * javaRound((pitch - out.xRotO) / 360.0F);
    out.yRotO += 360.0F * javaRound((yaw - out.yRotO) / 360.0F);
    out.xRot = mth::lerp(speed, out.xRotO, pitch);
    out.yRot = mth::lerp(speed, out.yRotO, yaw);
    return out;
}

Hit closestHit(std::span<const Candidate> candidates, const geometry::Point& from, const geometry::Point& to,
               float margin) {
    Hit best;
    double bestDistance = std::numeric_limits<double>::max();
    for (std::size_t at = 0; at < candidates.size(); ++at) {
        const auto entry = geometry::clip(geometry::inflate(candidates[at].box, static_cast<double>(margin)), from, to);
        if (entry.has_value() && geometry::distanceSquared(from, *entry) < bestDistance) {
            bestDistance = geometry::distanceSquared(from, *entry);
            best = {static_cast<std::int32_t>(at), *entry};
        }
    }
    return best;
}

Hit pickedHit(std::span<const Candidate> candidates, std::int32_t exceptRootVehicle, const geometry::Point& from,
              const geometry::Point& to, double maxValue) {
    Hit best;
    double limit = maxValue;
    for (std::size_t at = 0; at < candidates.size(); ++at) {
        const Candidate& candidate = candidates[at];
        const geometry::Box grown = geometry::inflate(candidate.box, static_cast<double>(candidate.pickRadius));
        const auto entry = geometry::clip(grown, from, to);
        if (geometry::contains(grown, from)) {
            if (limit >= 0.0 && candidate.pickableFromInside) {
                best = {static_cast<std::int32_t>(at), entry.value_or(from)};
                limit = 0.0;
            }
            continue;
        }
        if (!entry.has_value()) {
            continue;
        }
        const double distance = geometry::distanceSquared(from, *entry);
        if (distance >= limit && limit != 0.0) {
            continue;
        }
        const bool sameVehicle = candidate.rootVehicle == exceptRootVehicle;
        if (!sameVehicle || limit == 0.0) {
            best = {static_cast<std::int32_t>(at), *entry};
            if (!sameVehicle) {
                limit = distance;
            }
        }
    }
    return best;
}

float margin(std::int32_t tickCount) { return mth::clamp(static_cast<float>(tickCount - 2) / 20.0F, 0.0F, kMaxMargin); }

geometry::Box shulkerProgressBox(float size, std::int32_t direction, float progressFrom, float progressTo,
                                 const geometry::Point& position) {
    const std::array<int, 3>& step = kSteps[static_cast<std::size_t>(std::clamp(direction, 0, 5))];
    const auto extent = static_cast<double>(size);
    const double half = extent * 0.5;
    const geometry::Box base = geometry::makeBox(-half, 0.0, -half, half, extent, half);
    const auto far = static_cast<double>(std::max(progressFrom, progressTo));
    const auto near = static_cast<double>(std::min(progressFrom, progressTo));
    const double sx = step[0];
    const double sy = step[1];
    const double sz = step[2];
    const geometry::Box reach = geometry::expandTowards(base, sx * far * extent, sy * far * extent, sz * far * extent);
    return geometry::move(geometry::contract(reach, -sx * (1.0 + near) * extent, -sy * (1.0 + near) * extent,
                                             -sz * (1.0 + near) * extent),
                          position[0], position[1], position[2]);
}

} // namespace basalt::server::entity::projectile
