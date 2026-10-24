#include "MobStateTable.hpp"

#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>

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

} // namespace

void addMobCommon(Table& table, Operations& operations) {
    table["aggressive"] = plain(&ServerEntity::aggressive);
    table["despawn_in_peaceful"] = plain(&ServerEntity::despawnInPeaceful);
    table["burns_in_day"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return entity.burnsInDay >= 0 ? entity.burnsInDay != 0 : entity::sunBurn::burns(entity.type());
                 },
                 [](ServerEntity& entity, const Value& value) {
                     entity.burnsInDay = asBool(value) ? 1 : 0;
                     return true;
                 }};
    table["conversion_countdown"] = plain(&ServerEntity::conversionCountdown);
    table["conversion_clock"] = plain(&ServerEntity::conversionClock);
    table["age_locked"] = plain(&ServerEntity::ageLocked);
    table["nav_open_doors"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     const std::string_view path = entity.type().path();
                     return entity.navigationOpensDoors.value_or(path == "villager" || path == "copper_golem");
                 },
                 [](ServerEntity& entity, const Value& value) {
                     entity.navigationOpensDoors = asBool(value);
                     return true;
                 }};
    operations["ambient_sound"] = [](const Environment& environment, ServerEntity& entity,
                                     const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        const std::optional<std::string> sound = system == nullptr ? std::nullopt : system->pluginAmbientSound(entity);
        return sound.has_value() ? Value{*sound} : Value{};
    };
    operations["xp_reward"] = [](const Environment& environment, ServerEntity& entity,
                                 const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return static_cast<std::int64_t>(system == nullptr ? 0 : system->pluginExperienceReward(entity));
    };
    operations["ranged_attack"] = [](const Environment& environment, ServerEntity& entity,
                                     const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr &&
               system->pluginRangedAttack(entity, asReal(at(arguments, 0)), asReal(at(arguments, 1)),
                                          asReal(at(arguments, 2)), static_cast<float>(asReal(at(arguments, 3), 1.0)));
    };
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
