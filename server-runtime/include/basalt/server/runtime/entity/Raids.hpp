#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::nbt {
class NbtCompound;
}

namespace basalt::server::runtime::entity {

/// parity: `Raid.RaidStatus`.
enum class RaidStatus : std::uint8_t { Ongoing, Victory, Loss, Stopped };

/// parity: `Raid.RaiderType`, in its declaration order, which is the order a wave is spawned in.
enum class RaiderType : std::uint8_t { Vindicator, Evoker, Pillager, Witch, Ravager };

using RaidPos = std::array<std::int32_t, 3>;

/// One raid on one village.
///
/// parity: `Raid`. The raiders are held by entity id, grouped by the wave they joined.
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

    /// parity: `absorbRaidOmen`: the omen's level plus one, capped at five.
    void absorbOmen(std::int32_t amplifier);

    /// parity: `stop`. The bar's players are dropped by whoever owns the bar.
    void stop();

    /// parity: `getEnchantOdds`.
    [[nodiscard]] float enchantOdds() const noexcept;

    /// parity: `getDefaultNumSpawns` plus `getPotentialBonusSpawns`, drawn off the raid's random.
    [[nodiscard]] std::int32_t spawnsFor(RaiderType type, std::int32_t wave, std::string_view difficulty,
                                         bool bonusWave);

    /// Whether `entityId` is one of this raid's raiders.
    [[nodiscard]] bool has(std::int32_t entityId) const;

    /// parity: `addWaveMob`. Answers whether it was new.
    bool addWaveMob(std::int32_t wave, std::int32_t entityId);

    /// parity: `removeFromRaid`'s set removal. Answers whether it was there.
    bool removeRaider(std::int32_t wave, std::int32_t entityId);

    /// parity: the `MAP_CODEC` fields.
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
    /// Insertion order stands in for the `HashSet`'s.
    std::vector<wire::common::Uuid> heroes;
    std::int32_t celebrationTicks = 0;
    std::optional<RaidPos> waveSpawnPos;
    std::map<std::int32_t, std::vector<std::int32_t>> groups;
    std::map<std::int32_t, std::int32_t> leaders;

    /// parity: `RandomSource.create()`, a raid's own unseeded stream.
    random::RandomSource random;

    /// parity: the `ServerBossEvent`: its players by entity id, and what they were last sent.
    wire::common::Uuid barId;
    std::set<std::int32_t> barPlayers;
    wire::text::Component barName;
    float barProgress = 0.0F;
    bool barVisible = true;
};

/// parity: `Raid.RaiderType`'s per-wave arrays and `getNumGroups`.
namespace raidWaves {

[[nodiscard]] std::string_view typeName(RaiderType type);

[[nodiscard]] std::int32_t countFor(RaiderType type, std::int32_t wave);

[[nodiscard]] std::int32_t groupsFor(std::string_view difficulty);

} // namespace raidWaves

/// Every raid in one world.
///
/// parity: `Raids`, the saved data behind `ServerLevel.getRaids`, kept in `raids.nbt` next to the
/// world's regions.
class RaidManager {
public:
    [[nodiscard]] Raid* get(std::int32_t id);

    [[nodiscard]] std::optional<std::int32_t> idOf(const Raid* raid) const;

    /// parity: `getNearbyRaid`, the closest active one strictly inside the radius.
    [[nodiscard]] Raid* nearby(const RaidPos& pos, std::int32_t maxDistanceSquared = Raid::kValidRadiusSquared);

    /// parity: `getUniqueId`, which bumps before it answers: the first raid is number two.
    std::int32_t add(std::unique_ptr<Raid> raid);

    [[nodiscard]] bool contains(const Raid* raid) const { return idOf(raid).has_value(); }

    void remove(std::int32_t id);

    /// The ids in the order `Int2ObjectOpenHashMap` walks them.
    [[nodiscard]] std::vector<std::int32_t> tickOrder() const;

    [[nodiscard]] std::size_t size() const noexcept { return raids_.size(); }

    /// parity: `Raids.tick`'s own counter.
    std::int32_t tick = 0;
    bool dirty = false;

    void encode(wire::nbt::NbtCompound& tag) const;
    void decode(const wire::nbt::NbtCompound& tag);

    void load(std::string_view worldRoot);
    void save(std::string_view worldRoot);

private:
    std::map<std::int32_t, std::unique_ptr<Raid>> raids_;
    std::int32_t nextId_ = 1;
    std::int64_t seeds_ = 0;
};

} // namespace basalt::server::runtime::entity
