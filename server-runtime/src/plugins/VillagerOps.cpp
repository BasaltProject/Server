#include "MobStateTable.hpp"

#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

using entity::ServerEntity;
using namespace values;

[[nodiscard]] MobActionSystem* mobSystem(const Environment& environment) {
    return environment.hooks != nullptr && environment.hooks->mobSystem ? environment.hooks->mobSystem() : nullptr;
}

using Step = bool (MobActionSystem::*)(ServerEntity&);

void simple(Operations& operations, std::string_view name, Step step) {
    operations[name] = [step](const Environment& environment, ServerEntity& entity,
                              const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && (system->*step)(entity);
    };
}

} // namespace

void addVillager(Table& table, Operations& operations) {
    table["villager_restocks"] = Accessor{
        [](const ServerEntity& entity) -> Value {
            return entity.villager == nullptr ? Value{std::int64_t{0}}
                                              : Value{static_cast<std::int64_t>(entity.villager->restocksToday)};
        },
        [](ServerEntity& entity, const Value& value) {
            if (entity.type().path() != "villager") {
                return false;
            }
            entity::villager::villagerBrain::stateOf(entity).restocksToday = static_cast<std::int32_t>(asInt(value));
            return true;
        }};
    simple(operations, "villager_restock", &MobActionSystem::pluginRestock);
    simple(operations, "villager_update_demand", &MobActionSystem::pluginUpdateDemand);
    simple(operations, "villager_reset_offers", &MobActionSystem::pluginResetOffers);
    simple(operations, "villager_shake_head", &MobActionSystem::pluginShakeHead);
    simple(operations, "villager_wake", &MobActionSystem::pluginWake);
    operations["villager_zombify"] = [](const Environment& environment, ServerEntity& entity,
                                        const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return static_cast<std::int64_t>(system == nullptr ? -1 : system->pluginZombify(entity));
    };
    operations["villager_add_trades"] = [](const Environment& environment, ServerEntity& entity,
                                           const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return static_cast<std::int64_t>(
            system == nullptr ? 0
                              : system->pluginAddTrades(entity, static_cast<std::int32_t>(asInt(at(arguments, 0)))));
    };
    operations["villager_sleep"] = [](const Environment& environment, ServerEntity& entity,
                                      const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginSleep(entity, static_cast<std::int32_t>(asInt(at(arguments, 0))),
                                                        static_cast<std::int32_t>(asInt(at(arguments, 1))),
                                                        static_cast<std::int32_t>(asInt(at(arguments, 2))));
    };
    operations["villager_gossips"] = [](const Environment&, ServerEntity& entity,
                                        const Arguments&) -> std::optional<Value> {
        std::vector<std::string> rows;
        if (entity.villager != nullptr) {
            for (const auto& [target, values] : entity.villager->gossips.entries()) {
                rows.push_back(target);
                for (const std::int32_t value : values) {
                    rows.push_back(std::to_string(value));
                }
            }
        }
        return rows;
    };
    operations["villager_set_gossip"] = [](const Environment&, ServerEntity& entity,
                                           const Arguments& arguments) -> std::optional<Value> {
        const std::int64_t type = asInt(at(arguments, 1), -1);
        if (entity.type().path() != "villager" || type < 0 ||
            type >= static_cast<std::int64_t>(entity::villager::GossipType::kCount)) {
            return false;
        }
        entity::villager::villagerBrain::stateOf(entity).gossips.set(
            asText(at(arguments, 0)).value_or(""), static_cast<entity::villager::GossipType>(type),
            static_cast<std::int32_t>(asInt(at(arguments, 2))));
        return true;
    };
    operations["villager_clear_gossips"] = [](const Environment&, ServerEntity& entity,
                                              const Arguments&) -> std::optional<Value> {
        if (entity.villager == nullptr) {
            return false;
        }
        entity.villager->gossips.clear();
        return true;
    };
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
