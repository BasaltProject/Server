#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/HorseGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>

namespace basalt::server::runtime::entity::ai {
namespace {

/// parity: `AbstractHorse.getAmbientSoundInterval`, which is also its stand interval.
constexpr std::int32_t kStandInterval = 400;

} // namespace

RunAroundLikeCrazyGoal::RunAroundLikeCrazyGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool RunAroundLikeCrazyGoal::canUse(MobContext& mob) {
    if (horses::tamed(mob.mob) || mob.mob.passengerIds.empty()) {
        return false;
    }
    // parity: `ZombieHorse.isMobControlled`, a mob in the saddle.
    if (mob.mob.type().path() == "zombie_horse" && mob.playerById(mob.mob.passengerIds.front()) == nullptr) {
        return false;
    }
    // parity: `DefaultRandomPos.getPos(horse, 5, 4)`.
    wanted_ = mobBrain::strollTarget(mob.world, mob.mob, mob.random, 5, 4);
    return wanted_.has_value();
}

bool RunAroundLikeCrazyGoal::canContinueToUse(MobContext& mob) {
    return !horses::tamed(mob.mob) && !navigation::isDone(mob.mob) && !mob.mob.passengerIds.empty();
}

void RunAroundLikeCrazyGoal::start(MobContext& mob) {
    if (wanted_.has_value()) {
        static_cast<void>(
            navigation::moveTo(mob.world, mob.mob, (*wanted_)[0], (*wanted_)[1], (*wanted_)[2], speedModifier_));
    }
}

void RunAroundLikeCrazyGoal::tick(MobContext& mob) {
    constexpr std::int32_t kBuckOdds = 50;
    if (horses::tamed(mob.mob) || mob.random.nextInt(adjustedTickDelay(kBuckOdds)) != 0) {
        return;
    }
    if (mob.mob.passengerIds.empty()) {
        return;
    }
    const std::int32_t rider = mob.mob.passengerIds.front();
    if (mob.playerById(rider) != nullptr) {
        // parity: the more it has been fed, the likelier it takes to its rider.
        if (mob.random.nextInt(horses::maxTemper(mob.mob)) < horses::temper(mob.mob)) {
            mob.actions.tameHorse(mob.mob, rider);
            return;
        }
        static_cast<void>(horses::modifyTemper(mob.mob, 5));
    }
    mob.actions.throwRiders(mob.mob);
}

RandomStandGoal::RandomStandGoal() : nextStand_(-kStandInterval) {}

bool RandomStandGoal::canUse(MobContext& mob) {
    ++nextStand_;
    if (nextStand_ <= 0 || mob.random.nextInt(1000) >= nextStand_) {
        return false;
    }
    nextStand_ = -kStandInterval;
    const horses::HorseState* state = mob.mob.horse.get();
    const bool immobile = state != nullptr && (state->eating || state->standing);
    return !immobile && mob.random.nextInt(10) == 0;
}

void RandomStandGoal::start(MobContext& mob) { mob.actions.horseRears(mob.mob); }

} // namespace basalt::server::runtime::entity::ai
