#include <basalt/server/runtime/world/LocalMobCaps.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using entity::MobCategory;

[[nodiscard]] std::size_t indexOf(MobCategory category) { return static_cast<std::size_t>(category); }

} // namespace

LocalMobCaps::LocalMobCaps(std::vector<MobCapPlayer> players) : players_(std::move(players)) {
    for (const MobCapPlayer& player : players_) {
        counts_.emplace(player.id, std::array<std::int32_t, kCategoryCount>{});
    }
}

LocalMobCaps::LocalMobCaps(std::vector<MobCapPlayer> players, const Limits& limits, std::int32_t rangeChunks)
    : LocalMobCaps(std::move(players)) {
    limits_ = limits;
    rangeChunks_ = rangeChunks;
    distanceSquared_ = static_cast<double>(rangeChunks) * 16.0 * static_cast<double>(rangeChunks) * 16.0;
}

const std::vector<std::size_t>& LocalMobCaps::nearby(std::int32_t chunkX, std::int32_t chunkZ) {
    const std::int64_t key = WorldState::key(chunkX, chunkZ);
    if (const auto found = nearbyCache_.find(key); found != nearbyCache_.end()) {
        return found->second;
    }
    const double centreX = static_cast<double>(chunkX << 4) + 8.0;
    const double centreZ = static_cast<double>(chunkZ << 4) + 8.0;
    std::vector<std::size_t> within;
    for (std::size_t at = 0; at < players_.size(); ++at) {
        const double dx = centreX - players_[at].x;
        const double dz = centreZ - players_[at].z;
        if ((dx * dx) + (dz * dz) < distanceSquared_) {
            within.push_back(at);
        }
    }
    return nearbyCache_.emplace(key, std::move(within)).first->second;
}

bool LocalMobCaps::canSpawn(MobCategory category, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::int32_t maximum = limits_.has_value() && indexOf(category) < limits_->size()
                                     ? (*limits_)[indexOf(category)]
                                     : entity::mobCategoryFacts(category).maxPerChunk;
    return std::ranges::any_of(nearby(chunkX, chunkZ), [this, category, maximum](std::size_t at) {
        return counts_.at(players_[at].id)[indexOf(category)] < maximum;
    });
}

bool LocalMobCaps::isSpawnCandidate(std::int32_t chunkX, std::int32_t chunkZ) const {
    return std::ranges::any_of(players_, [this, chunkX, chunkZ](const MobCapPlayer& player) {
        const std::int32_t playerChunkX = wire::common::mth::floor(player.x) >> 4;
        const std::int32_t playerChunkZ = wire::common::mth::floor(player.z) >> 4;
        return std::abs(chunkX - playerChunkX) <= rangeChunks_ && std::abs(chunkZ - playerChunkZ) <= rangeChunks_;
    });
}

void LocalMobCaps::add(MobCategory category, std::int32_t chunkX, std::int32_t chunkZ) {
    // parity: the miscellaneous category has no cap, so nothing counts towards one.
    if (category == MobCategory::Misc) {
        return;
    }
    for (const std::size_t at : nearby(chunkX, chunkZ)) {
        ++counts_.at(players_[at].id)[indexOf(category)];
    }
}

std::int32_t LocalMobCaps::count(std::int32_t playerId, MobCategory category) const {
    const auto found = counts_.find(playerId);
    return found == counts_.end() ? 0 : found->second[indexOf(category)];
}

} // namespace basalt::server::runtime::world
