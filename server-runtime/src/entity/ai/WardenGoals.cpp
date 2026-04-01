#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/WardenGoals.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <limits>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace mth = wire::common::mth;
namespace brain = wardenBrain;

/// parity: `NearestLivingEntitySensor`'s sixteen blocks and `Sniffing`'s six across, twenty up.
constexpr double kSenseRange = 16.0;
constexpr double kSniffAngerXZ = 6.0;
constexpr double kSniffAngerY = 20.0;
constexpr double kInvestigateSpeed = static_cast<double>(0.7F);

/// Where the nearest thing it may fight stands, and its id. parity: `WardenEntitySensor`: the
/// nearest player first, and only then anything else.
struct Nearest {
    std::int32_t id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] std::optional<Nearest> nearestAttackable(const MobContext& mob) {
    std::optional<Nearest> best;
    double bestDistance = std::numeric_limits<double>::max();
    const auto inBox = [&mob](double x, double y, double z) {
        return std::abs(x - mob.mob.x) <= kSenseRange + 1.0 && std::abs(z - mob.mob.z) <= kSenseRange + 1.0 &&
               std::abs(y - mob.mob.y) <= kSenseRange + mob.mob.height();
    };
    for (const PlayerView& player : mob.players) {
        const double distance = mob.distanceSquaredTo(player.x, player.y, player.z);
        if (player.targetable && inBox(player.x, player.y, player.z) && distance < bestDistance) {
            bestDistance = distance;
            best = Nearest{.id = player.entityId, .x = player.x, .y = player.y, .z = player.z};
        }
    }
    if (best.has_value()) {
        return best;
    }
    for (const ServerEntity* other : mob.entities) {
        if (other == &mob.mob || other->removed || other->dying ||
            !registry::entityAttributes::isLiving(other->type()) || wardens::isWarden(other->type()) ||
            other->type().path() == "armor_stand" || !inBox(other->x, other->y, other->z)) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(other->x, other->y, other->z);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = Nearest{.id = other->entityId(), .x = other->x, .y = other->y, .z = other->z};
        }
    }
    return best;
}

} // namespace

bool WardenInvestigateGoal::canUse(MobContext& mob) {
    const brain::WardenState* state = mob.mob.warden.get();
    return state != nullptr && state->disturbance.has_value() && !mob.mob.targetId.has_value() &&
           !brain::isBusy(mob.mob);
}

void WardenInvestigateGoal::tick(MobContext& mob) {
    const brain::WardenState& state = *mob.mob.warden;
    if (!state.disturbance.has_value() || !navigation::isDone(mob.mob)) {
        return;
    }
    const std::array<std::int32_t, 3>& at = *state.disturbance;
    const double dx = at[0] - mth::floor(mob.mob.x);
    const double dy = at[1] - mth::floor(mob.mob.y);
    const double dz = at[2] - mth::floor(mob.mob.z);
    if ((dx * dx) + (dy * dy) + (dz * dz) < 4.0) {
        return;
    }
    // parity: `GoToTargetLocation.getNearbyPos`, a block either side on each flat axis.
    const std::int32_t offsetX = mob.random.nextInt(3) - 1;
    const std::int32_t offsetZ = mob.random.nextInt(3) - 1;
    lookControl::lookAt(mob.mob, at[0] + 0.5, at[1] + 0.5, at[2] + 0.5, 45.0F, 90.0F);
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, at[0] + offsetX + 0.5, static_cast<double>(at[1]),
                                         at[2] + offsetZ + 0.5, kInvestigateSpeed));
}

bool WardenSniffGoal::canUse(MobContext& mob) {
    const brain::WardenState* state = mob.mob.warden.get();
    return state != nullptr && state->sniffCooldown == 0 && !state->disturbance.has_value() &&
           !mob.mob.targetId.has_value() && !brain::isBusy(mob.mob) && nearestAttackable(mob).has_value();
}

bool WardenSniffGoal::canContinueToUse(MobContext& mob) {
    return mob.mob.warden->sniffTicks > 0 && !mob.mob.targetId.has_value();
}

void WardenSniffGoal::start(MobContext& mob) {
    brain::WardenState& state = brain::stateOf(mob.mob);
    state.sniffTicks = brain::kSniffTicks;
    state.sniffCooldown =
        brain::kSniffCooldownMin + mob.random.nextInt(brain::kSniffCooldownMax - brain::kSniffCooldownMin + 1);
    navigation::stop(mob.mob);
    mob.actions.mobSound(mob.mob, "entity.warden.sniff");
}

void WardenSniffGoal::stop(MobContext& mob) { mob.mob.warden->sniffTicks = 0; }

void WardenSniffGoal::tick(MobContext& mob) {
    brain::WardenState& state = *mob.mob.warden;
    if (state.sniffTicks <= 0 || --state.sniffTicks > 0) {
        return;
    }
    const std::optional<Nearest> nearest = nearestAttackable(mob);
    if (!nearest.has_value()) {
        return;
    }
    const double dx = nearest->x - mob.mob.x;
    const double dz = nearest->z - mob.mob.z;
    if ((dx * dx) + (dz * dz) < kSniffAngerXZ * kSniffAngerXZ && std::abs(nearest->y - mob.mob.y) < kSniffAngerY) {
        mob.actions.wardenAngered(mob.mob, nearest->id, wardens::kHeard, true);
    }
    if (!state.disturbance.has_value()) {
        brain::setDisturbance(mob.mob, {mth::floor(nearest->x), mth::floor(nearest->y), mth::floor(nearest->z)});
    }
}

} // namespace basalt::server::runtime::entity::ai
