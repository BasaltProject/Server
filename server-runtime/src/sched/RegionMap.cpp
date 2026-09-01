#include <basalt/server/runtime/sched/RegionMap.hpp>

#include <algorithm>
#include <deque>
#include <utility>

namespace basalt::server::runtime::sched::regionMap {

std::vector<std::unordered_set<std::int64_t>> components(const std::unordered_set<std::int64_t>& sections) {
    // Nothing loaded is the common answer while a world is starting or emptying, and the sort
    // below is what it saves.
    if (sections.empty()) {
        return {};
    }
    // Grown from the lowest key up rather than from wherever the hash puts its first element: which
    // region survives a merge follows from the order the groups come out in, and that must not
    // depend on how a set happened to be laid out in memory.
    std::vector<std::int64_t> seeds{sections.begin(), sections.end()};
    std::ranges::sort(seeds);

    std::unordered_set<std::int64_t> remaining = sections;
    std::vector<std::unordered_set<std::int64_t>> groups;
    std::deque<std::int64_t> pending;
    for (const std::int64_t start : seeds) {
        if (remaining.erase(start) == 0) {
            continue;
        }
        std::unordered_set<std::int64_t> group;
        group.insert(start);
        pending.push_back(start);
        while (!pending.empty()) {
            const std::int64_t current = pending.front();
            pending.pop_front();
            const std::int32_t x = sectionX(current);
            const std::int32_t z = sectionZ(current);
            for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
                for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                    // Skipping the section itself, which is already out of `remaining` and would
                    // be turned away below anyway.
                    if (offsetX == 0 && offsetZ == 0) {
                        continue;
                    }
                    const std::int64_t neighbour = sectionKey(x + offsetX, z + offsetZ);
                    if (remaining.erase(neighbour) == 0) {
                        continue;
                    }
                    group.insert(neighbour);
                    pending.push_back(neighbour);
                }
            }
        }
        groups.push_back(std::move(group));
    }
    // Biggest first: when two groups become one, the survivor should be the one that already owns
    // most of the map, so the fewest tasks and entities have to change hands. Stable, so two groups
    // of the same size keep the order they were found in, which is by their lowest section.
    std::ranges::stable_sort(groups,
                             [](const std::unordered_set<std::int64_t>& left,
                                const std::unordered_set<std::int64_t>& right) { return left.size() > right.size(); });
    return groups;
}

} // namespace basalt::server::runtime::sched::regionMap
