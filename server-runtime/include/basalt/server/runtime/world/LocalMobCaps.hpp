#pragma once

#include <basalt/server/runtime/entity/MobCategory.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace basalt::server::runtime::world {

struct MobCapPlayer {
    std::int32_t id = 0;
    double x = 0.0;
    double z = 0.0;
};

/// parity: `LocalMobCapCalculator`.
class LocalMobCaps {
public:
    /// parity: DistanceManager's radius-eight square used for the global spawnable-chunk count.
    static constexpr std::int32_t kGlobalCapRadius = 8;

    static constexpr double kSpawningDistanceSquared = 128.0 * 128.0;

    explicit LocalMobCaps(std::vector<MobCapPlayer> players);

    [[nodiscard]] bool canSpawn(entity::MobCategory category, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] bool isSpawnCandidate(std::int32_t chunkX, std::int32_t chunkZ) const;

    void add(entity::MobCategory category, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::int32_t count(std::int32_t playerId, entity::MobCategory category) const;

private:
    static constexpr std::size_t kCategoryCount = static_cast<std::size_t>(entity::MobCategory::Misc) + 1;

    /// Worked out once per chunk: a spawn pass asks about the same chunk per category and per mob placed.
    [[nodiscard]] const std::vector<std::size_t>& nearby(std::int32_t chunkX, std::int32_t chunkZ);

    std::vector<MobCapPlayer> players_;
    std::map<std::int32_t, std::array<std::int32_t, kCategoryCount>> counts_;
    std::map<std::int64_t, std::vector<std::size_t>> nearbyCache_;
};

} // namespace basalt::server::runtime::world
