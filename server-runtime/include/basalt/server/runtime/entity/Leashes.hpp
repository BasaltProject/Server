#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The lead between a mob or a boat and whatever holds it: a player, another mob, a boat or a knot
/// on a fence.
///
/// parity: `Leashable`, `Mob`'s and `PathfinderMob`'s leash overrides, and `LeashFenceKnotEntity`.
namespace leashes {

/// parity: `Leashable.leashSnapDistance` and `leashElasticDistance`, which the happy ghast lengthens.
inline constexpr double kSnapDistance = 12.0;
inline constexpr double kElasticDistance = 6.0;
/// parity: `Leashable.leashableInArea`, a 32 block cube around the centre.
inline constexpr double kAreaSize = 32.0;
/// parity: `LeashFenceKnotEntity.OFFSET_Y`, the knot's feet over the fence's square.
inline constexpr double kKnotOffsetY = 0.375;

/// Whether the entity is `Leashable` at all: every mob, and every boat and raft.
[[nodiscard]] bool isLeashable(const ServerEntity& entity);

/// parity: `canBeLeashed`, with `monster` the `Enemy` a mob refuses a lead for.
[[nodiscard]] bool canBeLeashed(const ServerEntity& entity, bool monster);

[[nodiscard]] double snapDistance(const ServerEntity& entity);

[[nodiscard]] double elasticDistance(const ServerEntity& entity);

/// Where something is and how it is shaped and moving, as the spring reads it.
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

/// What the rope does to the entity it pulls: the push added to its motion and the turn it gives.
struct Pull {
    std::array<double, 3> push{};
    double angularMomentum = 0.0;
};

/// parity: `checkElasticInteractions` over `computeElasticInteraction`: a damped spring from the
/// holder's middle to the entity's front, slack for its first `slack` blocks. Nothing when slack.
[[nodiscard]] std::optional<Pull> elastic(const Body& entity, const Body& holder, double slack);

/// parity: `Leashable.angularFriction`: the block's slipperiness on the ground, 0.8 in a liquid.
[[nodiscard]] float angularFriction(bool onGround, float blockFriction, bool inLiquid);

} // namespace leashes

} // namespace basalt::server::runtime::entity
