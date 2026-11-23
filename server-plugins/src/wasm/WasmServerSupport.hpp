#pragma once

#include "WasmValues.hpp"

#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins::wasm::server {
using wire::wasm::MapEntry;
using wire::wasm::Value;

inline constexpr std::string_view kArea = "server";

/// What one plugin keeps for this area between calls.
struct CommandEntry {
    std::string name;
    std::string origin;
    PluginCommandSpec spec;
    std::int32_t token = 0;
    std::int32_t completeToken = -1;
    PluginHandle registration;
};

struct Area {
    HandleTable<PluginHandle> providers;
    HandleTable<PluginHandle> services;
    std::vector<CommandEntry> commands;
};

[[nodiscard]] inline Area& area(State& state) { return state.areaState<Area>(kArea); }

/// True on a thread that is inside one of this area's imports; a provider is never asked from there.
[[nodiscard]] inline bool& insideImport() {
    thread_local bool inside = false;
    return inside;
}

struct ImportScope {
    ImportScope() : previous(insideImport()) { insideImport() = true; }

    ~ImportScope() { insideImport() = previous; }

    ImportScope(const ImportScope&) = delete;
    ImportScope& operator=(const ImportScope&) = delete;
    bool previous;
};

[[nodiscard]] inline Value mapOf(std::vector<MapEntry> entries) { return Value{Value::Map{std::move(entries)}}; }

[[nodiscard]] inline Value listOfText(const std::vector<std::string>& items) {
    Value::List list;
    list.reserve(items.size());
    for (const std::string& item : items) {
        list.emplace_back(item);
    }
    return Value{std::move(list)};
}

[[nodiscard]] inline std::string_view textOf(const Value* value) {
    const std::string* text = value != nullptr ? value->asString() : nullptr;
    return text != nullptr ? std::string_view{*text} : std::string_view{};
}

[[nodiscard]] inline std::optional<std::int64_t> integerOf(const Value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* whole = value->asInt()) {
        return *whole;
    }
    if (const double* real = value->asFloat()) {
        return static_cast<std::int64_t>(*real);
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<double> numberOf(const Value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const double* real = value->asFloat()) {
        return *real;
    }
    if (const std::int64_t* whole = value->asInt()) {
        return static_cast<double>(*whole);
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<bool> flagOf(const Value* value) {
    const bool* flag = value != nullptr ? value->asBool() : nullptr;
    return flag != nullptr ? std::optional<bool>{*flag} : std::nullopt;
}

/// A value read from guest memory, with the empty buffer meaning null.
[[nodiscard]] inline std::expected<Value, std::int32_t> readOptionalValue(const abi::GuestMemory& memory,
                                                                          std::int32_t ptr, std::int32_t len) {
    if (len == 0) {
        return Value{};
    }
    return readValue(memory, ptr, len);
}

[[nodiscard]] Value fromToml(const wire::common::toml::TomlValue& value);

[[nodiscard]] Value fromJson(const wire::common::JsonValue& value);

/// Nothing when the value holds bytes, which JSON cannot carry.
[[nodiscard]] std::optional<wire::common::JsonValue> toJson(const Value& value);

/// The dotted key with its '-' and '_' spelling tried both ways.
[[nodiscard]] const wire::common::toml::TomlValue* configFind(const wire::common::toml::TomlTable& table,
                                                              std::string_view dotted);

/// A subject of a permission question: "console", a uuid or a player name.
struct Subject {
    bool console = false;
    bool known = false;
    std::string uuid;
    std::string name;
};

[[nodiscard]] Subject resolveSubject(PluginHost& host, std::string_view text);

/// The result a buffer-returning import gives for a value, INVALID when the value breaks a limit.
[[nodiscard]] inline std::int32_t returnValue(abi::Call& c, std::size_t outIndex, const Value& value) {
    return putValue(c.memory(), c.i32(outIndex), c.i32(outIndex + 1), value);
}

/// Keeps `inner` listed among the plugin's registrations; removing the returned handle removes both.
[[nodiscard]] inline PluginHandle keep(State& state, std::string what, PluginHandle inner) {
    return state.plugin->registrations.add(std::move(what), [moved = std::move(inner)]() mutable { moved.remove(); });
}

/// Runs `body` on the owner as soon as it is free, only while the plugin is loaded.
[[nodiscard]] std::shared_ptr<PluginHandle> runSoon(State& state, std::string what, std::function<void(State&)> body);

/// Runs `body` after `ticks`, only while the plugin is loaded.
[[nodiscard]] std::shared_ptr<PluginHandle> runAfter(State& state, std::int32_t ticks, std::string what,
                                                     std::function<void(State&)> body);

/// The datapacks.* operations of server_call; the status is left alone unless the arguments are malformed.
[[nodiscard]] Value runDatapackOperation(State& state, std::string_view op, const Value& args, std::int32_t& status);

[[nodiscard]] std::vector<std::string> datapackOperationNames();

/// Runs `body` every `period` ticks on the owner until it answers true, only while the plugin is loaded.
[[nodiscard]] std::shared_ptr<PluginHandle> runEvery(State& state, std::int32_t period, std::string what,
                                                     std::function<bool(State&)> body);
} // namespace basalt::server::plugins::wasm::server
