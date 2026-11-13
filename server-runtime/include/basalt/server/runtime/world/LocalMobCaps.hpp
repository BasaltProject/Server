#pragma once

#include <basalt/server/runtime/entity/MobCategory.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
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

    using Limits = std::array<std::int32_t, 7>;

    explicit LocalMobCaps(std::vector<MobCapPlayer> players);

    /// Limits per capped category and the spawn range in chunks, in place of vanilla's.
    LocalMobCaps(std::vector<MobCapPlayer> players, const Limits& limits, std::int32_t rangeChunks);

    [[nodiscard]] bool canSpawn(entity::MobCategory category, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] bool isSpawnCandidate(std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] const std::vector<std::pair<std::int32_t, std::int32_t>>& playerChunks() const noexcept {
        return playerChunks_;
    }

    void add(entity::MobCategory category, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::int32_t count(std::int32_t playerId, entity::MobCategory category) const;

private:
    static constexpr std::size_t kCategoryCount = static_cast<std::size_t>(entity::MobCategory::Misc) + 1;

    /// Worked out once per chunk: a spawn pass asks about the same chunk per category and per mob placed.
    [[nodiscard]] std::span<const std::uint32_t> nearby(std::int32_t chunkX, std::int32_t chunkZ);

    std::vector<MobCapPlayer> players_;
    std::optional<Limits> limits_;
    std::int32_t rangeChunks_ = kGlobalCapRadius;
    double distanceSquared_ = kSpawningDistanceSquared;
    std::vector<std::array<std::int32_t, kCategoryCount>> counts_;
    std::vector<std::pair<std::int32_t, std::int32_t>> playerChunks_;

    struct Slot {
        std::int64_t key = 0;
        std::uint32_t first = 0;
        std::uint32_t length = 0;
        bool used = false;
    };

    void growCache();

    std::vector<Slot> nearbyCache_;
    std::size_t cached_ = 0;
    std::vector<std::uint32_t> nearbyPool_;
};

} // namespace basalt::server::runtime::world
