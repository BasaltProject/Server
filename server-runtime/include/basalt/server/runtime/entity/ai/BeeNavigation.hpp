#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity::ai {

class MobContext;

/// How a bee finds its way: a flying path forty-eight blocks long, and the random spots it wanders
/// and hops towards. The allay flies the same forty-eight blocks and shares the path.
///
/// parity: `Bee.createNavigation`, `Bee.pathfindRandomlyTowards` and `BeeWanderGoal.findPos`.
namespace beeNavigation {

using Block = std::array<std::int32_t, 3>;

/// A path worked out but not yet walked. parity: `Path` and its `canReach`.
struct Route {
    std::vector<Block> nodes;
    Block target{};
    bool reaches = false;
};

/// parity: `createPath(pos, accuracy)` with the navigation's `maxVisitedNodesMultiplier`.
[[nodiscard]] Route plan(MobContext& mob, const Block& target, std::int32_t accuracy, float visitedMultiplier = 1.0F);

/// parity: `navigation.moveTo(path, speed)`. Answers false for a path with nothing to walk.
bool follow(MobContext& mob, const Route& route, double speed);

/// parity: `Bee.pathfindRandomlyTowards`, a hop of up to six blocks the way of `target`.
void hopTowards(MobContext& mob, const Block& target);

/// parity: `BeeWanderGoal.findPos`: a hover spot ahead, or towards the hive once it is far, else one
/// in the air. The bottom centre of the chosen block.
[[nodiscard]] std::optional<std::array<double, 3>> wanderSpot(MobContext& mob, const std::optional<Block>& hive,
                                                              std::int32_t threshold);

/// parity: `AirAndWaterRandomPos.getPos` along the way the mob faces, which the allay's
/// `RandomStroll.fly` flies to as well.
[[nodiscard]] std::optional<std::array<double, 3>> airAndWaterSpot(MobContext& mob, std::int32_t horizontal,
                                                                   std::int32_t vertical, std::int32_t lift);

/// parity: `Level.findBlocksInBoxByManhattanDistance(origin, reach)`, nearest shells first.
template <typename Visit>
std::optional<Block> firstByManhattan(const Block& origin, std::int32_t reach, Visit visit) {
    const std::int32_t maxDepth = reach * 3;
    for (std::int32_t depth = 0; depth <= maxDepth; ++depth) {
        const std::int32_t maxX = depth < reach ? depth : reach;
        for (std::int32_t x = -maxX; x <= maxX; ++x) {
            const std::int32_t restX = depth - (x < 0 ? -x : x);
            const std::int32_t maxY = restX < reach ? restX : reach;
            for (std::int32_t y = -maxY; y <= maxY; ++y) {
                const std::int32_t z = restX - (y < 0 ? -y : y);
                if (z > reach) {
                    continue;
                }
                const Block first{origin[0] + x, origin[1] + y, origin[2] + z};
                if (visit(first)) {
                    return first;
                }
                if (z != 0) {
                    const Block mirrored{origin[0] + x, origin[1] + y, origin[2] - z};
                    if (visit(mirrored)) {
                        return mirrored;
                    }
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace beeNavigation

} // namespace basalt::server::runtime::entity::ai
