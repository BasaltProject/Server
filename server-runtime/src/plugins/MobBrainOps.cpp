#include "MobBrainOps.hpp"

#include "EntityOpsValues.hpp"

#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/brain/BrainPublish.hpp>
#include <basalt/server/runtime/entity/brain/BrainState.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>

#include <charconv>
#include <memory>
#include <string>

namespace basalt::server::runtime::plugins::entityOps::mobBrainOps {
namespace {

using entity::ServerEntity;
using entity::brain::BrainState;
using entity::brain::MemoryStatus;
using entity::brain::Value;
using namespace values;

constexpr char kSeparator = '|';

[[nodiscard]] std::vector<std::string_view> split(std::string_view text) {
    std::vector<std::string_view> parts;
    for (std::size_t from = 0; from <= text.size();) {
        const std::size_t end = std::min(text.find(kSeparator, from), text.size());
        parts.push_back(text.substr(from, end - from));
        from = end + 1;
    }
    return parts;
}

[[nodiscard]] double realOf(std::string_view text) { return std::strtod(std::string{text}.c_str(), nullptr); }

[[nodiscard]] std::int64_t wholeOf(std::string_view text) {
    std::int64_t out = 0;
    std::from_chars(text.data(), text.data() + text.size(), out);
    return out;
}

[[nodiscard]] std::string numberText(double number) {
    char buffer[40];
    const auto made = std::to_chars(buffer, buffer + sizeof buffer, number);
    return {buffer, made.ptr};
}

/// Compound value as `tag|field|...`; scalars inside a list use `b`, `i`, `r` and `t`.
[[nodiscard]] Value decodeParts(const std::vector<std::string_view>& parts, std::string_view whole) {
    Value value;
    const std::string_view tag = parts.empty() ? std::string_view{} : parts[0];
    const auto field = [&](std::size_t index) { return index < parts.size() ? parts[index] : std::string_view{}; };
    if (tag == "b") {
        value.kind = Value::Kind::Bool;
        value.flag = field(1) == "1";
    } else if (tag == "i") {
        value.kind = Value::Kind::Int;
        value.whole = wholeOf(field(1));
    } else if (tag == "r") {
        value.kind = Value::Kind::Real;
        value.real = realOf(field(1));
    } else if (tag == "t") {
        value.kind = Value::Kind::Text;
        value.text = whole.size() > 2 ? std::string{whole.substr(2)} : std::string{};
    } else if (tag == "entity") {
        value.kind = Value::Kind::Entity;
        value.whole = wholeOf(field(1));
    } else if (tag == "position") {
        value.kind = Value::Kind::Position;
        value.text = std::string{field(1)};
        value.at = {realOf(field(2)), realOf(field(3)), realOf(field(4))};
    } else if (tag == "walk") {
        value.kind = Value::Kind::Walk;
        value.at = {realOf(field(1)), realOf(field(2)), realOf(field(3))};
        value.whole = wholeOf(field(4));
        value.real = realOf(field(5));
        value.closeEnough = static_cast<std::int32_t>(wholeOf(field(6)));
    }
    return value;
}

[[nodiscard]] Value decodeElement(std::string_view text) { return decodeParts(split(text), text); }

[[nodiscard]] std::string encodeElement(const Value& value) {
    using Kind = Value::Kind;
    switch (value.kind) {
    case Kind::Bool:
        return std::string{"b|"} + (value.flag ? "1" : "0");
    case Kind::Int:
        return "i|" + std::to_string(value.whole);
    case Kind::Real:
        return "r|" + numberText(value.real);
    case Kind::Text:
        return "t|" + value.text;
    case Kind::Entity:
        return "entity|" + std::to_string(value.whole);
    case Kind::Position:
        return "position|" + value.text + "|" + numberText(value.at[0]) + "|" + numberText(value.at[1]) + "|" +
               numberText(value.at[2]);
    case Kind::Walk:
        return "walk|" + numberText(value.at[0]) + "|" + numberText(value.at[1]) + "|" + numberText(value.at[2]) + "|" +
               std::to_string(value.whole) + "|" + numberText(value.real) + "|" + std::to_string(value.closeEnough);
    case Kind::Marker:
    case Kind::List:
        break;
    }
    return "marker";
}

[[nodiscard]] Value decode(const server::plugins::PluginEntityValue& wire) {
    Value value;
    if (const auto* flag = std::get_if<bool>(&wire); flag != nullptr) {
        value.kind = Value::Kind::Bool;
        value.flag = *flag;
    } else if (const auto* whole = std::get_if<std::int64_t>(&wire); whole != nullptr) {
        value.kind = Value::Kind::Int;
        value.whole = *whole;
    } else if (const auto* real = std::get_if<double>(&wire); real != nullptr) {
        value.kind = Value::Kind::Real;
        value.real = *real;
    } else if (const auto* text = std::get_if<std::string>(&wire); text != nullptr) {
        value.kind = Value::Kind::Text;
        value.text = *text;
    } else if (const auto* list = std::get_if<std::vector<std::string>>(&wire); list != nullptr && !list->empty()) {
        if ((*list)[0] == "list") {
            value.kind = Value::Kind::List;
            for (std::size_t index = 1; index < list->size(); index++) {
                value.items.push_back(decodeElement((*list)[index]));
            }
        } else if ((*list)[0] != "marker") {
            std::string joined = (*list)[0];
            for (std::size_t index = 1; index < list->size(); index++) {
                joined += kSeparator + (*list)[index];
            }
            value = decodeElement(joined);
        }
    }
    return value;
}

[[nodiscard]] server::plugins::PluginEntityValue encode(const Value& value) {
    using Kind = Value::Kind;
    switch (value.kind) {
    case Kind::Bool:
        return value.flag;
    case Kind::Int:
        return value.whole;
    case Kind::Real:
        return value.real;
    case Kind::Text:
        return value.text;
    case Kind::List: {
        std::vector<std::string> out{"list"};
        for (const Value& item : value.items) {
            out.push_back(encodeElement(item));
        }
        return out;
    }
    case Kind::Marker:
        return std::vector<std::string>{"marker"};
    default: {
        std::vector<std::string> parts;
        for (const std::string_view part : split(encodeElement(value))) {
            parts.emplace_back(part);
        }
        return parts;
    }
    }
}

[[nodiscard]] std::vector<std::string> textsOf(const Arguments& arguments, std::size_t from) {
    std::vector<std::string> out;
    for (std::size_t index = from; index < arguments.size(); index++) {
        if (const auto* list = std::get_if<std::vector<std::string>>(&arguments[index]); list != nullptr) {
            out.insert(out.end(), list->begin(), list->end());
        } else if (const std::optional<std::string> text = asText(arguments[index]); text.has_value()) {
            out.push_back(*text);
        }
    }
    return out;
}

[[nodiscard]] MemoryStatus statusOf(std::string_view text) {
    if (text == "absent") {
        return MemoryStatus::Absent;
    }
    return text == "registered" ? MemoryStatus::Registered : MemoryStatus::Present;
}

BrainState* stateOf(ServerEntity& entity, bool create) {
    if (!entity::mobs::isMob(entity.type())) {
        return nullptr;
    }
    if (entity.brainState == nullptr && create) {
        entity.brainState = std::make_shared<BrainState>();
        entity::brain::adopt(entity, *entity.brainState);
    }
    return entity.brainState.get();
}

} // namespace

std::optional<server::plugins::PluginEntityValue>
invoke(const Environment& environment, ServerEntity& entity, std::string_view operation,
       const std::vector<server::plugins::PluginEntityValue>& arguments) {
    if (!operation.starts_with("brain_")) {
        return std::nullopt;
    }
    const bool creating =
        operation == "brain_register" || operation == "brain_define_activity" || operation == "brain_set_memory" ||
        operation == "brain_add_behavior" || operation == "brain_sensors" || operation == "brain_schedule" ||
        operation == "brain_set_core" || operation == "brain_set_default" || operation == "brain_set_activity" ||
        operation == "brain_tick" || operation == "brain_first_valid";
    BrainState* state = stateOf(entity, creating);
    const auto text = [&](std::size_t index) { return asText(at(arguments, index)).value_or(""); };
    if (operation == "brain_has") {
        return state != nullptr;
    }
    if (operation == "brain_clear") {
        entity.brainState.reset();
        return true;
    }
    if (state == nullptr) {
        if (operation == "brain_memories" || operation == "brain_active" || operation == "brain_behaviors") {
            return std::vector<std::string>{};
        }
        if (operation == "brain_has_memory") {
            return false;
        }
        return std::nullopt;
    }
    if (operation == "brain_tick") {
        state->requestDrive();
        if (environment.world != nullptr) {
            state->tick(environment.world->gameTime(), environment.world->clock().totalTicks(), entity.age < 0);
        }
        return true;
    }
    if (operation == "brain_clear_memories") {
        state->clearMemories();
        return true;
    }
    if (operation == "brain_register") {
        for (const std::string& key : textsOf(arguments, 0)) {
            state->registerMemory(key);
        }
        return true;
    }
    if (operation == "brain_memory") {
        const Value* found = state->get(text(0));
        return found == nullptr ? std::optional{server::plugins::PluginEntityValue{}} : std::optional{encode(*found)};
    }
    if (operation == "brain_set_memory") {
        const std::int64_t ttl =
            arguments.size() > 2 ? asInt(arguments[2], entity::brain::kForever) : entity::brain::kForever;
        return state->set(text(0), decode(at(arguments, 1)), ttl);
    }
    if (operation == "brain_erase_memory") {
        state->erase(text(0));
        return true;
    }
    if (operation == "brain_has_memory") {
        return state->has(text(0));
    }
    if (operation == "brain_check_memory") {
        return state->check(text(0), statusOf(text(1)));
    }
    if (operation == "brain_memory_ttl") {
        const std::optional<std::int64_t> left = state->timeToLive(text(0));
        return left.has_value() ? server::plugins::PluginEntityValue{*left} : server::plugins::PluginEntityValue{};
    }
    if (operation == "brain_memories") {
        std::vector<std::string> rows;
        for (const std::string& key : state->keys()) {
            rows.push_back(key);
            rows.push_back(state->has(key) ? "1" : "0");
            rows.push_back(std::to_string(state->timeToLive(key).value_or(entity::brain::kForever)));
        }
        return rows;
    }
    if (operation == "brain_define_activity") {
        std::vector<entity::brain::Requirement> requirements;
        for (const std::string& each : textsOf({at(arguments, 1)}, 0)) {
            const std::size_t equals = each.find('=');
            requirements.push_back({each.substr(0, equals),
                                    statusOf(equals == std::string::npos ? std::string_view{}
                                                                         : std::string_view{each}.substr(equals + 1))});
        }
        state->defineActivity(text(0), std::move(requirements), textsOf({at(arguments, 2)}, 0));
        return true;
    }
    if (operation == "brain_add_behavior") {
        state->addBehavior(text(0), static_cast<std::int32_t>(asInt(at(arguments, 1))), text(2));
        return true;
    }
    if (operation == "brain_clear_behaviors") {
        state->removeBehaviors();
        return true;
    }
    if (operation == "brain_behaviors") {
        std::vector<std::string> rows;
        for (const entity::brain::BehaviorRow& row : state->behaviors()) {
            rows.push_back(row.activity);
            rows.push_back(std::to_string(row.priority));
            rows.push_back(row.key);
        }
        return rows;
    }
    if (operation == "brain_set_core") {
        const std::vector<std::string> names = textsOf(arguments, 0);
        state->setCoreActivities({names.begin(), names.end()});
        return true;
    }
    if (operation == "brain_set_default") {
        state->setDefaultActivity(text(0));
        return true;
    }
    if (operation == "brain_set_activity") {
        if (text(1) == "force") {
            state->setActive(text(0));
        } else if (text(1) == "default") {
            state->useDefaultActivity();
        } else {
            state->setActiveIfPossible(text(0));
        }
        return true;
    }
    if (operation == "brain_first_valid") {
        state->setActiveToFirstValid(textsOf(arguments, 0));
        return true;
    }
    if (operation == "brain_active") {
        return std::vector<std::string>{state->active().begin(), state->active().end()};
    }
    if (operation == "brain_active_non_core") {
        const std::optional<std::string> found = state->activeNonCore();
        return found.has_value() ? server::plugins::PluginEntityValue{*found} : server::plugins::PluginEntityValue{};
    }
    if (operation == "brain_is_active") {
        return state->isActive(text(0));
    }
    if (operation == "brain_schedule") {
        state->setSchedule(text(0));
        return true;
    }
    if (operation == "brain_sensors") {
        state->sensors().clear();
        for (const std::string& each : textsOf(arguments, 0)) {
            const std::size_t marker = each.find('@');
            entity::brain::SensorSpec spec{.name = each.substr(0, marker)};
            if (marker != std::string::npos) {
                spec.range = realOf(std::string_view{each}.substr(marker + 1));
            }
            state->sensors().push_back(std::move(spec));
        }
        return true;
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::entityOps::mobBrainOps
