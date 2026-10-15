#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
namespace camels = entity::camels;
namespace kit = entity::ai::brainKit;
using camels::CamelState;

[[nodiscard]] bool husk(const ServerEntity& camel) { return camel.type().path() == "camel_husk"; }

/// parity: `Camel.canCamelChangePose`: room for the pose it would change to.
[[nodiscard]] bool canChangePose(const world::WorldState& world, const ServerEntity& camel, const CamelState& state) {
    if (!camels::sitting(state)) {
        return true;
    }
    const entity::entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    return !entity::entityCollision::collides(
        blockAt, entity::Aabb::of(camel.x, camel.y, camel.z, camel.width(), camel.height()));
}

} // namespace

namespace entity::camels {

bool isCamel(const wire::common::Identifier& type) { return type.path() == "camel" || type.path() == "camel_husk"; }

CamelState& stateOf(ServerEntity& camel) {
    if (camel.camel == nullptr) {
        camel.camel = std::make_shared<CamelState>();
        if (camel.sourceNbt.has_value()) {
            camel.camel->lastPoseTick = camel.sourceNbt->getLong("LastPoseTick");
        }
    }
    return *camel.camel;
}

bool sitting(const CamelState& state) { return state.lastPoseTick < 0; }

std::int64_t poseTime(const CamelState& state, std::int64_t gameTime) {
    return gameTime - std::abs(state.lastPoseTick);
}

bool refusesToMove(const CamelState& state, std::int64_t gameTime) {
    return sitting(state) || poseTime(state, gameTime) < (sitting(state) ? kSitDownTicks : kStandUpTicks);
}

void write(const ServerEntity& camel, wire::nbt::NbtCompound& tag) {
    if (camel.camel != nullptr) {
        tag.putLong("LastPoseTick", camel.camel->lastPoseTick);
    }
}

} // namespace entity::camels

void MobActionSystem::tickCamel(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& camel = context.mob;
    CamelState& state = camels::stateOf(camel);
    const std::int64_t now = context.gameTime;
    const bool inWater = entity::ai::inWater(context.world, camel);
    const std::string_view prefix = husk(camel) ? "entity.camel_husk." : "entity.camel.";
    const auto sound = [&](std::string_view name) {
        makeSound(world, camel, std::string{prefix} + std::string{name}, 1.0F);
    };
    const auto poseChange = [&](std::int64_t lastPoseTick) {
        state.lastPoseTick = lastPoseTick;
        world::vibrations::emit(world.state(), "entity_action", camel.x, camel.y, camel.z, now,
                                {.entity = camel.uuid()});
    };
    // parity: `standUpInstantly`, the pose skipping straight to fully up.
    const auto standUpInstantly = [&] { poseChange(std::max<std::int64_t>(0, now - camels::kStandUpTicks - 1)); };
    // parity: `actuallyHurt` and `tick`: a hit or water gets it up at once.
    if ((state.lastHealth >= 0.0 && camel.health < state.lastHealth) || (camels::sitting(state) && inWater)) {
        standUpInstantly();
    }
    state.lastHealth = camel.health;
    // parity: the core: `Swim(0.8)`, `CamelPanic(4.0)` and the countdowns.
    if (inWater && context.random.nextFloat() < 0.8F) {
        camel.velocityY += 0.04;
    }
    const bool mobControlled = husk(camel) && !camel.passengerIds.empty();
    const bool wasPanicking = state.panicTicks > 0;
    const bool panicking = !mobControlled && kit::panic(context, state.motion, state.panicTicks, 4.0F);
    if (panicking && !wasPanicking) {
        standUpInstantly();
    }
    if (state.gazeCooldown > 0) {
        --state.gazeCooldown;
    }
    const bool refuses = camels::refusesToMove(state, now);
    // parity: the idle activity, in its priority order.
    kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
    state.breedTarget = kit::makeLove(context, state.motion, state.look, 1.0F, 2);
    // parity: the `RunOne` of following food and following an adult.
    const auto tempts = [this, &camel](const wire::common::Identifier& item) {
        return itemTagged(husk(camel) ? "minecraft:camel_husk_food" : "minecraft:camel_food", item);
    };
    const double closeEnough = camel.age < 0 ? 2.5 : 3.5;
    bool temptAsked = false;
    if (state.temptation.following) {
        temptAsked = true;
        static_cast<void>(kit::tempt(context, state.motion, state.look, state.temptation, 2.5F, tempts, closeEnough));
    } else {
        for (const std::size_t choice : kit::shuffled(context.random, {1, 1})) {
            if (choice == 0) {
                temptAsked = true;
                if (kit::tempt(context, state.motion, state.look, state.temptation, 2.5F, tempts, closeEnough)) {
                    break;
                }
            } else if (!refuses && kit::followAdult(context, state.motion, state.look, 5, 16, 2.5F)) {
                break;
            }
        }
    }
    if (!temptAsked && state.temptation.cooldown > 0) {
        --state.temptation.cooldown;
    }
    kit::lookAround(context, state.look, state.gazeCooldown, 150, 250, 30.0F);
    // parity: the idle `RunOne` of a stroll.
    if (state.idleTicks > 0) {
        --state.idleTicks;
    } else if (!state.motion.walk.has_value()) {
        for (const std::size_t choice : kit::shuffled(context.random, {1, 1, 1, 1})) {
            if (choice == 0 && !refuses) {
                if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                    kit::walkTo(state.motion, *spot, 2.0F, 0);
                }
                break;
            }
            if (choice == 1 && !refuses && (state.look.entity.has_value() || state.look.at.has_value())) {
                std::optional<kit::Vec> at = state.look.at;
                if (state.look.entity.has_value()) {
                    at = kit::positionOf(context, *state.look.entity, false);
                }
                if (at.has_value()) {
                    kit::walkTo(state.motion, *at, 2.0F, 3);
                }
                break;
            }
            if (choice == 2 && !inWater && camels::poseTime(state, now) >= camels::kMinimalPoseTicks &&
                !camel.leashHolderId.has_value() && camel.onGround && camel.passengerIds.empty() &&
                canChangePose(context.world, camel, state)) {
                // parity: `RandomSitting.start`: up if down, down if up and not running scared.
                if (camels::sitting(state)) {
                    sound("stand");
                    poseChange(now);
                } else if (!panicking) {
                    sound("sit");
                    poseChange(-now);
                }
                break;
            }
            if (choice == 3) {
                state.idleTicks = 30 + context.random.nextInt(31);
                break;
            }
        }
    }
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
    // parity: `Camel.travel`: on the ground and not standing, it goes nowhere.
    if (camels::refusesToMove(state, now) && camel.onGround) {
        camel.velocityX = 0.0;
        camel.velocityZ = 0.0;
        context.speed = 0.0;
    }
}

void MobActionSystem::camelRidden(world::LoadedWorld& world, ServerEntity& camel, const player::PlayerSession& rider) {
    // parity: `Camel.tickRidden`: forward on the keys gets a settled sitting camel up.
    CamelState& state = camels::stateOf(camel);
    const std::int64_t now = world.gameTime();
    const bool forward = rider.inputForward && !rider.inputBack;
    if (!forward || !camels::sitting(state) || camels::poseTime(state, now) < camels::kSitDownTicks) {
        return;
    }
    makeSound(world, camel, husk(camel) ? "entity.camel_husk.stand" : "entity.camel.stand", 1.0F);
    state.lastPoseTick = now;
    world::vibrations::emit(world.state(), "entity_action", camel.x, camel.y, camel.z, now, {.entity = camel.uuid()});
}

} // namespace basalt::server::runtime
