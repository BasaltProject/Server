#include <basalt/server/runtime/entity/Raids.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace basalt::server::runtime::entity {
namespace {

using Counts = std::array<std::int32_t, 8>;

const std::map<std::string, Counts, std::less<>>& waveCounts() {
    static const std::map<std::string, Counts, std::less<>> kCounts{{"evoker", Counts{0, 0, 0, 0, 0, 1, 1, 2}},
                                                                    {"pillager", Counts{0, 4, 3, 3, 4, 4, 4, 2}},
                                                                    {"ravager", Counts{0, 0, 0, 1, 0, 1, 0, 2}},
                                                                    {"vindicator", Counts{0, 0, 2, 0, 1, 4, 2, 5}},
                                                                    {"witch", Counts{0, 0, 0, 0, 3, 0, 0, 1}}};
    return kCounts;
}

std::string lowercase(std::string_view value) {
    std::string result{value};
    std::ranges::transform(result, result.begin(), [](unsigned char letter) {
        return static_cast<char>(letter >= 'A' && letter <= 'Z' ? letter + ('a' - 'A') : letter);
    });
    return result;
}

} // namespace

Raid::Raid(std::int32_t centerX, std::int32_t centerY, std::int32_t centerZ, std::int32_t totalGroups)
    : centerX_(centerX), centerY_(centerY), centerZ_(centerZ), totalGroups_(totalGroups),
      barId_(wire::common::Uuid::random()) {}

double Raid::distanceSquaredTo(double x, double y, double z) const {
    const double dx = centerX_ + 0.5 - x;
    const double dy = centerY_ + 0.5 - y;
    const double dz = centerZ_ + 0.5 - z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

namespace raidWaves {

const std::vector<std::string>& types() {
    static const std::vector<std::string> kTypes = [] {
        std::vector<std::string> names;
        for (const auto& [name, counts] : waveCounts()) {
            names.push_back(name);
        }
        return names;
    }();
    return kTypes;
}

std::int32_t countFor(std::string_view type, std::int32_t wave) {
    const auto found = waveCounts().find(type);
    if (found == waveCounts().end()) {
        return 0;
    }
    const auto last = static_cast<std::int32_t>(found->second.size()) - 1;
    return found->second[static_cast<std::size_t>(std::clamp(wave, 0, last))];
}

std::int32_t groupsFor(std::string_view difficulty) {
    const std::string name = lowercase(difficulty);
    if (name == "peaceful") {
        return 0;
    }
    if (name == "easy") {
        return 3;
    }
    if (name == "hard") {
        return 7;
    }
    return 5;
}

} // namespace raidWaves

Raid* RaidManager::nearby(double x, double y, double z, double radiusSquared) {
    for (const std::unique_ptr<Raid>& raid : raids_) {
        if (raid->active() && raid->distanceSquaredTo(x, y, z) < radiusSquared) {
            return raid.get();
        }
    }
    return nullptr;
}

Raid* RaidManager::createOrExtend(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t amplifier,
                                  std::string_view difficulty) {
    const std::int32_t groups = raidWaves::groupsFor(difficulty);
    if (groups == 0) {
        return nullptr;
    }
    Raid* raid = nearby(x + 0.5, y + 0.5, z + 0.5);
    if (raid == nullptr) {
        raids_.push_back(std::make_unique<Raid>(x, y, z, groups));
        raid = raids_.back().get();
    }
    raid->omenLevel = std::clamp(raid->omenLevel + amplifier + 1, 0, Raid::kMaxOmenLevel);
    return raid;
}

void RaidManager::forget(const std::vector<Raid*>& finished) {
    std::erase_if(raids_, [&finished](const std::unique_ptr<Raid>& raid) {
        return std::ranges::find(finished, raid.get()) != finished.end();
    });
}

} // namespace basalt::server::runtime::entity
