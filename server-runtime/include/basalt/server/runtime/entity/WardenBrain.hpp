#pragma once

#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The warden's timed behaviours: rising out of the ground, burrowing back into it, the roar before
/// a fight and the sonic boom in one.
///
/// parity: `WardenAi`'s emerge, dig, roar and fight activities, `Emerging`, `Digging`, `Roar` and
/// `SonicBoom`. The anger itself stays in `wardens`.
namespace wardenBrain {

/// parity: `WardenAi.EMERGE_DURATION`, `DIGGING_DURATION` and `ROAR_DURATION`.
inline constexpr std::int32_t kEmergeTicks = 134;
inline constexpr std::int32_t kDigTicks = 100;
inline constexpr std::int32_t kRoarTicks = 84;
inline constexpr std::int32_t kDigCooldown = 1200;
/// parity: `SonicBoom`'s sixty ticks, the thirty-four before the blast, and the cooldowns.
inline constexpr std::int32_t kSonicTicks = 60;
inline constexpr std::int32_t kSonicBlastAt = 34;
inline constexpr std::int32_t kSonicCooldown = 40;
inline constexpr std::int32_t kFirstSonicCooldown = 200;
inline constexpr float kSonicDamage = 10.0F;
/// parity: `Warden`'s darkness pulse.
inline constexpr std::int32_t kDarknessInterval = 120;
inline constexpr std::int32_t kDarknessTicks = 260;
inline constexpr double kDarknessRadius = 20.0;
inline constexpr std::int32_t kDarknessDisplayLimit = 200;
/// parity: the synched `CLIENT_ANGER_LEVEL`, right after `Monster`'s fields.
inline constexpr std::uint8_t kAngerDataIndex = 16;

struct WardenState {
    std::int32_t emergeTicks = 0;
    std::int32_t digTicks = 0;
    std::int32_t roarTicks = 0;
    std::optional<std::int32_t> roarTarget;
    /// Zero when the dig cooldown has run out and nobody refreshes it any more.
    std::int32_t digCooldown = kDigCooldown;
    /// Ticks into a sonic boom, zero when there is none.
    std::int32_t sonicTicks = 0;
    std::int32_t sonicCooldown = 0;
};

/// One thing the brain wants done this tick.
struct Action {
    enum class Kind : std::uint8_t { Sound, EntityEvent, Pose, SonicBlast, Discard, StartFight };
    Kind kind = Kind::Sound;
    std::string_view sound;
    float volume = 1.0F;
    std::int8_t event = 0;
    wire::protocol::play::Pose pose = wire::protocol::play::Pose::Standing;
    std::int32_t targetId = 0;
};

/// What the brain reads about the warden's surroundings.
struct View {
    /// Where the attack target is, if it has one.
    std::optional<std::array<double, 3>> target;
    bool onGround = false;
    bool inFluid = false;
    bool persistent = false;
};

[[nodiscard]] WardenState& stateOf(ServerEntity& warden);

[[nodiscard]] bool isDiggingOrEmerging(const ServerEntity& warden);

/// Whether a timed behaviour holds the warden still, so its ordinary goals wait.
[[nodiscard]] bool isBusy(const ServerEntity& warden);

[[nodiscard]] wire::protocol::play::Pose poseOf(const ServerEntity& warden);

/// parity: `finalizeSpawn` with `EntitySpawnReason.TRIGGERED`, the warden rising from a shrieker.
[[nodiscard]] std::vector<Action> startEmerging(ServerEntity& warden);

/// parity: `SetRoarTarget` then `Roar.start`: a new suspect is roared at before it is fought.
[[nodiscard]] std::vector<Action> startRoar(ServerEntity& warden, std::int32_t targetId);

/// parity: `WardenAi.setDigCooldown`, which only ever refreshes a cooldown still running.
void refreshDigCooldown(ServerEntity& warden);

/// parity: `Warden.doHurtTarget`, whose swing pushes the next sonic boom back.
[[nodiscard]] std::vector<Action> meleeHit(ServerEntity& warden);

/// One tick of the timed behaviours, in the order `WardenAi.updateActivity` would pick them.
[[nodiscard]] std::vector<Action> tick(ServerEntity& warden, const View& view);

} // namespace wardenBrain

} // namespace basalt::server::runtime::entity
