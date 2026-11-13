#include <basalt/server/runtime/world/LocalMobCaps.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using entity::MobCategory;

[[nodiscard]] std::size_t indexOf(MobCategory category) { return static_cast<std::size_t>(category); }

constexpr std::size_t kInitialSlots = 1024;

[[nodiscard]] std::size_t hashOf(std::int64_t key) {
    return static_cast<std::size_t>((static_cast<std::uint64_t>(key) * 0x9E3779B97F4A7C15ULL) >> 20);
}

} // namespace

LocalMobCaps::LocalMobCaps(std::vector<MobCapPlayer> players) : players_(std::move(players)) {
    counts_.assign(players_.size(), std::array<std::int32_t, kCategoryCount>{});
    playerChunks_.reserve(players_.size());
    for (const MobCapPlayer& player : players_) {
        playerChunks_.emplace_back(wire::common::mth::floor(player.x) >> 4, wire::common::mth::floor(player.z) >> 4);
    }
}

LocalMobCaps::LocalMobCaps(std::vector<MobCapPlayer> players, const Limits& limits, std::int32_t rangeChunks)
    : LocalMobCaps(std::move(players)) {
    limits_ = limits;
    rangeChunks_ = rangeChunks;
    distanceSquared_ = static_cast<double>(rangeChunks) * 16.0 * static_cast<double>(rangeChunks) * 16.0;
}

void LocalMobCaps::growCache() {
    std::vector<Slot> old = std::move(nearbyCache_);
    nearbyCache_.assign(old.empty() ? kInitialSlots : old.size() * 2, Slot{});
    const std::size_t mask = nearbyCache_.size() - 1;
    for (const Slot& slot : old) {
        if (!slot.used) {
            continue;
        }
        std::size_t at = hashOf(slot.key) & mask;
        while (nearbyCache_[at].used) {
            at = (at + 1) & mask;
        }
        nearbyCache_[at] = slot;
    }
}

std::span<const std::uint32_t> LocalMobCaps::nearby(std::int32_t chunkX, std::int32_t chunkZ) {
    const std::int64_t key = (static_cast<std::int64_t>(chunkX) << 32) | static_cast<std::uint32_t>(chunkZ);
    if (nearbyCache_.empty() || (cached_ + 1) * 2 > nearbyCache_.size()) {
        growCache();
    }
    const std::size_t mask = nearbyCache_.size() - 1;
    std::size_t at = hashOf(key) & mask;
    while (nearbyCache_[at].used) {
        if (nearbyCache_[at].key == key) {
            return std::span<const std::uint32_t>{nearbyPool_}.subspan(nearbyCache_[at].first, nearbyCache_[at].length);
        }
        at = (at + 1) & mask;
    }
    const double centreX = static_cast<double>(chunkX << 4) + 8.0;
    const double centreZ = static_cast<double>(chunkZ << 4) + 8.0;
    const auto first = static_cast<std::uint32_t>(nearbyPool_.size());
    for (std::size_t player = 0; player < players_.size(); ++player) {
        const double dx = centreX - players_[player].x;
        const double dz = centreZ - players_[player].z;
        if ((dx * dx) + (dz * dz) < distanceSquared_) {
            nearbyPool_.push_back(static_cast<std::uint32_t>(player));
        }
    }
    const auto length = static_cast<std::uint32_t>(nearbyPool_.size()) - first;
    nearbyCache_[at] = Slot{.key = key, .first = first, .length = length, .used = true};
    ++cached_;
    return std::span<const std::uint32_t>{nearbyPool_}.subspan(first, length);
}

bool LocalMobCaps::canSpawn(MobCategory category, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::int32_t maximum = limits_.has_value() && indexOf(category) < limits_->size()
                                     ? (*limits_)[indexOf(category)]
                                     : entity::mobCategoryFacts(category).maxPerChunk;
    return std::ranges::any_of(nearby(chunkX, chunkZ), [this, category, maximum](std::uint32_t at) {
        return counts_[at][indexOf(category)] < maximum;
    });
}

bool LocalMobCaps::isSpawnCandidate(std::int32_t chunkX, std::int32_t chunkZ) const {
    return std::ranges::any_of(playerChunks_, [this, chunkX, chunkZ](const std::pair<std::int32_t, std::int32_t>& at) {
        return std::abs(chunkX - at.first) <= rangeChunks_ && std::abs(chunkZ - at.second) <= rangeChunks_;
    });
}

void LocalMobCaps::add(MobCategory category, std::int32_t chunkX, std::int32_t chunkZ) {
    // parity: the miscellaneous category has no cap, so nothing counts towards one.
    if (category == MobCategory::Misc) {
        return;
    }
    for (const std::uint32_t at : nearby(chunkX, chunkZ)) {
        ++counts_[at][indexOf(category)];
    }
}

std::int32_t LocalMobCaps::count(std::int32_t playerId, MobCategory category) const {
    for (std::size_t at = 0; at < players_.size(); ++at) {
        if (players_[at].id == playerId) {
            return counts_[at][indexOf(category)];
        }
    }
    return 0;
}

} // namespace basalt::server::runtime::world
