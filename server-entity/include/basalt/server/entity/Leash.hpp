#pragma once

#include <array>

namespace basalt::server::entity::leash {

/// parity: `Leashable.angularFriction`.
[[nodiscard]] float angularFriction(bool onGround, float blockFriction, bool inLiquid);

struct Body {
    std::array<double, 3> position{};
    float width = 0.0F;
    float height = 0.0F;
    float yaw = 0.0F;
};

struct Pull {
    bool pulled = false;
    double torque = 0.0;
    std::array<double, 3> push{};
};

/// parity: `Leashable.checkElasticInteractions`; the motions are the ones the entities know, zero for a mob without AI.
[[nodiscard]] Pull elasticPull(const Body& self, const Body& holder, bool quad, double slack,
                               const std::array<double, 3>& selfMotion, const std::array<double, 3>& holderMotion,
                               const std::array<double, 3>& selfKnownMotion);

} // namespace basalt::server::entity::leash
