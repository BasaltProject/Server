#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity {

/// The sniffer: scenting, sniffing out a spot, digging there and the seed it turns up.
///
/// parity: `Sniffer` and `SnifferAi`.
namespace sniffers {

/// parity: `Sniffer.DATA_STATE` and `DATA_DROP_SEED_AT_TICK`, after `AgeableMob`'s own.
inline constexpr std::uint8_t kStateDataIndex = 18;
inline constexpr std::uint8_t kDropSeedDataIndex = 19;
inline constexpr std::int64_t kSniffCooldown = 9600;
inline constexpr std::int32_t kDropSeedOffset = 120;
/// parity: `Sniffer.onDiggingStart`'s entity event, which starts the client's digging animation.
inline constexpr std::int8_t kDiggingEvent = 63;

/// parity: `Sniffer.State`, in id order.
enum class Pose : std::uint8_t { Idling, FeelingHappy, Scenting, Sniffing, Searching, Digging, Rising };

enum class Activity : std::uint8_t { Idle, Sniff, Dig };

/// The behaviours that run for a while, one at a time.
enum class Running : std::uint8_t { None, Scenting, Sniffing, Searching, Digging, FinishedDigging, Nothing };

struct SnifferState {
    Activity activity = Activity::Idle;
    Pose pose = Pose::Idling;
    Running running = Running::None;
    std::int64_t runningEnd = 0;
    /// parity: `FeelingHappy(40, 100)`, which runs beside the idle `RunOne`.
    bool feelingHappy = false;
    std::int64_t happyEnd = 0;
    bool happy = false;
    bool digging = false;
    std::optional<ai::brainKit::Block> sniffingTarget;
    ai::brainKit::Memory<bool> sniffCooldown;
    /// parity: `SNIFFER_EXPLORED_POSITIONS`, newest first, as dimension and square.
    std::vector<std::pair<std::string, ai::brainKit::Block>> explored;
    std::int32_t dropSeedAt = 0;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::brainKit::Temptation temptation;
    std::optional<std::int32_t> breedTarget;
    std::int32_t panicTicks = 0;
    std::int32_t lookTicker = 0;
    /// The game time of the last turn, which a saved cooldown's time to live is counted from.
    std::int64_t lastTick = 0;
    /// A loaded cooldown's time to live, until the first turn turns it into an end.
    std::optional<std::int64_t> loadedCooldown;
};

[[nodiscard]] SnifferState& stateOf(ServerEntity& sniffer);

/// parity: `Sniffer.canMate`'s own half: idling, scenting or happy.
[[nodiscard]] bool canMateNow(const ServerEntity& sniffer);

/// parity: the explored squares and the sniff cooldown the brain saves.
void write(const ServerEntity& sniffer, wire::nbt::NbtCompound& tag);

} // namespace sniffers

} // namespace basalt::server::runtime::entity
