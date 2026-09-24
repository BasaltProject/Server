#pragma once

#include <basalt/server/runtime/world/poi/PoiManager.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity::villager {

using BlockPos = world::poi::BlockPos;

/// parity: `Activity`, the ones a villager's brain knows.
enum class Activity : std::uint8_t { kCore, kIdle, kWork, kPlay, kRest, kMeet, kPanic, kRaid, kPreRaid, kHide, kCount };

inline constexpr std::int64_t kNoExpiry = std::numeric_limits<std::int64_t>::max();

/// parity: `MemorySlot`: a value, and how many more ticks it is kept when it expires at all.
template <typename T>
struct Memory {
    std::optional<T> value;
    std::int64_t ttl = kNoExpiry;

    [[nodiscard]] bool present() const noexcept { return value.has_value(); }

    void set(T newValue, std::int64_t timeToLive = kNoExpiry) {
        value = std::move(newValue);
        ttl = timeToLive;
    }

    void erase() noexcept {
        value.reset();
        ttl = kNoExpiry;
    }

    /// parity: `MemorySlot.tick`.
    void tick() noexcept {
        if (!value.has_value() || ttl == kNoExpiry) {
            return;
        }
        if (ttl <= 0) {
            erase();
        } else {
            --ttl;
        }
    }
};

/// parity: `PositionTracker`: a fixed spot, or an entity followed wherever it goes.
struct Tracker {
    /// Zero for a spot, otherwise whose position this is.
    std::int32_t entityId = 0;
    /// parity: `EntityTracker.trackEyeHeight`.
    bool eyeHeight = false;
    std::array<double, 3> spot{};
    BlockPos block{};

    [[nodiscard]] static Tracker ofBlock(const BlockPos& pos) {
        return Tracker{.spot = {pos.x + 0.5, pos.y + 0.5, pos.z + 0.5}, .block = pos};
    }

    [[nodiscard]] static Tracker ofSpot(const std::array<double, 3>& spot);

    [[nodiscard]] static Tracker ofEntity(std::int32_t entityId, bool eyeHeight) {
        return Tracker{.entityId = entityId, .eyeHeight = eyeHeight};
    }
};

/// parity: `WalkTarget`.
struct WalkTarget {
    Tracker target;
    float speed = 0.0F;
    std::int32_t closeEnough = 0;
};

/// Which memory a behaviour's parameters name. parity: the `MemoryModuleType`s villagers use.
enum class MemoryId : std::uint8_t {
    kNone,
    kHome,
    kJobSite,
    kPotentialJobSite,
    kMeetingPoint,
    kSecondaryJobSite,
    kNearestHostile,
    kHurtByEntity,
    kInteractionTarget,
    kBreedTarget,
    kVisibleVillagerBabies,
};

/// Everything a villager's brain remembers. parity: the memories `Villager.BRAIN_PROVIDER` registers.
struct Memories {
    Memory<BlockPos> home;
    Memory<BlockPos> jobSite;
    Memory<BlockPos> potentialJobSite;
    Memory<BlockPos> meetingPoint;
    Memory<BlockPos> nearestBed;
    Memory<BlockPos> hidingPlace;
    Memory<std::vector<BlockPos>> secondaryJobSite;
    Memory<std::vector<BlockPos>> doorsToClose;
    Memory<Tracker> lookTarget;
    Memory<WalkTarget> walkTarget;
    /// parity: `PATH`, present while `MoveToTargetSink` has the navigation walking.
    Memory<bool> path;
    Memory<std::int64_t> cantReachWalkTargetSince;
    Memory<std::int64_t> lastSlept;
    Memory<std::int64_t> lastWoken;
    Memory<std::int64_t> lastWorkedAtPoi;
    Memory<std::int64_t> heardBellTime;
    Memory<bool> golemDetectedRecently;
    Memory<std::int32_t> interactionTarget;
    Memory<std::int32_t> breedTarget;
    Memory<std::int32_t> hurtByEntity;
    Memory<std::int32_t> nearestHostile;
    Memory<std::int32_t> nearestVisiblePlayer;
    Memory<std::int32_t> nearestVisibleWantedItem;
    Memory<std::string> hurtBy;
    /// Sorted nearest first, players and mobs alike.
    Memory<std::vector<std::int32_t>> nearestLivingEntities;
    /// parity: `NEAREST_VISIBLE_LIVING_ENTITIES`, the ones of those it can see.
    Memory<std::vector<std::int32_t>> visibleLivingEntities;
    Memory<std::vector<std::int32_t>> nearestPlayers;
    Memory<std::vector<std::int32_t>> visibleVillagerBabies;

    /// parity: `Brain.forgetOutdatedMemories`.
    void tick() noexcept;

    [[nodiscard]] bool visible(std::int32_t entityId) const;

    /// A position memory by id, or null for one that is not.
    [[nodiscard]] Memory<BlockPos>* position(MemoryId id);

    [[nodiscard]] Memory<std::int32_t>* entity(MemoryId id);

    [[nodiscard]] bool present(MemoryId id) const;
};

} // namespace basalt::server::runtime::entity::villager
