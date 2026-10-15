#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobCombat {

[[nodiscard]] double attackReach();

inline constexpr std::int32_t kAttackInterval = 20;

/// parity: `NearestAttackableTargetGoal`'s own one-in-ten roll before it even looks.
inline constexpr std::int32_t kTargetInterval = 10;

[[nodiscard]] bool isMelee(const wire::common::Identifier& type);

[[nodiscard]] bool needsDarkness(const wire::common::Identifier& type);

struct Target {
    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double width = 0.0;
    double height = 0.0;
};

/// parity: `NearestAttackableTargetGoal.canUse`.
[[nodiscard]] const Target* acquire(const ServerEntity& mob, const std::vector<Target>& targets,
                                    random::RandomSource& random, std::int32_t light);

bool tick(world::WorldState& world, ServerEntity& mob, const Target& target, random::RandomSource& random,
          double speed);

[[nodiscard]] bool withinReach(const ServerEntity& mob, const Target& target);

[[nodiscard]] std::array<std::int32_t, 3> blockPosition(const ServerEntity& mob);

} // namespace mobCombat

} // namespace basalt::server::runtime::entity
