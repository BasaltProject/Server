#include <basalt/server/runtime/entity/FlyingPathFinder.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>

namespace basalt::server::runtime::entity::flyingPathFinder {
namespace {

/// parity: `PathFinder.FUDGING`.
constexpr float kFudging = 1.5F;

/// parity: `BlockPos.asLong`, twenty six bits across, twelve up.
[[nodiscard]] std::int64_t keyOf(const Node& at) {
    constexpr std::uint64_t kXz = (1ULL << 26) - 1;
    constexpr std::uint64_t kY = (1ULL << 12) - 1;
    return static_cast<std::int64_t>(((static_cast<std::uint64_t>(at[0]) & kXz) << 38) |
                                     ((static_cast<std::uint64_t>(at[2]) & kXz) << 12) |
                                     (static_cast<std::uint64_t>(at[1]) & kY));
}

[[nodiscard]] float distance(const Node& a, const Node& b) {
    const auto dx = static_cast<float>(b[0] - a[0]);
    const auto dy = static_cast<float>(b[1] - a[1]);
    const auto dz = static_cast<float>(b[2] - a[2]);
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

/// What one square costs to fly through, negative when blocked. parity: `FlyNodeEvaluator.findAcceptedNode`
/// over `getPathTypeOfMob`.
class Costs {
public:
    Costs(world::WorldState& world, const Footprint& size, const Mob& mob) : world_(&world), size_(size), mob_(&mob) {}

    [[nodiscard]] float at(const Node& node) {
        const std::int64_t key = keyOf(node);
        if (const auto found = cache_.find(key); found != cache_.end()) {
            return found->second;
        }
        const pathTypes::PathType type = typeOf(node);
        float cost = pathTypes::malusOf(mob_->maluses, type);
        // parity: a walkable square costs a point more to a flier.
        if (cost >= 0.0F && type == pathTypes::PathType::Walkable) {
            cost += 1.0F;
        }
        cache_.emplace(key, cost);
        return cost;
    }

private:
    [[nodiscard]] pathTypes::PathType single(std::int32_t x, std::int32_t y, std::int32_t z) const {
        pathTypes::PathType type = pathTypes::flying(*world_, x, y, z, mob_->minY, mob_->at);
        // parity: `getPathTypeWithinMobBB`: a rail it is not already on cannot be crossed.
        if (type == pathTypes::PathType::Rail && mob_->at.has_value()) {
            const Node& at = *mob_->at;
            if (pathTypes::flying(*world_, at[0], at[1], at[2], mob_->minY, mob_->at) != pathTypes::PathType::Rail &&
                pathTypes::flying(*world_, at[0], at[1] - 1, at[2], mob_->minY, mob_->at) !=
                    pathTypes::PathType::Rail) {
                type = pathTypes::PathType::UnpassableRail;
            }
        }
        return type;
    }

    /// parity: `WalkNodeEvaluator.getPathTypeOfMob`.
    [[nodiscard]] pathTypes::PathType typeOf(const Node& node) const {
        std::array<bool, pathTypes::kCount> seen{};
        std::size_t distinct = 0;
        for (std::int32_t dx = 0; dx < size_.width; ++dx) {
            for (std::int32_t dy = 0; dy < size_.height; ++dy) {
                for (std::int32_t dz = 0; dz < size_.width; ++dz) {
                    const auto index = static_cast<std::size_t>(single(node[0] + dx, node[1] + dy, node[2] + dz));
                    distinct += seen[index] ? 0 : 1;
                    seen[index] = true;
                }
            }
        }
        const auto has = [&seen](pathTypes::PathType type) { return seen[static_cast<std::size_t>(type)]; };
        if (distinct == 1) {
            for (std::size_t index = 0; index < pathTypes::kCount; ++index) {
                if (seen[index]) {
                    return static_cast<pathTypes::PathType>(index);
                }
            }
        }
        if (has(pathTypes::PathType::Fence)) {
            return pathTypes::PathType::Fence;
        }
        if (has(pathTypes::PathType::UnpassableRail)) {
            return pathTypes::PathType::UnpassableRail;
        }
        pathTypes::PathType highest = pathTypes::PathType::Blocked;
        float highestMalus = pathTypes::malusOf(mob_->maluses, highest);
        // parity: the set is an `EnumSet`, so it is walked in id order.
        for (std::size_t index = 0; index < pathTypes::kCount; ++index) {
            if (!seen[index]) {
                continue;
            }
            const auto type = static_cast<pathTypes::PathType>(index);
            const float malus = pathTypes::malusOf(mob_->maluses, type);
            if (malus < 0.0F) {
                return type;
            }
            if (malus >= highestMalus) {
                highestMalus = malus;
                highest = type;
            }
        }
        const pathTypes::PathType own = single(node[0], node[1], node[2]);
        if (size_.width > 1) {
            const bool cheaper =
                pathTypes::malusOf(mob_->maluses, own) < highestMalus &&
                pathTypes::malusOf(mob_->maluses, pathTypes::PathType::BigMobsCloseToDanger) < highestMalus;
            return cheaper ? pathTypes::PathType::BigMobsCloseToDanger : highest;
        }
        return own == pathTypes::PathType::Open && highest != pathTypes::PathType::Open && highestMalus == 0.0F
                   ? pathTypes::PathType::Open
                   : highest;
    }

    world::WorldState* world_;
    Footprint size_;
    const Mob* mob_;
    std::unordered_map<std::int64_t, float> cache_;
};

struct Visit {
    Node at;
    float g = 0.0F;
    float h = 0.0F;
    std::optional<std::int64_t> from;
    bool closed = false;
};

} // namespace

std::vector<Node> find(world::WorldState& world, const Footprint& size, const Node& from, const Node& to,
                       std::int32_t maxNodes, double reach, const Mob& mob) {
    Costs costs{world, size, mob};
    std::unordered_map<std::int64_t, Visit> visits;
    using Entry = std::pair<float, std::int64_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    const std::int64_t start = keyOf(from);
    visits.emplace(start, Visit{.at = from, .h = distance(from, to)});
    open.emplace(distance(from, to) * kFudging, start);
    std::int64_t best = start;
    std::int32_t opened = 0;
    std::optional<std::int64_t> reached;
    while (!open.empty() && opened < maxNodes) {
        const std::int64_t current = open.top().second;
        open.pop();
        Visit& visit = visits.at(current);
        if (visit.closed) {
            continue;
        }
        visit.closed = true;
        ++opened;
        // parity: `Target` reached within one square, as `moveTo` asks for.
        if (std::abs(visit.at[0] - to[0]) + std::abs(visit.at[1] - to[1]) + std::abs(visit.at[2] - to[2]) <= 1) {
            reached = current;
            break;
        }
        const Visit& bestVisit = visits.at(best);
        if (visit.h < bestVisit.h || (visit.h == bestVisit.h && visit.g < bestVisit.g)) {
            best = current;
        }
        const Node here = visit.at;
        const float g = visit.g;
        const auto open1 = [&costs](const Node& node) { return costs.at(node) >= 0.0F; };
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dz = -1; dz <= 1; ++dz) {
                    if (dx == 0 && dy == 0 && dz == 0) {
                        continue;
                    }
                    const Node next{here[0] + dx, here[1] + dy, here[2] + dz};
                    if (!open1(next)) {
                        continue;
                    }
                    // parity: a diagonal only past the squares it cuts the corner of.
                    bool corners = true;
                    for (std::int32_t mask = 1; mask < 7 && corners; ++mask) {
                        const Node corner{here[0] + ((mask & 1) != 0 ? dx : 0), here[1] + ((mask & 2) != 0 ? dy : 0),
                                          here[2] + ((mask & 4) != 0 ? dz : 0)};
                        if (corner != next && corner != here) {
                            corners = open1(corner);
                        }
                    }
                    if (!corners || static_cast<double>(distance(from, next)) > reach) {
                        continue;
                    }
                    const float cost = g + distance(here, next) + costs.at(next);
                    auto [found, inserted] =
                        visits.try_emplace(keyOf(next), Visit{.at = next, .h = distance(next, to)});
                    Visit& neighbour = found->second;
                    if (neighbour.closed || (!inserted && cost >= neighbour.g)) {
                        continue;
                    }
                    neighbour.g = cost;
                    neighbour.from = current;
                    open.emplace(cost + (neighbour.h * kFudging), found->first);
                }
            }
        }
    }
    std::vector<Node> path;
    for (std::optional<std::int64_t> at = reached.value_or(best); at.has_value() && *at != start;
         at = visits.at(*at).from) {
        path.push_back(visits.at(*at).at);
    }
    std::ranges::reverse(path);
    return path;
}

} // namespace basalt::server::runtime::entity::flyingPathFinder
