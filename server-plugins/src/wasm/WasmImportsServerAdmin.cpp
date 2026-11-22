#include "WasmServerSupport.hpp"

#include <algorithm>

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;

struct AdminCall {
    State& state;
    const Value& args;
    std::int32_t status = abi::kOk;

    [[nodiscard]] PluginServices& services() const { return state.host->services(); }

    [[nodiscard]] const Value* field(std::string_view key) const { return args.find(key); }

    [[nodiscard]] std::string_view text(std::string_view key) const { return textOf(field(key)); }

    Value invalid() {
        status = abi::kErrInvalid;
        return Value{};
    }
};

using Operation = Value (*)(AdminCall&);

[[nodiscard]] Value rowOf(const PluginKnownPlayer& player) {
    return mapOf({{"uuid", Value{player.uuid}},
                  {"name", Value{player.name}},
                  {"first_seen", Value{player.firstSeenMillis}},
                  {"last_seen", Value{player.lastSeenMillis}}});
}

[[nodiscard]] std::optional<std::string> uuidOf(AdminCall& call, std::string_view who) {
    if (who.empty()) {
        return std::nullopt;
    }
    const Subject subject = resolveSubject(*call.state.host, who);
    if (subject.known && !subject.console) {
        return subject.uuid;
    }
    PluginAccess& access = call.services().access();
    if (const std::optional<PluginKnownPlayer> byUuid = access.playerByUuid(who)) {
        return byUuid->uuid;
    }
    if (const std::optional<PluginKnownPlayer> byName = access.playerByName(who)) {
        return byName->uuid;
    }
    return std::nullopt;
}

[[nodiscard]] Value optionalText(const std::optional<std::string>& text) { return text ? Value{*text} : Value{}; }

Value settingsKeys(AdminCall& call) { return listOfText(call.services().serverControl().settingKeys()); }

Value settingsGet(AdminCall& call) {
    return call.field("key") == nullptr ? call.invalid()
                                        : optionalText(call.services().serverControl().setting(call.text("key")));
}

Value settingsSet(AdminCall& call) {
    const Value* value = call.field("value");
    if (call.field("key") == nullptr || value == nullptr) {
        return call.invalid();
    }
    std::string text;
    if (const std::string* word = value->asString()) {
        text = *word;
    } else if (const std::int64_t* whole = value->asInt()) {
        text = std::to_string(*whole);
    } else if (const double* real = value->asFloat()) {
        text = std::to_string(*real);
    } else if (const bool* flag = value->asBool()) {
        text = *flag ? "true" : "false";
    } else {
        return call.invalid();
    }
    return Value{call.services().serverControl().setSetting(call.text("key"), text)};
}

Value tickRate(AdminCall& call) { return Value{static_cast<double>(call.services().serverControl().tickRate())}; }

Value tickSetRate(AdminCall& call) {
    const std::optional<double> rate = numberOf(call.field("rate"));
    if (!rate || *rate <= 0.0) {
        return call.invalid();
    }
    call.services().serverControl().setTickRate(static_cast<float>(*rate));
    return Value{};
}

Value tickFrozen(AdminCall& call) { return Value{call.services().serverControl().frozen()}; }

Value tickSetFrozen(AdminCall& call) {
    const std::optional<bool> frozen = flagOf(call.field("frozen"));
    if (!frozen) {
        return call.invalid();
    }
    call.services().serverControl().setFrozen(*frozen);
    return Value{};
}

Value tickStep(AdminCall& call) {
    const std::optional<std::int64_t> count = integerOf(call.field("count"));
    return count && *count > 0 ? Value{call.services().serverControl().step(static_cast<std::int32_t>(*count))}
                               : call.invalid();
}

Value tickSprint(AdminCall& call) {
    const std::optional<std::int64_t> count = integerOf(call.field("count"));
    return count && *count > 0 ? Value{call.services().serverControl().sprint(static_cast<std::int32_t>(*count))}
                               : call.invalid();
}

Value tickStopSprint(AdminCall& call) { return Value{call.services().serverControl().stopSprinting()}; }

Value tickPace(AdminCall& call) {
    const std::array<double, 4> pace = call.services().serverControl().pace();
    return mapOf({{"tps_1m", Value{pace[0]}},
                  {"tps_5m", Value{pace[1]}},
                  {"tps_15m", Value{pace[2]}},
                  {"mean_tick_ms", Value{pace[3]}}});
}

Value tickTimes(AdminCall& call) {
    Value::List list;
    for (const std::int64_t nanos : call.services().serverControl().tickTimes()) {
        list.emplace_back(nanos);
    }
    return Value{std::move(list)};
}

Value tickPaused(AdminCall& call) { return Value{call.services().serverControl().paused()}; }

Value linksList(AdminCall& call) {
    Value::List list;
    for (const PluginServerLink& link : call.services().serverControl().links()) {
        list.push_back(mapOf({{"kind", Value{link.kind}}, {"label", Value{link.label}}, {"url", Value{link.url}}}));
    }
    return Value{std::move(list)};
}

Value linksSet(AdminCall& call) {
    const Value* given = call.field("links");
    const Value::List* list = given != nullptr ? given->asList() : nullptr;
    if (list == nullptr) {
        return call.invalid();
    }
    std::vector<PluginServerLink> links;
    for (const Value& row : *list) {
        if (row.asMap() == nullptr || textOf(row.find("url")).empty()) {
            return call.invalid();
        }
        links.push_back(PluginServerLink{.kind = std::string{textOf(row.find("kind"))},
                                         .label = std::string{textOf(row.find("label"))},
                                         .url = std::string{textOf(row.find("url"))}});
    }
    call.services().serverControl().setLinks(std::move(links));
    return Value{};
}

Value accessBanned(AdminCall& call) {
    const std::optional<std::string> uuid = uuidOf(call, call.text("who"));
    return Value{uuid && call.services().access().banOf(*uuid, false).has_value()};
}

Value accessWhitelisted(AdminCall& call) {
    const std::optional<std::string> uuid = uuidOf(call, call.text("who"));
    return Value{uuid && call.services().access().isWhitelisted(*uuid)};
}

Value accessOperator(AdminCall& call) {
    const std::optional<std::string> uuid = uuidOf(call, call.text("who"));
    return Value{uuid && call.services().access().isOperator(*uuid)};
}

Value accessKnown(AdminCall& call) {
    const std::string_view who = call.text("who");
    if (who.empty()) {
        return call.invalid();
    }
    PluginAccess& access = call.services().access();
    std::optional<PluginKnownPlayer> found = access.playerByUuid(who);
    if (!found) {
        found = access.playerByName(who);
    }
    return found ? rowOf(*found) : Value{};
}

Value accessStatistic(AdminCall& call) {
    return call.text("uuid").empty() || call.text("key").empty()
               ? call.invalid()
               : Value{call.services().access().statistic(call.text("uuid"), call.text("key"))};
}

Value accessSetStatistic(AdminCall& call) {
    const std::optional<std::int64_t> value = integerOf(call.field("value"));
    if (call.text("uuid").empty() || call.text("key").empty() || !value) {
        return call.invalid();
    }
    return Value{call.services().access().setStatistic(call.text("uuid"), call.text("key"), *value)};
}

Value accessLastDeath(AdminCall& call) {
    const std::optional<std::array<std::string, 4>> death = call.services().access().lastDeath(call.text("uuid"));
    if (!death) {
        return Value{};
    }
    return mapOf({{"dimension", Value{(*death)[0]}},
                  {"x", Value{static_cast<std::int64_t>(std::atoll((*death)[1].c_str()))}},
                  {"y", Value{static_cast<std::int64_t>(std::atoll((*death)[2].c_str()))}},
                  {"z", Value{static_cast<std::int64_t>(std::atoll((*death)[3].c_str()))}}});
}

Value stopwatchesList(AdminCall& call) { return listOfText(call.services().serverControl().stopwatchIds()); }

Value stopwatchFlag(AdminCall& call, std::string_view operation) {
    if (call.text("id").empty()) {
        return call.invalid();
    }
    return Value{call.services().serverControl().stopwatch(operation, call.text("id")).has_value()};
}

Value stopwatchesCreate(AdminCall& call) { return stopwatchFlag(call, "create"); }

Value stopwatchesRestart(AdminCall& call) { return stopwatchFlag(call, "restart"); }

Value stopwatchesRemove(AdminCall& call) { return stopwatchFlag(call, "remove"); }

Value stopwatchesElapsed(AdminCall& call) {
    if (call.text("id").empty()) {
        return call.invalid();
    }
    const std::optional<double> seconds = call.services().serverControl().stopwatch("elapsed", call.text("id"));
    return seconds ? Value{*seconds} : Value{};
}

struct Entry {
    std::string_view name;
    Operation run;
    bool datapacks;
};

constexpr std::array kOperations{
    Entry{"settings.keys", settingsKeys, false},
    Entry{"settings.get", settingsGet, false},
    Entry{"settings.set", settingsSet, false},
    Entry{"tick.rate", tickRate, false},
    Entry{"tick.set_rate", tickSetRate, false},
    Entry{"tick.frozen", tickFrozen, false},
    Entry{"tick.set_frozen", tickSetFrozen, false},
    Entry{"tick.step", tickStep, false},
    Entry{"tick.sprint", tickSprint, false},
    Entry{"tick.stop_sprint", tickStopSprint, false},
    Entry{"tick.pace", tickPace, false},
    Entry{"tick.times", tickTimes, false},
    Entry{"tick.paused", tickPaused, false},
    Entry{"links.list", linksList, false},
    Entry{"links.set", linksSet, false},
    Entry{"access.is_banned", accessBanned, false},
    Entry{"access.is_whitelisted", accessWhitelisted, false},
    Entry{"access.is_operator", accessOperator, false},
    Entry{"access.known", accessKnown, false},
    Entry{"access.statistic", accessStatistic, false},
    Entry{"access.set_statistic", accessSetStatistic, false},
    Entry{"access.last_death", accessLastDeath, false},
    Entry{"stopwatches.list", stopwatchesList, false},
    Entry{"stopwatches.create", stopwatchesCreate, false},
    Entry{"stopwatches.restart", stopwatchesRestart, false},
    Entry{"stopwatches.remove", stopwatchesRemove, false},
    Entry{"stopwatches.elapsed", stopwatchesElapsed, false},
};
} // namespace

namespace {
void serverCall(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> op = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> args = readOptionalValue(memory, c.i32(2), c.i32(3));
    if (!op || !args) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const ImportScope scope;
    if (*op == "ops") {
        std::vector<std::string> names;
        for (const Entry& entry : kOperations) {
            names.emplace_back(entry.name);
        }
        for (std::string& name : datapackOperationNames()) {
            names.push_back(std::move(name));
        }
        std::ranges::sort(names);
        names.emplace_back("ops");
        c.ret(returnValue(c, 4, listOfText(names)));
        return;
    }
    if (op->starts_with("datapacks.")) {
        if (!state.capable("datapacks")) {
            c.ret(abi::kErrDenied);
            return;
        }
        std::int32_t status = abi::kOk;
        const Value result = runDatapackOperation(state, *op, *args, status);
        c.ret(status != abi::kOk ? status : returnValue(c, 4, result));
        return;
    }
    for (const Entry& entry : kOperations) {
        if (entry.name == *op) {
            AdminCall call{state, *args};
            const Value result = entry.run(call);
            c.ret(call.status != abi::kOk ? call.status : returnValue(c, 4, result));
            return;
        }
    }
    c.ret(abi::kErrInvalid);
}
} // namespace

void addServerAdminImports(State& state, std::vector<abi::HostImport>& out) {
    out.insert(out.end(), {{"server_call", serverCall, &state}});
}
} // namespace basalt::server::plugins::wasm
