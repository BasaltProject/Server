#include <basalt/server/entity/Leash.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <span>

namespace basalt::server::entity::leash {
namespace {
namespace mth = wire::common::mth;
using Vec = std::array<double, 3>;

constexpr float kDegreesToRadians = static_cast<float>(3.14159265358979323846) / 180.0F;
constexpr Vec kElasticity{0.8, 0.2, 0.8};
constexpr double kStiffness = 0.11;
constexpr double kTorsion = 10.0;
constexpr double kPullAlong = 0.3;
constexpr double kQuadShare = 0.25;

constexpr std::array<Vec, 1> kSelfPoint{{{0.0, 0.5, 0.5}}};
constexpr std::array<Vec, 1> kHolderPoint{{{0.0, 0.5, 0.0}}};
constexpr std::array<Vec, 4> kQuadPoints{{{-0.5, 0.5, 0.5}, {-0.5, 0.5, -0.5}, {0.5, 0.5, -0.5}, {0.5, 0.5, 0.5}}};

Vec scaled(const Body& body, const Vec& point) {
    const auto width = static_cast<double>(body.width);
    return {point[0] * width, point[1] * static_cast<double>(body.height), point[2] * width};
}

Vec turned(const Vec& v, float radians) {
    const auto cos = static_cast<double>(mth::cos(radians));
    const auto sin = static_cast<double>(mth::sin(radians));
    return {(v[0] * cos) + (v[2] * sin), v[1], (v[2] * cos) - (v[0] * sin)};
}

Pull pullAcross(const Body& self, const Body& holder, std::span<const Vec> selfPoints,
                std::span<const Vec> holderPoints, bool quad, double slack, const Vec& selfMotion,
                const Vec& holderMotion, const Vec& selfKnown) {
    const float selfYaw = self.yaw * kDegreesToRadians;
    const float holderYaw = holder.yaw * kDegreesToRadians;
    Vec force{};
    double torque = 0.0;
    bool any = false;
    for (std::size_t at = 0; at < selfPoints.size(); ++at) {
        const Vec lever = turned(scaled(self, selfPoints[at]), -selfYaw);
        const Vec selfPos{self.position[0] + lever[0], self.position[1] + lever[1], self.position[2] + lever[2]};
        const Vec arm = turned(scaled(holder, holderPoints[at]), -holderYaw);
        const Vec pivot{holder.position[0] + arm[0], holder.position[1] + arm[1], holder.position[2] + arm[2]};
        const double dx = pivot[0] - selfPos[0];
        const double dy = pivot[1] - selfPos[1];
        const double dz = pivot[2] - selfPos[2];
        const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (distance < slack) {
            continue;
        }
        const double length = distance;
        const double weight = distance - slack;
        Vec pull{};
        if (length >= 1.0E-5) {
            pull = {dx / length * weight, dy / length * weight, dz / length * weight};
        }
        const double along = (selfMotion[0] * pull[0]) + (selfMotion[1] * pull[1]) + (selfMotion[2] * pull[2]);
        const double factor = along >= 0.0 ? kPullAlong : 1.0;
        force[0] += pull[0] * factor;
        force[1] += pull[1] * factor;
        force[2] += pull[2] * factor;
        torque += (lever[2] * pull[0]) - (lever[0] * pull[2]);
        any = true;
    }
    Pull out;
    if (!any) {
        return out;
    }
    const double share = quad ? kQuadShare : 1.0;
    out.pulled = true;
    out.torque = kTorsion * (torque * share);
    const Vec relative{holderMotion[0] - selfKnown[0], holderMotion[1] - selfKnown[1], holderMotion[2] - selfKnown[2]};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        out.push[axis] = (force[axis] * share * kElasticity[axis]) + (relative[axis] * kStiffness);
    }
    return out;
}
} // namespace

float angularFriction(bool onGround, float blockFriction, bool inLiquid) {
    if (onGround) {
        return blockFriction * 0.91F;
    }
    return inLiquid ? 0.8F : 0.91F;
}

Pull elasticPull(const Body& self, const Body& holder, bool quad, double slack, const std::array<double, 3>& selfMotion,
                 const std::array<double, 3>& holderMotion, const std::array<double, 3>& selfKnownMotion) {
    if (quad) {
        return pullAcross(self, holder, kQuadPoints, kQuadPoints, true, slack, selfMotion, holderMotion,
                          selfKnownMotion);
    }
    return pullAcross(self, holder, kSelfPoint, kHolderPoint, false, slack, selfMotion, holderMotion, selfKnownMotion);
}

} // namespace basalt::server::entity::leash
