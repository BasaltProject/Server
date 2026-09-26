#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The allay: the player it likes, the note block it listens to, the stack it gathers, and the
/// dancing and duplicating a jukebox sets off.
///
/// parity: `Allay` and `AllayAi`.
namespace allays {

/// parity: `DATA_DANCING` and `DATA_CAN_DUPLICATE`, after `Mob`'s flags.
inline constexpr std::uint8_t kDancingDataIndex = 16;
inline constexpr std::uint8_t kCanDuplicateDataIndex = 17;

inline constexpr std::int64_t kDuplicationCooldown = 6000;
/// parity: `TIME_TO_FORGET_NOTEBLOCK` and `ITEM_PICKUP_COOLDOWN_DURATION`.
inline constexpr std::int32_t kNoteblockCooldown = 600;
inline constexpr std::int32_t kPickupCooldown = 60;
inline constexpr double kLikedPlayerRange = 64.0;
inline constexpr std::int32_t kNoteblockDistance = 1024;
/// parity: `GameEvent.JUKEBOX_PLAY`'s notification radius and the vibration user's sixteen.
inline constexpr double kJukeboxRadius = 10.0;
inline constexpr double kListenerRadius = 16.0;

using Pos = std::array<std::int32_t, 3>;

/// parity: `WalkTarget` over its `PositionTracker`: a point, or an entity followed as it moves.
struct WalkTarget {
    std::array<double, 3> at{};
    std::optional<std::int32_t> entity;
    float speed = 1.0F;
    std::int32_t closeEnough = 0;
};

/// What an allay keeps beyond the entity itself, its brain's memories included.
struct AllayState {
    std::optional<wire::common::Uuid> likedPlayer;
    /// The liked player's entity id, looked up on the allay's own turn.
    std::optional<std::int32_t> likedPlayerEntity;
    std::optional<Pos> likedNoteblock;
    std::string noteblockDimension;
    std::optional<std::int32_t> noteblockCooldown;
    std::int32_t pickupCooldown = 0;
    /// parity: the one-slot `SimpleContainer`.
    std::optional<server::world::loot::ItemStack> inventory;
    std::int64_t duplicationCooldown = 0;
    bool dancing = false;
    std::optional<Pos> jukebox;
    /// The brain's walk and look targets.
    std::optional<WalkTarget> walk;
    std::optional<std::array<double, 3>> lastPathTarget;
    std::optional<std::int32_t> lookEntity;
    std::optional<std::array<double, 3>> lookAt;
    std::int32_t lookTicks = 0;
    std::int32_t lookCooldown = 0;
    /// parity: `GoAndGiveItemsToTarget`'s run, twenty ticks at most; negative when idle.
    std::int32_t giveTicks = -1;
    /// parity: `DoNothing(30, 60)` still waiting, and the `AnimalPanic` run.
    std::int32_t idleTicks = 0;
    std::int32_t panicTicks = 0;
    /// The last game event heard, by time and count within that tick.
    std::int64_t heardTime = -1;
    std::size_t heardCount = 0;
    /// parity: `NEAREST_VISIBLE_WANTED_ITEM`, by entity id, and the sensor's own countdown to its
    /// next look, every twenty ticks from a random start.
    std::optional<std::int32_t> wantedItem;
    std::int32_t itemSensorTicks = -1;

    /// parity: `VibrationSystem.Data`: the note heard this tick, the one on its way and its travel.
    struct Vibration {
        Pos pos{};
        float distance = 0.0F;
        std::int64_t tick = 0;
    };

    std::optional<Vibration> vibrationCandidate;
    std::optional<Vibration> vibrationTravelling;
    std::int32_t vibrationTicks = 0;
};

[[nodiscard]] bool isAllay(const wire::common::Identifier& type);

/// The allay's state, made on first use from what it saved.
[[nodiscard]] AllayState& stateOf(ServerEntity& allay);

/// parity: `addAdditionalSaveData` and the three memories its brain keeps.
void write(const ServerEntity& allay, wire::nbt::NbtCompound& tag);

/// parity: `ItemStack.isSameItem` and the potion check of `allayConsidersItemEqual`.
[[nodiscard]] bool considersEqual(const server::world::loot::ItemStack& held,
                                  const server::world::loot::ItemStack& other);

/// How many of `stack` the one slot takes. parity: `SimpleContainer.addItem`.
[[nodiscard]] std::int32_t roomFor(const AllayState& state, const server::world::loot::ItemStack& stack);

} // namespace allays

} // namespace basalt::server::runtime::entity
