#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cmath>
#include <memory>

namespace basalt::server::runtime::entity::wardenBrain {
namespace {

using wire::protocol::play::Pose;

/// parity: `Roar`'s twenty-five ticks before the roar is heard.
constexpr std::int32_t kRoarSoundAt = 25;
constexpr std::int32_t kRoarAnger = 20;
/// parity: `SonicBoom.checkExtraStartConditions`, `closerThan(target, 15, 20)`.
constexpr double kSonicReachXZ = 15.0;
constexpr double kSonicReachY = 20.0;
constexpr std::int8_t kSonicEvent = 62;
constexpr std::int8_t kAttackEvent = 4;

[[nodiscard]] Action sound(std::string_view name, float volume) {
    return Action{.kind = Action::Kind::Sound, .sound = name, .volume = volume};
}

[[nodiscard]] Action pose(Pose value) { return Action{.kind = Action::Kind::Pose, .pose = value}; }

[[nodiscard]] bool inSonicReach(const ServerEntity& warden, const std::array<double, 3>& target) {
    const double dx = target[0] - warden.x;
    const double dz = target[2] - warden.z;
    return (dx * dx) + (dz * dz) < kSonicReachXZ * kSonicReachXZ && std::abs(target[1] - warden.y) < kSonicReachY;
}

} // namespace

WardenState& stateOf(ServerEntity& warden) {
    if (warden.warden == nullptr) {
        warden.warden = std::make_shared<WardenState>();
    }
    return *warden.warden;
}

bool isDiggingOrEmerging(const ServerEntity& warden) {
    return warden.warden != nullptr && (warden.warden->emergeTicks > 0 || warden.warden->digTicks > 0);
}

bool isBusy(const ServerEntity& warden) {
    return warden.warden != nullptr &&
           (isDiggingOrEmerging(warden) || warden.warden->roarTicks > 0 || warden.warden->sonicTicks > 0);
}

Pose poseOf(const ServerEntity& warden) {
    if (warden.warden == nullptr) {
        return Pose::Standing;
    }
    if (warden.warden->emergeTicks > 0) {
        return Pose::Emerging;
    }
    if (warden.warden->digTicks > 0) {
        return Pose::Digging;
    }
    if (warden.warden->roarTicks > 0) {
        return Pose::Roaring;
    }
    return warden.warden->sniffTicks > 0 ? Pose::Sniffing : Pose::Standing;
}

std::vector<Action> startEmerging(ServerEntity& warden) {
    WardenState& state = stateOf(warden);
    state.emergeTicks = kEmergeTicks;
    // parity: `finalizeSpawn`'s agitated growl, then `Emerging.start`'s own sound.
    return {pose(Pose::Emerging), sound("entity.warden.agitated", 5.0F), sound("entity.warden.emerge", 5.0F)};
}

std::vector<Action> startRoar(ServerEntity& warden, std::int32_t targetId) {
    WardenState& state = stateOf(warden);
    if (state.roarTicks > 0 || isDiggingOrEmerging(warden)) {
        return {};
    }
    state.roarTarget = targetId;
    state.roarTicks = kRoarTicks;
    // parity: `Roar.start` raises the anger by twenty and turns to face the target.
    static_cast<void>(wardens::anger(warden, targetId, kRoarAnger));
    return {pose(Pose::Roaring)};
}

void refreshDigCooldown(ServerEntity& warden) {
    WardenState& state = stateOf(warden);
    if (state.digCooldown > 0) {
        state.digCooldown = kDigCooldown;
    }
}

void setDisturbance(ServerEntity& warden, const std::array<std::int32_t, 3>& at) {
    WardenState& state = stateOf(warden);
    if (warden.targetId.has_value() || wardens::target(warden).has_value()) {
        return;
    }
    refreshDigCooldown(warden);
    state.sniffCooldown = kDisturbanceTicks;
    state.disturbance = at;
    state.disturbanceTicks = kDisturbanceTicks;
    ai::navigation::stop(warden);
}

void offer(WardenState& state, Heard heard) {
    if (state.candidate.has_value()) {
        const Heard& standing = *state.candidate;
        if (heard.tick != standing.tick || heard.distance > standing.distance) {
            return;
        }
        if (heard.distance == standing.distance &&
            world::vibrations::frequencyOf(heard.event) <= world::vibrations::frequencyOf(standing.event)) {
            return;
        }
    }
    state.candidate = std::move(heard);
}

std::vector<Action> meleeHit(ServerEntity& warden) {
    WardenState& state = stateOf(warden);
    state.sonicCooldown = kSonicCooldown;
    return {Action{.kind = Action::Kind::EntityEvent, .event = kAttackEvent},
            sound("entity.warden.attack_impact", 10.0F)};
}

std::vector<Action> tick(ServerEntity& warden, const View& view) {
    WardenState& state = stateOf(warden);
    std::vector<Action> actions;
    if (view.persistent) {
        refreshDigCooldown(warden);
    }
    if (state.digCooldown > 0) {
        state.digCooldown--;
    }
    if (state.sonicCooldown > 0) {
        state.sonicCooldown--;
    }
    // parity: the memories that expire: sniffing may start again, and the disturbance is forgotten.
    if (state.sniffCooldown > 0) {
        state.sniffCooldown--;
    }
    if (state.disturbanceTicks > 0 && --state.disturbanceTicks == 0) {
        state.disturbance.reset();
    }
    // parity: `EMERGE` comes first and holds everything else until it is done.
    if (state.emergeTicks > 0) {
        if (--state.emergeTicks == 0) {
            actions.push_back(pose(Pose::Standing));
        }
        return actions;
    }
    if (state.digTicks > 0) {
        if (--state.digTicks == 0) {
            actions.push_back(Action{.kind = Action::Kind::Discard});
        }
        return actions;
    }
    if (state.roarTicks > 0) {
        const std::int32_t elapsed = kRoarTicks - state.roarTicks;
        if (elapsed == kRoarSoundAt) {
            actions.push_back(sound("entity.warden.roar", 3.0F));
        }
        if (--state.roarTicks == 0) {
            actions.push_back(pose(Pose::Standing));
            if (state.roarTarget.has_value()) {
                // parity: `Warden.setAttackTarget`, which holds the first boom back ten seconds.
                actions.push_back(Action{.kind = Action::Kind::StartFight, .targetId = *state.roarTarget});
                state.sonicCooldown = kFirstSonicCooldown;
            }
            state.roarTarget.reset();
        }
        return actions;
    }
    if (view.target.has_value()) {
        // parity: the fight activity keeps a running dig cooldown running.
        refreshDigCooldown(warden);
        if (state.sonicTicks > 0) {
            state.sonicTicks++;
            if (state.sonicTicks == kSonicBlastAt + 1 && inSonicReach(warden, *view.target)) {
                actions.push_back(Action{.kind = Action::Kind::SonicBlast});
            }
            if (state.sonicTicks > kSonicTicks) {
                state.sonicTicks = 0;
                state.sonicCooldown = kSonicCooldown;
            }
        } else if (state.sonicCooldown == 0 && inSonicReach(warden, *view.target)) {
            state.sonicTicks = 1;
            actions.push_back(Action{.kind = Action::Kind::EntityEvent, .event = kSonicEvent});
            actions.push_back(sound("entity.warden.sonic_charge", 3.0F));
        }
        return actions;
    }
    state.sonicTicks = 0;
    // parity: `DIG` once the cooldown is gone and nothing is left to roar at or fight.
    if (state.digCooldown == 0 && (view.onGround || view.inFluid)) {
        if (view.onGround) {
            state.digTicks = kDigTicks;
            actions.push_back(pose(Pose::Digging));
            actions.push_back(sound("entity.warden.dig", 5.0F));
        } else {
            actions.push_back(sound("entity.warden.agitated", 5.0F));
            actions.push_back(Action{.kind = Action::Kind::Discard});
        }
    }
    return actions;
}

} // namespace basalt::server::runtime::entity::wardenBrain
