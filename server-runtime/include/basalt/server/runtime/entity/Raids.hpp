#pragma once

#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

/// How a raid ended, which is what decides whether anybody gets the hero effect.
enum class RaidStatus : std::uint8_t { Ongoing, Victory, Loss, Stopped };

/// One raid on one village.
///
/// parity: `Raid`, minus what needs a POI map: the centre never moves, and the raid is over when its
/// waves are, rather than when the village stops being one.
class Raid {
public:
    /// parity: `DEFAULT_PRE_RAID_TICKS`, `RAID_TIMEOUT_TICKS` and `MAX_CELEBRATION_TICKS`.
    static constexpr std::int32_t kPreRaidTicks = 300;
    static constexpr std::int32_t kTimeoutTicks = 48000;
    static constexpr std::int32_t kCelebrationTicks = 600;

    /// parity: `VALID_RAID_RADIUS_SQR`, the distance a player counts as being in the raid from.
    static constexpr double kValidRadiusSquared = 9216.0;

    /// parity: `MAX_RAID_OMEN_LEVEL` and the hero effect's own duration.
    static constexpr std::int32_t kMaxOmenLevel = 5;
    static constexpr std::int32_t kHeroDurationTicks = 48000;

    /// parity: `getNumGroups`, three waves on easy, five on normal, seven on hard.
    Raid(std::int32_t centerX, std::int32_t centerY, std::int32_t centerZ, std::int32_t totalGroups);

    [[nodiscard]] std::int32_t centerX() const noexcept { return centerX_; }

    [[nodiscard]] std::int32_t centerY() const noexcept { return centerY_; }

    [[nodiscard]] std::int32_t centerZ() const noexcept { return centerZ_; }

    [[nodiscard]] std::int32_t totalGroups() const noexcept { return totalGroups_; }

    [[nodiscard]] const wire::common::Uuid& barId() const noexcept { return barId_; }

    [[nodiscard]] bool active() const noexcept { return status == RaidStatus::Ongoing; }

    [[nodiscard]] bool hasMoreWaves() const noexcept { return groupsSpawned < totalGroups_; }

    [[nodiscard]] double distanceSquaredTo(double x, double y, double z) const;

    std::int32_t omenLevel = 0;
    std::int32_t groupsSpawned = 0;
    std::int32_t ticksActive = 0;

    /// parity: `raidCooldownTicks`, the fifteen seconds between one wave dying and the next.
    std::int32_t cooldown = kPreRaidTicks;
    RaidStatus status = RaidStatus::Ongoing;
    std::int32_t celebrationTicks = 0;

    /// Entity ids, which is all this build needs to count what is left of a wave.
    std::vector<std::int32_t> raiders;
    double totalHealth = 0.0;

    /// Whoever was near while it was on, and so gets the hero effect when it is won.
    std::set<std::int32_t> heroes;

    /// Which players are currently being sent the bar.
    std::set<std::int32_t> watchers;

private:
    std::int32_t centerX_ = 0;
    std::int32_t centerY_ = 0;
    std::int32_t centerZ_ = 0;
    std::int32_t totalGroups_ = 0;
    wire::common::Uuid barId_;
};

/// parity: `Raid.RaiderType`, whose arrays are how many of each kind each wave brings.
namespace raidWaves {

[[nodiscard]] const std::vector<std::string>& types();

/// How many of `type` wave `wave` brings, counting from one.
[[nodiscard]] std::int32_t countFor(std::string_view type, std::int32_t wave);

/// parity: `getNumGroups`, which is the difficulty and nothing else.
[[nodiscard]] std::int32_t groupsFor(std::string_view difficulty);

} // namespace raidWaves

/// Every raid running in one world. parity: `Raids`, the saved data behind `ServerLevel.getRaids`.
class RaidManager {
public:
    [[nodiscard]] const std::vector<std::unique_ptr<Raid>>& all() const noexcept { return raids_; }

    /// parity: `getNearbyRaid`, which is what tells a village it is under attack.
    [[nodiscard]] Raid* nearby(double x, double y, double z, double radiusSquared = Raid::kValidRadiusSquared);

    /// parity: `createOrExtendRaid`: a second omen carried into a raid that is already running makes
    /// that raid harder rather than starting another one.
    Raid* createOrExtend(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t amplifier,
                         std::string_view difficulty);

    void forget(const std::vector<Raid*>& finished);

private:
    std::vector<std::unique_ptr<Raid>> raids_;
};

} // namespace basalt::server::runtime::entity
