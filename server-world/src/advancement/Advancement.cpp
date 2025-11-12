#include <basalt/server/world/advancement/Advancement.hpp>

#include <algorithm>

namespace basalt::server::world::advancement {

bool AdvancementRequirements::isComplete(const std::set<std::string, std::less<>>& granted) const {
    if (groups.empty()) {
        return false;
    }
    return std::all_of(groups.begin(), groups.end(), [&granted](const std::vector<std::string>& group) {
        return std::any_of(group.begin(), group.end(),
                           [&granted](const std::string& name) { return granted.contains(name); });
    });
}

const AdvancementCriterion* Advancement::criterion(std::string_view name) const {
    for (const std::pair<std::string, AdvancementCriterion>& entry : criteria) {
        if (entry.first == name) {
            return &entry.second;
        }
    }
    return nullptr;
}

} // namespace basalt::server::world::advancement
