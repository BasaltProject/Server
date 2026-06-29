#include "MobStateTable.hpp"

#include <basalt/server/runtime/entity/FishingHooks.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

using entity::ServerEntity;
using entity::fishingHooks::HookState;
using namespace values;

constexpr auto kHook = &ServerEntity::hook;
constexpr double kPullScale = 0.1;

} // namespace

void addFishingHook(Table& table, Operations& operations) {
    table["hook_min_wait"] = viaState(kHook, &HookState::minWaitTime);
    table["hook_max_wait"] = viaState(kHook, &HookState::maxWaitTime);
    table["hook_min_lure"] = viaState(kHook, &HookState::minLureTime);
    table["hook_max_lure"] = viaState(kHook, &HookState::maxLureTime);
    table["hook_min_angle"] = viaState(kHook, &HookState::minLureAngle);
    table["hook_max_angle"] = viaState(kHook, &HookState::maxLureAngle);
    table["hook_apply_lure"] = viaState(kHook, &HookState::applyLure);
    table["hook_rain_influenced"] = viaState(kHook, &HookState::rainInfluenced);
    table["hook_sky_influenced"] = viaState(kHook, &HookState::skyInfluenced);
    table["hook_bite_chance"] = viaState(kHook, &HookState::biteChance);
    table["hook_wait"] = viaState(kHook, &HookState::timeUntilLured);
    table["hook_open_water"] = viaState(kHook, &HookState::openWater, false);
    table["hook_phase"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return entity.hook == nullptr ? Value{} : Value{static_cast<std::int64_t>(entity.hook->phase)};
                 },
                 nullptr};
    table["hook_hooked"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return entity.hook == nullptr
                                ? Value{}
                                : Value{static_cast<std::int64_t>(entity.hook->hookedIn.value_or(-1))};
                 },
                 [](ServerEntity& entity, const Value& value) {
                     if (entity.hook == nullptr) {
                         return false;
                     }
                     const std::int64_t id = asInt(value, -1);
                     entity.hook->hookedIn = id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
                     return true;
                 }};
    table["hook_bite_in"] = Accessor{
        [](const ServerEntity& entity) -> Value {
            return entity.hook == nullptr ? Value{} : Value{static_cast<std::int64_t>(entity.hook->timeUntilHooked)};
        },
        [](ServerEntity& entity, const Value& value) {
            HookState* state = entity.hook.get();
            if (state == nullptr) {
                return false;
            }
            const auto ticks = static_cast<std::int32_t>(asInt(value));
            if (!(state->timeUntilHooked > 0 && state->timeUntilLured <= 0)) {
                state->fishAngle =
                    entity::fishingHooks::nextFloatBetween(state->random, state->minLureAngle, state->maxLureAngle);
                state->timeUntilLured = 0;
            }
            state->timeUntilHooked = ticks;
            return true;
        }};
    operations["hook_reset"] = [](const Environment&, ServerEntity& entity, const Arguments&) -> std::optional<Value> {
        HookState* state = entity.hook.get();
        if (state == nullptr) {
            return false;
        }
        entity::fishingHooks::resetTimeUntilLured(*state);
        state->timeUntilHooked = 0;
        return true;
    };
    operations["hook_pull"] = [](const Environment& environment, ServerEntity& entity,
                                 const Arguments&) -> std::optional<Value> {
        const HookState* state = entity.hook.get();
        if (state == nullptr || !state->hookedIn.has_value() || environment.world == nullptr) {
            return false;
        }
        double ownerX = entity.x;
        double ownerY = entity.y;
        double ownerZ = entity.z;
        if (environment.players) {
            for (const player::PlayerSession* session : environment.players()) {
                if (session->entityId() == state->ownerId) {
                    ownerX = session->x;
                    ownerY = session->y;
                    ownerZ = session->z;
                }
            }
        }
        ServerEntity* caught = environment.world->state().entity(*state->hookedIn);
        if (caught == nullptr) {
            return false;
        }
        caught->velocityX += (ownerX - entity.x) * kPullScale;
        caught->velocityY += (ownerY - entity.y) * kPullScale;
        caught->velocityZ += (ownerZ - entity.z) * kPullScale;
        return true;
    };
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
