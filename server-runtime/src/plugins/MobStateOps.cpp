#include "MobGoalOps.hpp"
#include "MobStateTable.hpp"

#include <basalt/server/runtime/plugins/MobStateOps.hpp>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

struct Tables {
    Table properties;
    Operations operations;
};

const Tables& tables() {
    static const Tables kAll = [] {
        Tables all;
        addFishingHook(all.properties, all.operations);
        addMobCommon(all.properties, all.operations);
        addVehicles(all.properties, all.operations);
        addVillager(all.properties, all.operations);
        addBeasts(all.properties, all.operations);
        addFlagOps(all.properties, all.operations);
        addEntityState(all.properties, all.operations);
        return all;
    }();
    return kAll;
}

} // namespace

std::optional<server::plugins::PluginEntityValue> get(const Environment& /*environment*/,
                                                      const entity::ServerEntity& entity, std::string_view name) {
    const Table& properties = tables().properties;
    const auto found = properties.find(name);
    if (found == properties.end() || !found->second.read) {
        return std::nullopt;
    }
    return found->second.read(entity);
}

std::optional<bool> set(const Environment& /*environment*/, entity::ServerEntity& entity, std::string_view name,
                        const server::plugins::PluginEntityValue& value) {
    const Table& properties = tables().properties;
    const auto found = properties.find(name);
    if (found == properties.end()) {
        return std::nullopt;
    }
    return found->second.write && found->second.write(entity, value);
}

std::optional<server::plugins::PluginEntityValue>
invoke(const Environment& environment, entity::ServerEntity& entity, std::string_view operation,
       const std::vector<server::plugins::PluginEntityValue>& arguments) {
    if (std::optional<Value> goal = mobGoalOps::invoke(entity, operation, arguments); goal.has_value()) {
        return goal;
    }
    const Operations& operations = tables().operations;
    const auto found = operations.find(operation);
    return found == operations.end() ? std::nullopt : found->second(environment, entity, arguments);
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
