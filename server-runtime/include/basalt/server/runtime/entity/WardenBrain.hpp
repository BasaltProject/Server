#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace wardenBrain {

inline constexpr std::int32_t kEmergeTicks = 134;
inline constexpr std::int32_t kDigTicks = 100;
inline constexpr std::int32_t kRoarTicks = 84;
inline constexpr std::int32_t kDigCooldown = 1200;
/// parity: `SonicBoom`'s sixty ticks.
inline constexpr std::int32_t kSonicTicks = 60;
inline constexpr std::int32_t kSonicBlastAt = 34;
inline constexpr std::int32_t kSonicCooldown = 40;
inline constexpr std::int32_t kFirstSonicCooldown = 200;
inline constexpr float kSonicDamage = 10.0F;
inline constexpr std::int32_t kDarknessInterval = 120;
inline constexpr std::int32_t kDarknessTicks = 260;
inline constexpr double kDarknessRadius = 20.0;
inline constexpr std::int32_t kDarknessDisplayLimit = 200;
/// parity: the synched `CLIENT_ANGER_LEVEL`, right after `Monster`'s fields.
inline constexpr std::uint8_t kAngerDataIndex = 16;

inline constexpr std::int32_t kSniffTicks = 84;
inline constexpr std::int32_t kSniffCooldownMin = 100;
inline constexpr std::int32_t kSniffCooldownMax = 200;
inline constexpr std::int32_t kDisturbanceTicks = 100;
/// parity: the warden's `VibrationUser`.
inline constexpr std::int32_t kListenerRadius = 16;
inline constexpr std::int32_t kVibrationCooldown = 40;
inline constexpr std::int32_t kRecentProjectileTicks = 100;

struct Heard {
    std::string event;
    float distance = 0.0F;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::optional<wire::common::Uuid> entity;
    std::optional<wire::common::Uuid> projectileOwner;
    std::int64_t tick = 0;
};

struct WardenState {
    std::int32_t emergeTicks = 0;
    std::int32_t digTicks = 0;
    std::int32_t roarTicks = 0;
    std::optional<std::int32_t> roarTarget;
    std::int32_t digCooldown = kDigCooldown;
    std::int32_t sonicTicks = 0;
    std::int32_t sonicCooldown = 0;
    std::optional<Heard> candidate;
    std::optional<Heard> travelling;
    std::int32_t travelTicks = 0;
    /// parity: `VibrationSystem.Data.reloadVibrationParticle`, runtime only like the rest.
    bool vibrationParticlePending = false;
    std::int64_t heardTime = -1;
    std::size_t heardCount = 0;
    std::int32_t vibrationCooldown = 0;
    std::int32_t recentProjectile = 0;
    std::int32_t sniffCooldown = 0;
    std::int32_t sniffTicks = 0;
    std::optional<std::array<std::int32_t, 3>> disturbance;
    std::int32_t disturbanceTicks = 0;
};

struct Action {
    enum class Kind : std::uint8_t { Sound, EntityEvent, Pose, SonicBlast, Discard, StartFight };
    Kind kind = Kind::Sound;
    std::string_view sound;
    float volume = 1.0F;
    std::int8_t event = 0;
    wire::protocol::play::Pose pose = wire::protocol::play::Pose::Standing;
    std::int32_t targetId = 0;
};

struct View {
    std::optional<std::array<double, 3>> target;
    bool onGround = false;
    bool inFluid = false;
    bool persistent = false;
};

[[nodiscard]] WardenState& stateOf(ServerEntity& warden);

[[nodiscard]] bool isDiggingOrEmerging(const ServerEntity& warden);

[[nodiscard]] bool isBusy(const ServerEntity& warden);

[[nodiscard]] wire::protocol::play::Pose poseOf(const ServerEntity& warden);

[[nodiscard]] std::vector<Action> startEmerging(ServerEntity& warden);

/// parity: `SetRoarTarget` then `Roar.start`.
[[nodiscard]] std::vector<Action> startRoar(ServerEntity& warden, std::int32_t targetId);

/// parity: `WardenAi.setDigCooldown`, which only ever refreshes a cooldown still running.
void refreshDigCooldown(ServerEntity& warden);

/// parity: `WardenAi.setDisturbanceLocation`.
void setDisturbance(ServerEntity& warden, const std::array<std::int32_t, 3>& at);

void offer(WardenState& state, Heard heard);

[[nodiscard]] std::vector<Action> meleeHit(ServerEntity& warden);

[[nodiscard]] std::vector<Action> tick(ServerEntity& warden, const View& view);

} // namespace wardenBrain

} // namespace basalt::server::runtime::entity
