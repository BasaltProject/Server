#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `Leashable`.
namespace leashes {

/// parity: `Leashable.leashSnapDistance` and `leashElasticDistance`.
inline constexpr double kSnapDistance = 12.0;
inline constexpr double kElasticDistance = 6.0;
/// parity: `Leashable.leashableInArea`, a 32 block cube around the centre.
inline constexpr double kAreaSize = 32.0;
/// parity: `LeashFenceKnotEntity.OFFSET_Y`, the knot's feet over the fence's square.
inline constexpr double kKnotOffsetY = 0.375;

[[nodiscard]] bool isLeashable(const ServerEntity& entity);

/// parity: `canBeLeashed`, with `monster` the `Enemy` a mob refuses a lead for.
[[nodiscard]] bool canBeLeashed(const ServerEntity& entity, bool monster);

[[nodiscard]] double snapDistance(const ServerEntity& entity);

[[nodiscard]] double elasticDistance(const ServerEntity& entity);

struct Body {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    double width = 0.0;
    double height = 0.0;
    /// parity: `getKnownMovement`, what it moved last.
    std::array<double, 3> movement{};
};

/// parity: `leashDistanceTo`, box centre to box centre.
[[nodiscard]] double distance(const Body& entity, const Body& holder);

struct Pull {
    std::array<double, 3> push{};
    double angularMomentum = 0.0;
};

/// parity: `checkElasticInteractions`.
[[nodiscard]] std::optional<Pull> elastic(const Body& entity, const Body& holder, double slack);

/// parity: `Leashable.angularFriction`.
[[nodiscard]] float angularFriction(bool onGround, float blockFriction, bool inLiquid);

} // namespace leashes

} // namespace basalt::server::runtime::entity
