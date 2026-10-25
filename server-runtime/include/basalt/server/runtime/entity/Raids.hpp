#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/IntHashOrder.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity {

enum class RaidStatus : std::uint8_t { Ongoing, Victory, Loss, Stopped };

enum class RaiderType : std::uint8_t { Vindicator, Evoker, Pillager, Witch, Ravager };

using RaidPos = std::array<std::int32_t, 3>;

class Raid {
public:
    static constexpr std::int32_t kPreRaidTicks = 300;
    static constexpr std::int64_t kTimeoutTicks = 48000;
    static constexpr std::int32_t kCelebrationTicks = 600;
    static constexpr std::int32_t kPostRaidTicks = 40;
    static constexpr std::int32_t kMaxNoActionTime = 2400;
    static constexpr std::int32_t kOutsideRaidTimeout = 30;
    static constexpr std::int32_t kMaxOmenLevel = 5;
    static constexpr std::int32_t kHeroDurationTicks = 48000;
    static constexpr std::int32_t kValidRadiusSquared = 9216;
    static constexpr std::int32_t kRemovalRadiusSquared = 12544;

    Raid(const RaidPos& center, std::int32_t numGroups, std::int64_t seed);

    [[nodiscard]] bool isOver() const noexcept { return status == RaidStatus::Victory || status == RaidStatus::Loss; }

    [[nodiscard]] bool isStopped() const noexcept { return status == RaidStatus::Stopped; }

    [[nodiscard]] bool hasFirstWaveSpawned() const noexcept { return groupsSpawned > 0; }

    [[nodiscard]] bool isBetweenWaves() const {
        return hasFirstWaveSpawned() && totalRaidersAlive() == 0 && cooldownTicks > 0;
    }

    [[nodiscard]] std::int32_t totalRaidersAlive() const;

    [[nodiscard]] bool hasBonusWave() const noexcept { return raidOmenLevel > 1; }

    [[nodiscard]] bool isFinalWave() const noexcept { return groupsSpawned == numGroups; }

    [[nodiscard]] bool hasSpawnedBonusWave() const noexcept { return groupsSpawned > numGroups; }

    [[nodiscard]] bool hasMoreWaves() const noexcept {
        return hasBonusWave() ? !hasSpawnedBonusWave() : !isFinalWave();
    }

    [[nodiscard]] bool shouldSpawnBonusGroup() const {
        return isFinalWave() && totalRaidersAlive() == 0 && hasBonusWave();
    }

    [[nodiscard]] bool shouldSpawnGroup() const {
        return cooldownTicks == 0 && (groupsSpawned < numGroups || shouldSpawnBonusGroup()) && totalRaidersAlive() == 0;
    }

    void absorbOmen(std::int32_t amplifier);

    /// parity: `stop`. The bar's players are dropped by whoever owns the bar.
    void stop();

    [[nodiscard]] float enchantOdds() const noexcept;

    [[nodiscard]] std::int32_t spawnsFor(RaiderType type, std::int32_t wave, std::string_view difficulty,
                                         bool bonusWave);

    [[nodiscard]] bool has(std::int32_t entityId) const;

    bool addWaveMob(std::int32_t wave, std::int32_t entityId);

    bool removeRaider(std::int32_t wave, std::int32_t entityId);

    void encode(wire::nbt::NbtCompound& tag) const;
    [[nodiscard]] static std::unique_ptr<Raid> decode(const wire::nbt::NbtCompound& tag, std::int64_t seed);

    bool started = false;
    bool active = true;
    std::int64_t ticksActive = 0;
    std::int32_t raidOmenLevel = 0;
    std::int32_t groupsSpawned = 0;
    std::int32_t cooldownTicks = kPreRaidTicks;
    std::int32_t postRaidTicks = 0;
    float totalHealth = 0.0F;
    std::int32_t numGroups = 0;
    RaidStatus status = RaidStatus::Ongoing;
    /// How it ended before `stop` wrote over it. Not saved.
    RaidStatus endedAs = RaidStatus::Ongoing;
    RaidPos center{};
    std::vector<wire::common::Uuid> heroes;
    std::int32_t celebrationTicks = 0;
    std::optional<RaidPos> waveSpawnPos;
    std::map<std::int32_t, std::vector<std::int32_t>> groups;
    std::map<std::int32_t, std::int32_t> leaders;

    random::RandomSource random;

    wire::common::Uuid barId;
    std::set<std::int32_t> barPlayers;
    wire::text::Component barName;
    float barProgress = 0.0F;
    std::int64_t barStamp = 0;
    bool barVisible = true;
    std::int32_t barColor = 2;
    std::int32_t barStyle = 2;
    /// Bits one, two and four: darken the sky, play the boss music, create fog.
    std::int32_t barFlags = 0;
    /// What a plugin keeps on the raid, saved as `BukkitValues`.
    wire::nbt::NbtCompound values;
};

namespace raidWaves {

[[nodiscard]] std::string_view typeName(RaiderType type);

[[nodiscard]] std::int32_t countFor(RaiderType type, std::int32_t wave);

[[nodiscard]] std::int32_t groupsFor(std::string_view difficulty);

} // namespace raidWaves

class RaidManager {
public:
    [[nodiscard]] Raid* get(std::int32_t id);

    [[nodiscard]] std::optional<std::int32_t> idOf(const Raid* raid) const;

    [[nodiscard]] Raid* nearby(const RaidPos& pos, std::int32_t maxDistanceSquared = Raid::kValidRadiusSquared);

    /// parity: `getUniqueId`, which bumps before it answers: the first raid is number two.
    std::int32_t add(std::unique_ptr<Raid> raid);

    [[nodiscard]] bool contains(const Raid* raid) const { return idOf(raid).has_value(); }

    void remove(std::int32_t id);

    [[nodiscard]] std::vector<std::int32_t> tickOrder() const { return order_.order(); }

    void forEachInTickOrder(const std::function<bool(std::int32_t id, Raid& raid)>& visit);

    [[nodiscard]] std::size_t size() const noexcept { return raids_.size(); }

    std::int32_t tick = 0;
    bool dirty = false;

    void encode(wire::nbt::NbtCompound& tag) const;
    void decode(const wire::nbt::NbtCompound& tag);

    void load(std::string_view worldRoot, std::string_view dimension);
    void save(std::string_view worldRoot);

private:
    std::map<std::int32_t, std::unique_ptr<Raid>> raids_;
    IntHashOrder order_;
    std::int32_t nextId_ = 1;
    std::int64_t seeds_ = 0;
};

} // namespace basalt::server::runtime::entity
