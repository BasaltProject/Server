#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealTestEnvironments.hpp>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

constexpr int kMaxDepth = 8;

[[nodiscard]] std::string withoutNamespace(std::string text) {
    constexpr std::string_view kVanilla = "minecraft:";
    return text.starts_with(kVanilla) ? text.substr(kVanilla.size()) : text;
}

[[nodiscard]] const std::string* stringAt(const JsonObject& object, std::string_view key) {
    const JsonValue* value = object.find(key);
    return value == nullptr ? nullptr : value->get<std::string>();
}

/// parity: a `Timeline` track sampled at `ticks` of its period, constant between keyframes unless it eases linearly.
[[nodiscard]] std::optional<double> sampleTrack(const JsonObject& track, std::int64_t clockTicks, std::int64_t period) {
    const JsonValue* list = track.find("keyframes");
    const auto* frames = list == nullptr ? nullptr : list->get<std::vector<JsonValue>>();
    if (frames == nullptr || frames->empty()) {
        return std::nullopt;
    }

    struct Frame {
        std::int64_t ticks;
        double value;
        bool numeric;
    };

    std::vector<Frame> parsed;
    for (const JsonValue& frame : *frames) {
        const JsonObject* object = frame.get<JsonObject>();
        const JsonValue* value = object == nullptr ? nullptr : object->find("value");
        const JsonValue* ticks = object == nullptr ? nullptr : object->find("ticks");
        if (value == nullptr || ticks == nullptr) {
            continue;
        }
        const std::int64_t at = ticks->get<std::int64_t>() != nullptr ? *ticks->get<std::int64_t>()
                                : ticks->get<double>() != nullptr     ? static_cast<std::int64_t>(*ticks->get<double>())
                                                                      : 0;
        if (const bool* flag = value->get<bool>(); flag != nullptr) {
            parsed.push_back({at, *flag ? 1.0 : 0.0, false});
        } else if (const double* real = value->get<double>(); real != nullptr) {
            parsed.push_back({at, *real, true});
        } else if (const std::int64_t* whole = value->get<std::int64_t>(); whole != nullptr) {
            parsed.push_back({at, static_cast<double>(*whole), true});
        }
    }
    if (parsed.empty()) {
        return std::nullopt;
    }
    std::ranges::sort(parsed, {}, &Frame::ticks);
    const std::int64_t now = period > 0 ? clockTicks % period : clockTicks;
    const Frame* before = &parsed.back();
    const Frame* after = &parsed.front();
    for (const Frame& frame : parsed) {
        if (frame.ticks <= now) {
            before = &frame;
        }
    }
    for (auto frame = parsed.rbegin(); frame != parsed.rend(); ++frame) {
        if (frame->ticks > now) {
            after = &*frame;
        }
    }
    const std::string* ease = stringAt(track, "ease");
    if (!before->numeric || (ease != nullptr && withoutNamespace(*ease) == "constant") || before == after) {
        return before->value;
    }
    const std::int64_t span = (after->ticks - before->ticks + (period > 0 ? period : 0)) % (period > 0 ? period : 1);
    if (period <= 0 || span == 0) {
        return before->value;
    }
    const double progress = static_cast<double>((now - before->ticks + period) % period) / static_cast<double>(span);
    return before->value + (after->value - before->value) * progress;
}

/// parity: `TestEnvironmentDefinition.Weather.Type`.
struct WeatherType {
    std::int32_t clearTime;
    std::int32_t rainTime;
    bool raining;
    bool thundering;
};

[[nodiscard]] WeatherType weatherNamed(std::string_view name) {
    if (name == "rain") {
        return {0, 100000, true, false};
    }
    if (name == "thunder") {
        return {0, 100000, true, true};
    }
    return {100000, 0, false, false};
}

} // namespace

RealTestEnvironments::RealTestEnvironments(const RealTestInstances::Jar& jar, wire::common::Logger logger)
    : logger_(std::move(logger)) {
    if (!jar.entriesUnder || !jar.readText) {
        return;
    }
    for (const auto& [id, entry] : datapack::dataFiles(jar.entriesUnder, "timeline")) {
        const std::optional<std::string> text = jar.readText(entry);
        std::expected<JsonValue, wire::common::JsonError> parsed =
            text.has_value() ? wire::common::parseJson(*text) : std::unexpected{wire::common::JsonError{}};
        if (parsed.has_value()) {
            timelines_.insert_or_assign(id, std::move(*parsed));
        }
    }
    for (const auto& [id, entry] : datapack::dataFiles(jar.entriesUnder, "test_environment")) {
        const std::optional<std::string> text = jar.readText(entry);
        if (!text.has_value()) {
            continue;
        }
        std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(*text);
        if (parsed.has_value()) {
            named_.insert_or_assign(id, std::move(*parsed));
        }
    }
}

std::function<void()> RealTestEnvironments::activate(const JsonValue& definition, LoadedWorld& world,
                                                     const command::CommandEnvironment& environment,
                                                     const RunFunction& runFunction) const {
    return apply(definition, world, environment, runFunction, 0);
}

std::function<void()> RealTestEnvironments::apply(const JsonValue& definition, LoadedWorld& world,
                                                  const command::CommandEnvironment& environment,
                                                  const RunFunction& runFunction, const int depth) const {
    if (depth > kMaxDepth) {
        return {};
    }
    if (const std::string* name = definition.get<std::string>(); name != nullptr) {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*name);
        const auto found = id.has_value() ? named_.find(*id) : named_.end();
        if (found == named_.end()) {
            return {};
        }
        return apply(found->second, world, environment, runFunction, depth + 1);
    }
    const JsonObject* object = definition.get<JsonObject>();
    const std::string* typeName = object == nullptr ? nullptr : stringAt(*object, "type");
    if (typeName == nullptr) {
        return {};
    }
    const std::string type = withoutNamespace(*typeName);
    LoadedWorld* target = &world;
    const command::CommandEnvironment* env = &environment;
    if (type == "all_of") {
        std::vector<std::function<void()>> undone;
        const JsonValue* list = object->find("definitions");
        const auto* items = list == nullptr ? nullptr : list->get<std::vector<JsonValue>>();
        for (const JsonValue& item : items == nullptr ? std::vector<JsonValue>{} : *items) {
            if (std::function<void()> one = apply(item, world, environment, runFunction, depth + 1); one) {
                undone.push_back(std::move(one));
            }
        }
        return [undone = std::move(undone)] {
            for (auto one = undone.rbegin(); one != undone.rend(); ++one) {
                (*one)();
            }
        };
    }
    if (type == "weather") {
        const std::string* wanted = stringAt(*object, "weather");
        const WeatherType before{0, 0, world.weather().raining, world.weather().thundering};
        const WeatherType now = weatherNamed(wanted == nullptr ? "clear" : *wanted);
        world.weather().set(now.clearTime, now.rainTime, now.raining, now.thundering);
        environment.syncWeather(world);
        return [target, env, before] {
            // parity: `Weather.teardown` forgets the cycle, then puts the old kind back.
            const WeatherType back = before.thundering ? weatherNamed("thunder")
                                     : before.raining  ? weatherNamed("rain")
                                                       : weatherNamed("clear");
            target->weather().set(back.clearTime, back.rainTime, back.raining, back.thundering);
            env->syncWeather(*target);
        };
    }
    if (type == "clock_time") {
        const JsonValue* time = object->find("time");
        const std::int64_t* ticks = time == nullptr ? nullptr : time->get<std::int64_t>();
        if (ticks == nullptr) {
            return {};
        }
        const std::int64_t before = world.clock().totalTicks();
        world.clock().setTotalTicks(*ticks);
        environment.syncTime(world);
        return [target, env, before] {
            target->clock().setTotalTicks(before);
            env->syncTime(*target);
        };
    }
    if (type == "difficulty") {
        const std::string* wanted = stringAt(*object, "difficulty");
        if (wanted == nullptr) {
            return {};
        }
        const std::string before = world.difficulty();
        world.setDifficulty(*wanted);
        return [target, before] { target->setDifficulty(before); };
    }
    if (type == "game_rules") {
        const JsonValue* rules = object->find("rules");
        const JsonObject* map = rules == nullptr ? nullptr : rules->get<JsonObject>();
        if (map == nullptr) {
            return {};
        }
        auto before = std::make_shared<std::vector<std::pair<const GameRule*, std::string>>>();
        for (std::size_t at = 0; at < map->names().size(); ++at) {
            const GameRule* rule = gameRules::byId(withoutNamespace(map->names()[at]));
            const JsonValue& value = map->values()[at];
            std::string text;
            if (const bool* flag = value.get<bool>(); flag != nullptr) {
                text = *flag ? "true" : "false";
            } else if (const std::int64_t* whole = value.get<std::int64_t>(); whole != nullptr) {
                text = std::to_string(*whole);
            }
            if (rule == nullptr || text.empty()) {
                continue;
            }
            before->emplace_back(rule, world.gameRules().serialized(*rule));
            world.gameRules().set(*rule, text);
        }
        return [target, before] {
            for (const auto& [rule, text] : *before) {
                target->gameRules().set(*rule, text);
            }
        };
    }
    if (type == "function") {
        const auto named = [&object](std::string_view key) -> std::optional<Identifier> {
            const std::string* text = stringAt(*object, key);
            std::expected<Identifier, wire::common::IdentifierError> id =
                text == nullptr ? std::unexpected{wire::common::IdentifierError{}} : Identifier::parse(*text);
            return id.has_value() ? std::optional{*id} : std::nullopt;
        };
        const std::optional<Identifier> setup = named("setup");
        const std::optional<Identifier> teardown = named("teardown");
        if (setup.has_value() && runFunction) {
            runFunction(*setup);
        }
        if (!teardown.has_value() || !runFunction) {
            return {};
        }
        return [teardown, runFunction] { runFunction(*teardown); };
    }
    if (type == "timeline_attributes") {
        const JsonValue* list = object->find("timelines");
        const auto* items = list == nullptr ? nullptr : list->get<std::vector<JsonValue>>();
        auto before = std::make_shared<std::vector<std::pair<std::string, std::optional<double>>>>();
        for (const JsonValue& item : items == nullptr ? std::vector<JsonValue>{} : *items) {
            const JsonValue* held = &item;
            if (const std::string* name = item.get<std::string>(); name != nullptr) {
                const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*name);
                const auto found = id.has_value() ? timelines_.find(*id) : timelines_.end();
                held = found == timelines_.end() ? nullptr : &found->second;
            }
            const JsonObject* timeline = held == nullptr ? nullptr : held->get<JsonObject>();
            const JsonValue* tracks = timeline == nullptr ? nullptr : timeline->find("tracks");
            const JsonObject* map = tracks == nullptr ? nullptr : tracks->get<JsonObject>();
            if (map == nullptr) {
                continue;
            }
            std::int64_t period = 0;
            if (const JsonValue* length = timeline->find("period_ticks"); length != nullptr) {
                period = length->get<std::int64_t>() != nullptr ? *length->get<std::int64_t>() : 0;
            }
            for (std::size_t at = 0; at < map->names().size(); ++at) {
                const JsonObject* track = map->values()[at].get<JsonObject>();
                const std::optional<double> value =
                    track == nullptr ? std::nullopt : sampleTrack(*track, world.clock().totalTicks(), period);
                if (value.has_value()) {
                    const std::string& attribute = map->names()[at];
                    before->emplace_back(attribute, world.attributeOverride(attribute));
                    world.setAttributeOverride(attribute, value);
                }
            }
        }
        return [target, before] {
            for (auto one = before->rbegin(); one != before->rend(); ++one) {
                target->setAttributeOverride(one->first, one->second);
            }
        };
    }
    logger_.debug([&type] { return "A test environment of type " + type + " is not applied."; });
    return {};
}

} // namespace basalt::server::runtime::world
