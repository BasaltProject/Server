#include "MobGoalOps.hpp"

#include "EntityOpsValues.hpp"

#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/MobGoals.hpp>
#include <basalt/server/runtime/entity/ai/PluginGoal.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>

namespace basalt::server::runtime::plugins::entityOps::mobGoalOps {
namespace {

using entity::ServerEntity;
using namespace values;
using entity::ai::GoalFlag;

constexpr std::string_view kVanillaPrefix = "minecraft:";

[[nodiscard]] std::string keyOf(const Value& value) {
    std::string key = asText(value).value_or("");
    return key.starts_with(kVanillaPrefix) ? key.substr(kVanillaPrefix.size()) : key;
}

[[nodiscard]] std::set<GoalFlag> flagsOf(const Value& value) {
    std::set<GoalFlag> flags;
    std::string text = asText(value).value_or("");
    for (std::size_t from = 0; from <= text.size();) {
        const std::size_t end = std::min(text.find(',', from), text.size());
        const std::string_view part{text.data() + from, end - from};
        from = end + 1;
        if (part == "move") {
            flags.insert(GoalFlag::Move);
        } else if (part == "look") {
            flags.insert(GoalFlag::Look);
        } else if (part == "jump") {
            flags.insert(GoalFlag::Jump);
        } else if (part == "target") {
            flags.insert(GoalFlag::Target);
        }
    }
    return flags;
}

[[nodiscard]] std::string flagsText(const std::set<GoalFlag>& flags) {
    std::string text;
    for (const GoalFlag flag : flags) {
        constexpr std::string_view kNames[] = {"move", "look", "jump", "target"};
        text += (text.empty() ? "" : ",") + std::string{kNames[static_cast<std::size_t>(flag)]};
    }
    return text;
}

entity::ai::GoalSelector* selectorOf(ServerEntity& entity, bool create) {
    if (!entity::mobs::isMob(entity.type())) {
        return nullptr;
    }
    if (entity.goals == nullptr) {
        entity.goals = entity::ai::mobGoals::forType(entity.type());
        if (entity.goals == nullptr && create) {
            entity.goals = std::make_shared<entity::ai::GoalSelector>();
        }
    }
    return entity.goals.get();
}

} // namespace

std::optional<Value> invoke(ServerEntity& entity, std::string_view operation, const Arguments& arguments) {
    if (operation == "add_goal") {
        entity::ai::GoalSelector* selector = selectorOf(entity, true);
        const std::string key = keyOf(at(arguments, 0));
        if (selector == nullptr || key.empty()) {
            return false;
        }
        selector->add(static_cast<std::int32_t>(asInt(at(arguments, 1))),
                      std::make_unique<entity::ai::PluginGoal>(key, flagsOf(at(arguments, 2))), key, true);
        return true;
    }
    if (operation == "remove_goal") {
        entity::ai::GoalSelector* selector = selectorOf(entity, false);
        return static_cast<std::int64_t>(selector == nullptr ? 0 : selector->remove(keyOf(at(arguments, 0))));
    }
    if (operation == "remove_goals") {
        entity::ai::GoalSelector* selector = selectorOf(entity, false);
        return static_cast<std::int64_t>(selector == nullptr ? 0 : selector->removeAll(flagsOf(at(arguments, 0))));
    }
    if (operation == "has_goal") {
        entity::ai::GoalSelector* selector = selectorOf(entity, false);
        return selector != nullptr && selector->has(keyOf(at(arguments, 0)));
    }
    if (operation == "goals") {
        std::vector<std::string> rows;
        if (const entity::ai::GoalSelector* selector = selectorOf(entity, false); selector != nullptr) {
            for (const entity::ai::GoalSelector::Row& row : selector->rows()) {
                rows.push_back(row.name);
                rows.push_back(std::to_string(row.priority));
                rows.push_back(row.running ? "1" : "0");
                rows.push_back(row.plugin ? "1" : "0");
                rows.push_back(row.usable ? "1" : "0");
                rows.push_back(flagsText(row.flags));
            }
        }
        return rows;
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::entityOps::mobGoalOps
