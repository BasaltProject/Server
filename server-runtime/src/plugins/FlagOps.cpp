#include "MobStateTable.hpp"

#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Frogs.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

using entity::ServerEntity;
using namespace values;

template <typename T>
[[nodiscard]] Accessor plain(T ServerEntity::* member) {
    return Accessor{[member](const ServerEntity& entity) -> Value { return toValue(entity.*member); },
                    [member](ServerEntity& entity, const Value& value) {
                        fromValue(entity.*member, value);
                        return true;
                    }};
}

[[nodiscard]] MobActionSystem* mobSystem(const Environment& environment) {
    return environment.hooks != nullptr && environment.hooks->mobSystem ? environment.hooks->mobSystem() : nullptr;
}

[[nodiscard]] Value positionText(const std::optional<std::array<std::int32_t, 3>>& at) {
    if (!at.has_value()) {
        return Value{};
    }
    return std::to_string((*at)[0]) + "," + std::to_string((*at)[1]) + "," + std::to_string((*at)[2]);
}

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> parsePosition(const Value& value) {
    const std::optional<std::string> text = asText(value);
    if (!text.has_value() || text->empty()) {
        return std::nullopt;
    }
    std::array<std::int32_t, 3> at{};
    std::size_t from = 0;
    for (std::int32_t& axis : at) {
        const std::size_t end = text->find(',', from);
        axis = std::atoi(text->substr(from, end == std::string::npos ? end : end - from).c_str());
        from = end == std::string::npos ? text->size() : end + 1;
    }
    return at;
}

[[nodiscard]] Accessor positionOf(std::optional<std::array<std::int32_t, 3>> ServerEntity::* member) {
    return Accessor{[member](const ServerEntity& entity) -> Value { return positionText(entity.*member); },
                    [member](ServerEntity& entity, const Value& value) {
                        entity.*member = parsePosition(value);
                        return true;
                    }};
}

[[nodiscard]] Accessor idOf(std::optional<std::int32_t> ServerEntity::* member) {
    return Accessor{[member](const ServerEntity& entity) -> Value {
                        return static_cast<std::int64_t>((entity.*member).value_or(-1));
                    },
                    [member](ServerEntity& entity, const Value& value) {
                        const std::int64_t id = asInt(value, -1);
                        entity.*member = id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
                        return true;
                    }};
}

void ensurePiglin(ServerEntity& entity) {
    if (entity::piglins::isPiglin(entity.type())) {
        static_cast<void>(entity::piglins::stateOf(entity));
    }
}

void ensureAllay(ServerEntity& entity) {
    if (entity::allays::isAllay(entity.type())) {
        static_cast<void>(entity::allays::stateOf(entity));
    }
}

void ensureBullet(ServerEntity& entity) {
    if (entity::shulkers::isBullet(entity.type())) {
        entity.bullet = std::make_shared<entity::shulkers::BulletState>();
    }
}

void addFlags(Table& table) {
    table["bat_resting"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.flight.resting; }, nullptr};
    table["wolf_interested"] = plain(&ServerEntity::begging);
    table["neutral_anger_end"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return static_cast<std::int64_t>(entity.neutral == nullptr ? -1 : entity.neutral->endTime);
                 },
                 [](ServerEntity& entity, const Value& value) {
                     if (!entity::neutralAnger::isNeutral(entity.type())) {
                         return false;
                     }
                     entity::neutralAnger::stateOf(entity).endTime = asInt(value, -1);
                     return true;
                 }};
    table["skeleton_trap"] = plain(&ServerEntity::skeletonTrap);
    table["skeleton_trap_time"] = plain(&ServerEntity::skeletonTrapTime);
    table["spell"] = Accessor{
        [](const ServerEntity& entity) -> Value { return static_cast<std::int64_t>(entity.spellType); }, nullptr};
    table["cat_lying"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.lying; }, nullptr};
    table["cat_head_up"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.relaxOne; }, nullptr};
    table["puff_state"] = plain(&ServerEntity::puffState);
    table["despawn_delay"] = plain(&ServerEntity::despawnDelay);
    table["wander_target"] = positionOf(&ServerEntity::wanderTarget);
    table["fox_flags"] = plain(&ServerEntity::foxFlags);
    table["panda_flags"] = plain(&ServerEntity::pandaFlags);
    table["panda_unhappy"] = plain(&ServerEntity::pandaUnhappy);
    table["panda_sneeze"] = plain(&ServerEntity::pandaSneeze);
    table["panda_eat"] = plain(&ServerEntity::pandaEat);
    table["charging"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.charging; }, nullptr};
    table["immune_to_zombification"] = plain(&ServerEntity::immuneToZombification);
    table["moistness"] = plain(&ServerEntity::moistness);
    table["got_fish"] = plain(&ServerEntity::gotFish);
    table["dolphin_treasure"] = positionOf(&ServerEntity::treasurePos);
    table["guardian_laser_ticks"] = plain(&ServerEntity::beamTicks);
    table["guardian_laser_target"] = idOf(&ServerEntity::beamTargetId);
    table["raid_wave"] = plain(&ServerEntity::raidWave);
    table["raid_can_join"] = plain(&ServerEntity::canJoinRaid);
    table["raid_ticks_outside"] = plain(&ServerEntity::ticksOutsideRaid);
    table["raid_celebrating"] =
        Accessor{[](const ServerEntity& entity) -> Value { return entity.celebrating; }, nullptr};
    table["raid_id"] = idOf(&ServerEntity::raidId);
    table["patrol_leader"] = plain(&ServerEntity::patrolLeader);
    table["patrol_target"] = positionOf(&ServerEntity::patrolTarget);
    table["llama_caravan_head"] = idOf(&ServerEntity::caravanHead);
    table["llama_caravan_tail"] = idOf(&ServerEntity::caravanTail);
    table["enderman_stared_at"] = Accessor{
        [](const ServerEntity& entity) -> Value { return entity.neutral != nullptr && entity.neutral->staredAt; },
        [](ServerEntity& entity, const Value& value) {
            if (entity.type().path() != "enderman") {
                return false;
            }
            entity::neutralAnger::stateOf(entity).staredAt = asBool(value);
            return true;
        }};
    table["piglin_cannot_hunt"] =
        viaState(&ServerEntity::piglin, &entity::piglins::PiglinState::cannotHunt, true, ensurePiglin);
    table["piglin_dancing"] =
        viaState(&ServerEntity::piglin, &entity::piglins::PiglinState::dancing, true, ensurePiglin);
    table["piglin_charging"] =
        viaState(&ServerEntity::piglin, &entity::piglins::PiglinState::chargingCrossbow, true, ensurePiglin);
    table["allay_duplication_cooldown"] =
        viaState(&ServerEntity::allay, &entity::allays::AllayState::duplicationCooldown, true, ensureAllay);
    table["frog_tongue_target"] = Accessor{
        [](const ServerEntity& entity) -> Value {
            return static_cast<std::int64_t>(entity.frog == nullptr ? -1 : entity.frog->tongueTarget.value_or(-1));
        },
        [](ServerEntity& entity, const Value& value) {
            if (entity.type().path() != "frog") {
                return false;
            }
            const std::int64_t id = asInt(value, -1);
            entity::frogs::stateOf(entity).tongueTarget =
                id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
            return true;
        }};
}

void addBullet(Table& table) {
    constexpr auto kBullet = &ServerEntity::bullet;
    using entity::shulkers::BulletState;
    table["shulker_peek"] = viaState(&ServerEntity::shulker, &entity::shulkers::ShulkerState::peek, false);
    table["shulker_face"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return static_cast<std::int64_t>(entity.shulker == nullptr ? server::registry::BlockFace::Down
                                                                                : entity.shulker->attachFace);
                 },
                 [](ServerEntity& entity, const Value& value) {
                     if (!entity::shulkers::isShulker(entity.type())) {
                         return false;
                     }
                     const std::int64_t face = asInt(value, -1);
                     if (face < 0 || face > 5) {
                         return false;
                     }
                     entity::shulkers::stateOf(entity).attachFace = static_cast<server::registry::BlockFace>(face);
                     entity::shulkers::save(entity);
                     return true;
                 }};
    table["bullet_steps"] = viaState(kBullet, &BulletState::flightSteps, true, ensureBullet);
    table["bullet_delta_x"] = viaState(kBullet, &BulletState::targetDeltaX, true, ensureBullet);
    table["bullet_delta_y"] = viaState(kBullet, &BulletState::targetDeltaY, true, ensureBullet);
    table["bullet_delta_z"] = viaState(kBullet, &BulletState::targetDeltaZ, true, ensureBullet);
    table["bullet_target"] = Accessor{
        [](const ServerEntity& entity) -> Value {
            return static_cast<std::int64_t>(entity.bullet == nullptr ? -1 : entity.bullet->target.value_or(-1));
        },
        [](ServerEntity& entity, const Value& value) {
            ensureBullet(entity);
            if (entity.bullet == nullptr) {
                return false;
            }
            const std::int64_t id = asInt(value, -1);
            entity.bullet->target = id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
            return true;
        }};
    table["bullet_direction"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return static_cast<std::int64_t>(entity.bullet == nullptr || !entity.bullet->direction.has_value()
                                                          ? -1
                                                          : static_cast<std::int64_t>(*entity.bullet->direction));
                 },
                 [](ServerEntity& entity, const Value& value) {
                     if (entity.bullet == nullptr) {
                         return false;
                     }
                     const std::int64_t face = asInt(value, -1);
                     entity.bullet->direction = face < 0 || face > 5
                                                    ? std::nullopt
                                                    : std::optional{static_cast<server::registry::BlockFace>(face)};
                     return true;
                 }};
}

using Step = bool (MobActionSystem::*)(ServerEntity&, bool);

void withFlag(Operations& operations, std::string_view name, Step step) {
    operations[name] = [step](const Environment& environment, ServerEntity& entity,
                              const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && (system->*step)(entity, asBool(at(arguments, 0)));
    };
}

void addActions(Operations& operations) {
    operations["set_charging"] = [](const Environment& environment, ServerEntity& entity,
                                    const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        if (system == nullptr) {
            return false;
        }
        system->setCharging(entity, asBool(at(arguments, 0)));
        return true;
    };
    operations["shulker_set_peek"] = [](const Environment& environment, ServerEntity& entity,
                                        const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        if (system == nullptr || !entity::shulkers::isShulker(entity.type())) {
            return false;
        }
        system->shulkerPeek(entity,
                            static_cast<std::int32_t>(std::clamp<std::int64_t>(asInt(at(arguments, 0)), 0, 100)));
        return true;
    };
    operations["bat_awake"] = [](const Environment& environment, ServerEntity& entity,
                                 const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        if (system == nullptr || entity.type().path() != "bat") {
            return false;
        }
        entity.flight.resting = !asBool(at(arguments, 0));
        system->poseChanged(entity);
        return true;
    };
    operations["set_sleeping"] = [](const Environment& environment, ServerEntity& entity,
                                    const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        if (system == nullptr) {
            return false;
        }
        system->setSleeping(entity, asBool(at(arguments, 0)));
        return true;
    };
    operations["converting"] = [](const Environment& environment, ServerEntity& entity,
                                  const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginConverting(entity);
    };
    operations["set_spell"] = [](const Environment& environment, ServerEntity& entity,
                                 const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginSpell(entity, static_cast<std::int32_t>(asInt(at(arguments, 0), -1)));
    };
    withFlag(operations, "cat_lying", &MobActionSystem::pluginCatLying);
    withFlag(operations, "cat_head_up", &MobActionSystem::pluginCatRelaxed);
    withFlag(operations, "raider_celebrate", &MobActionSystem::pluginCelebrate);
    withFlag(operations, "guardian_laser", &MobActionSystem::pluginGuardianLaser);
    operations["enderman_teleport"] = [](const Environment& environment, ServerEntity& entity,
                                         const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr &&
               system->pluginEnderTeleport(entity, asInt(at(arguments, 0)) != 0, asReal(at(arguments, 1)),
                                           asReal(at(arguments, 2)), asReal(at(arguments, 3)));
    };
    operations["llama_join_caravan"] = [](const Environment& environment, ServerEntity& entity,
                                          const Arguments& arguments) -> std::optional<Value> {
        if (environment.world == nullptr || !entity::horses::isLlama(entity.type())) {
            return false;
        }
        ServerEntity* head = environment.world->state().entity(static_cast<std::int32_t>(asInt(at(arguments, 0), -1)));
        if (head == nullptr || head == &entity || !entity::horses::isLlama(head->type())) {
            return false;
        }
        entity.caravanHead = head->entityId();
        head->caravanTail = entity.entityId();
        return true;
    };
    operations["llama_leave_caravan"] = [](const Environment& environment, ServerEntity& entity,
                                           const Arguments&) -> std::optional<Value> {
        if (environment.world == nullptr || !entity.caravanHead.has_value()) {
            return false;
        }
        if (ServerEntity* head = environment.world->state().entity(*entity.caravanHead); head != nullptr) {
            head->caravanTail.reset();
        }
        entity.caravanHead.reset();
        return true;
    };
}

} // namespace

void addFlagOps(Table& table, Operations& operations) {
    addFlags(table);
    addBullet(table);
    addActions(operations);
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
