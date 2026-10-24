#include "MobStateTable.hpp"

#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Axolotls.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <limits>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

using entity::ServerEntity;
using namespace values;

[[nodiscard]] MobActionSystem* mobSystem(const Environment& environment) {
    return environment.hooks != nullptr && environment.hooks->mobSystem ? environment.hooks->mobSystem() : nullptr;
}

[[nodiscard]] Value position(const std::optional<std::array<std::int32_t, 3>>& at) {
    if (!at.has_value()) {
        return Value{};
    }
    return std::to_string((*at)[0]) + "," + std::to_string((*at)[1]) + "," + std::to_string((*at)[2]);
}

void ensureBee(ServerEntity& entity) {
    if (entity::bees::isBee(entity.type())) {
        random::RandomSource random = random::RandomSource::legacy(0);
        static_cast<void>(entity::bees::stateOf(entity, random));
    }
}

void ensureAllay(ServerEntity& entity) {
    if (entity::allays::isAllay(entity.type())) {
        static_cast<void>(entity::allays::stateOf(entity));
    }
}

void ensureGolem(ServerEntity& entity) {
    if (entity::copperGolems::isCopperGolem(entity.type())) {
        random::RandomSource random = random::RandomSource::legacy(0);
        static_cast<void>(entity::copperGolems::stateOf(entity, random));
    }
}

void addHorses(Table& table) {
    using entity::horses::HorseState;
    table["horse_eating"] =
        Accessor{[](const ServerEntity& entity) -> Value { return entity.horse != nullptr && entity.horse->eating; },
                 [](ServerEntity& entity, const Value& value) {
                     if (!entity::horses::isHorse(entity.type())) {
                         return false;
                     }
                     HorseState& state = entity::horses::stateOf(entity);
                     state.eating = asBool(value);
                     if (state.eating) {
                         state.standing = false;
                     }
                     return true;
                 }};
    table["horse_mouth_open"] =
        Accessor{[](const ServerEntity& entity) -> Value { return entity.horse != nullptr && entity.horse->mouthOpen; },
                 [](ServerEntity& entity, const Value& value) {
                     if (!entity::horses::isHorse(entity.type())) {
                         return false;
                     }
                     entity::horses::stateOf(entity).mouthOpen = asBool(value);
                     return true;
                 }};
    table["horse_rearing"] =
        Accessor{[](const ServerEntity& entity) -> Value { return entity.horse != nullptr && entity.horse->standing; },
                 [](ServerEntity& entity, const Value& value) {
                     if (!entity::horses::isHorse(entity.type())) {
                         return false;
                     }
                     if (asBool(value)) {
                         entity::horses::stand(entity, std::numeric_limits<std::int32_t>::max());
                     } else {
                         HorseState& state = entity::horses::stateOf(entity);
                         state.standing = false;
                         state.standCounter = 0;
                     }
                     return true;
                 }};
    table["horse_dashing"] =
        Accessor{[](const ServerEntity& entity) -> Value { return entity.horse != nullptr && entity.horse->dashing; },
                 [](ServerEntity& entity, const Value& value) {
                     if (!entity::horses::isCamel(entity.type())) {
                         return false;
                     }
                     entity::horses::stateOf(entity).dashing = asBool(value);
                     return true;
                 }};
    table["horse_max_temper"] = Accessor{[](const ServerEntity& entity) -> Value {
                                             return static_cast<std::int64_t>(entity::horses::maxTemper(entity));
                                         },
                                         [](ServerEntity& entity, const Value& value) {
                                             if (!entity::horses::isHorse(entity.type())) {
                                                 return false;
                                             }
                                             entity::horses::stateOf(entity).maxTemperOverride =
                                                 static_cast<std::int32_t>(asInt(value));
                                             return true;
                                         }};
}

void addBee(Table& table) {
    using entity::bees::BeeState;
    constexpr auto kBee = &ServerEntity::bee;
    table["bee_sting_age"] = viaState(kBee, &BeeState::timeSinceSting, true, ensureBee);
    table["bee_rolling"] = viaState(kBee, &BeeState::rolling, false);
    table["bee_rolling_override"] = viaState(kBee, &BeeState::rollingOverride, true, ensureBee);
}

void addAllay(Table& table, Operations& operations) {
    using entity::allays::AllayState;
    table["allay_dancing"] =
        Accessor{[](const ServerEntity& entity) -> Value { return entity.allay != nullptr && entity.allay->dancing; },
                 [](ServerEntity& entity, const Value& value) {
                     if (!entity::allays::isAllay(entity.type())) {
                         return false;
                     }
                     AllayState& state = entity::allays::stateOf(entity);
                     state.dancing = asBool(value);
                     state.forceDancing = state.dancing;
                     if (!state.dancing) {
                         state.jukebox.reset();
                     }
                     return true;
                 }};
    table["allay_jukebox"] = Accessor{[](const ServerEntity& entity) -> Value {
                                          return entity.allay == nullptr ? Value{} : position(entity.allay->jukebox);
                                      },
                                      nullptr};
    operations["allay_duplicate"] = [](const Environment& environment, ServerEntity& entity,
                                       const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return static_cast<std::int64_t>(system == nullptr ? -1 : system->pluginDuplicateAllay(entity));
    };
}

void addArmadillo(Table& table, Operations& operations) {
    table["armadillo_state"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return static_cast<std::int64_t>(entity.armadillo == nullptr ? entity::armadillos::State::Idle
                                                                                  : entity.armadillo->state);
                 },
                 nullptr};
    operations["armadillo_roll_up"] = [](const Environment& environment, ServerEntity& entity,
                                         const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginRoll(entity, true);
    };
    operations["armadillo_roll_out"] = [](const Environment& environment, ServerEntity& entity,
                                          const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginRoll(entity, false);
    };
}

void addCreaking(Table& table, Operations& operations) {
    table["creaking_active"] = Accessor{
        [](const ServerEntity& entity) -> Value { return entity.creaking != nullptr && entity.creaking->active; },
        nullptr};
    table["creaking_home"] = Accessor{[](const ServerEntity& entity) -> Value {
                                          return entity.creaking == nullptr ? Value{} : position(entity.creaking->home);
                                      },
                                      nullptr};
    operations["creaking_activate"] = [](const Environment&, ServerEntity& entity,
                                         const Arguments& arguments) -> std::optional<Value> {
        if (!entity::creakings::isCreaking(entity.type())) {
            return false;
        }
        entity::creakings::CreakingState& state = entity::creakings::stateOf(entity);
        state.active = true;
        entity.targetId = static_cast<std::int32_t>(asInt(at(arguments, 0), -1));
        if (*entity.targetId < 0) {
            entity.targetId.reset();
        }
        return true;
    };
    operations["creaking_deactivate"] = [](const Environment&, ServerEntity& entity,
                                           const Arguments&) -> std::optional<Value> {
        if (!entity::creakings::isCreaking(entity.type())) {
            return false;
        }
        entity::creakings::stateOf(entity).active = false;
        entity.targetId.reset();
        return true;
    };
}

void addCopperGolem(Table& table) {
    constexpr auto kGolem = &ServerEntity::copperGolem;
    using entity::copperGolems::GolemState;
    table["copper_weather"] = viaState(kGolem, &GolemState::weather, true, ensureGolem);
    table["copper_pose"] = viaState(kGolem, &GolemState::pose, true, ensureGolem);
    table["copper_weathering_tick"] = viaState(kGolem, &GolemState::nextWeatheringTick, true, ensureGolem);
}

void addSniffer(Table& table, Operations& operations) {
    table["sniffer_state"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return static_cast<std::int64_t>(entity.sniffer == nullptr ? entity::sniffers::Pose::Idling
                                                                                : entity.sniffer->pose);
                 },
                 nullptr};
    operations["sniffer_explored"] = [](const Environment& environment, ServerEntity& entity,
                                        const Arguments&) -> std::optional<Value> {
        std::vector<std::string> rows;
        if (entity.sniffer != nullptr && environment.world != nullptr) {
            const std::string dimension = config::dimensionRegistryKey(environment.world->declaration().dimension);
            for (const auto& [where, block] : entity.sniffer->explored) {
                if (where == dimension) {
                    rows.push_back(std::to_string(block[0]) + "," + std::to_string(block[1]) + "," +
                                   std::to_string(block[2]));
                }
            }
        }
        return rows;
    };
    operations["sniffer_explore"] = [](const Environment& environment, ServerEntity& entity,
                                       const Arguments& arguments) -> std::optional<Value> {
        if (entity.type().path() != "sniffer" || environment.world == nullptr) {
            return false;
        }
        auto& explored = entity::sniffers::stateOf(entity).explored;
        const std::string dimension = config::dimensionRegistryKey(environment.world->declaration().dimension);
        const entity::ai::brainKit::Block block{static_cast<std::int32_t>(asInt(at(arguments, 0))),
                                                static_cast<std::int32_t>(asInt(at(arguments, 1))),
                                                static_cast<std::int32_t>(asInt(at(arguments, 2)))};
        const auto found = std::ranges::find_if(
            explored, [&](const auto& entry) { return entry.first == dimension && entry.second == block; });
        if (asBool(at(arguments, 3))) {
            if (found == explored.end()) {
                return false;
            }
            explored.erase(found);
            return true;
        }
        if (found == explored.end()) {
            explored.emplace_back(dimension, block);
        }
        return true;
    };
}

void addWarden(Table& table, Operations& operations) {
    operations["warden_anger"] = [](const Environment&, ServerEntity& entity,
                                    const Arguments& arguments) -> std::optional<Value> {
        return static_cast<std::int64_t>(
            entity::wardens::angerAt(entity, static_cast<std::int32_t>(asInt(at(arguments, 0)))));
    };
    operations["warden_set_anger"] = [](const Environment&, ServerEntity& entity,
                                        const Arguments& arguments) -> std::optional<Value> {
        if (!entity::wardens::isWarden(entity.type())) {
            return false;
        }
        const auto suspect = static_cast<std::int32_t>(asInt(at(arguments, 0)));
        const auto level =
            static_cast<std::int32_t>(std::clamp<std::int64_t>(asInt(at(arguments, 1)), 0, entity::wardens::kMaxAnger));
        if (!entity.anger.has_value()) {
            entity.anger.emplace();
        }
        if (level == 0) {
            entity.anger->erase(suspect);
        } else {
            (*entity.anger)[suspect] = level;
        }
        return true;
    };
    operations["warden_increase_anger"] = [](const Environment&, ServerEntity& entity,
                                             const Arguments& arguments) -> std::optional<Value> {
        if (!entity::wardens::isWarden(entity.type())) {
            return false;
        }
        entity::wardens::anger(entity, static_cast<std::int32_t>(asInt(at(arguments, 0))),
                               static_cast<std::int32_t>(asInt(at(arguments, 1))));
        return true;
    };
    operations["warden_clear_anger"] = [](const Environment&, ServerEntity& entity,
                                          const Arguments& arguments) -> std::optional<Value> {
        return entity.anger.has_value() && entity.anger->erase(static_cast<std::int32_t>(asInt(at(arguments, 0)))) > 0;
    };
    table["warden_highest_anger"] = Accessor{[](const ServerEntity& entity) -> Value {
                                                 std::int32_t best = 0;
                                                 if (entity.anger.has_value()) {
                                                     for (const auto& [suspect, level] : *entity.anger) {
                                                         best = std::max(best, level);
                                                     }
                                                 }
                                                 return static_cast<std::int64_t>(best);
                                             },
                                             nullptr};
    table["warden_angry_at"] = Accessor{[](const ServerEntity& entity) -> Value {
                                            const std::optional<std::int32_t> target = entity::wardens::target(entity);
                                            return static_cast<std::int64_t>(target.value_or(-1));
                                        },
                                        nullptr};
    operations["warden_disturb"] = [](const Environment&, ServerEntity& entity,
                                      const Arguments& arguments) -> std::optional<Value> {
        if (!entity::wardens::isWarden(entity.type())) {
            return false;
        }
        entity::wardenBrain::setDisturbance(entity, {static_cast<std::int32_t>(asInt(at(arguments, 0))),
                                                     static_cast<std::int32_t>(asInt(at(arguments, 1))),
                                                     static_cast<std::int32_t>(asInt(at(arguments, 2)))});
        return true;
    };
}

void addAxolotl(Table& table) {
    table["axolotl_playing_dead"] =
        Accessor{[](const ServerEntity& entity) -> Value { return entity::axolotls::playingDead(entity); },
                 [](ServerEntity& entity, const Value& value) {
                     if (entity.type().path() != "axolotl") {
                         return false;
                     }
                     entity::axolotls::AxolotlState& state = entity::axolotls::stateOf(entity);
                     if (asBool(value)) {
                         state.playDeadTicks = entity::axolotls::kPlayDeadTime;
                     } else {
                         state.playDeadTicks.reset();
                     }
                     return true;
                 }};
}

void addDragon(Table& table) {
    table["dragon_phase"] = Accessor{
        [](const ServerEntity& entity) -> Value {
            return static_cast<std::int64_t>(entity.dragon == nullptr ? entity::enderDragon::Phase::Hover
                                                                      : entity.dragon->phase);
        },
        [](ServerEntity& entity, const Value& value) {
            if (entity.dragon == nullptr && entity::enderDragon::isDragon(entity.type())) {
                entity.dragon =
                    std::make_shared<entity::enderDragon::DragonState>(entity::enderDragon::freshState(entity));
            }
            if (entity.dragon == nullptr) {
                return false;
            }
            entity::enderDragon::setPhase(*entity.dragon,
                                          entity::enderDragon::phaseById(static_cast<std::int32_t>(asInt(value))));
            return true;
        }};
    table["dragon_death_ticks"] = viaState(&ServerEntity::dragon, &entity::enderDragon::DragonState::deathTime, false);
    table["dragon_podium"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return entity.dragon == nullptr ? Value{} : position(entity.dragon->fightOrigin);
                 },
                 [](ServerEntity& entity, const Value& value) {
                     const std::optional<std::string> text = asText(value);
                     if (entity.dragon == nullptr || !text.has_value()) {
                         return false;
                     }
                     std::array<std::int32_t, 3> at{};
                     std::size_t from = 0;
                     for (std::int32_t& axis : at) {
                         const std::size_t end = text->find(',', from);
                         axis = std::atoi(text->substr(from, end == std::string::npos ? end : end - from).c_str());
                         from = end == std::string::npos ? text->size() : end + 1;
                     }
                     entity.dragon->fightOrigin = at;
                     return true;
                 }};
}

} // namespace

void addBeasts(Table& table, Operations& operations) {
    addHorses(table);
    addBee(table);
    addAllay(table, operations);
    addArmadillo(table, operations);
    addCreaking(table, operations);
    addCopperGolem(table);
    addSniffer(table, operations);
    addWarden(table, operations);
    addDragon(table);
    addAxolotl(table);
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
