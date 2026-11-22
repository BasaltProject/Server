#pragma once

#include "WasmState.hpp"
#include "WasmValues.hpp"

#include <basalt/server/plugins/PluginEntityOps.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::server::plugins::wasm {
using wire::wasm::MapEntry;
using wire::wasm::Value;

/// What the events area keeps per plugin: the post copy being delivered, and pending forms and dialogs by player.
struct EventsArea {
    const PluginEvent* post = nullptr;
    std::map<std::string, std::int32_t> forms;
    std::map<std::string, std::int32_t> dialogs;
    bool dialogListener = false;
};

[[nodiscard]] inline EventsArea& eventsArea(State& state) { return state.areaState<EventsArea>("events"); }

[[nodiscard]] inline Value valueOf(const PluginEventValue& value) {
    if (const auto* flag = std::get_if<bool>(&value)) {
        return Value{*flag};
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value)) {
        return Value{*whole};
    }
    if (const auto* real = std::get_if<double>(&value)) {
        return Value{*real};
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        return Value{*text};
    }
    return Value{};
}

[[nodiscard]] inline Value valueOf(const PluginEntityValue& value) {
    if (const auto* flag = std::get_if<bool>(&value)) {
        return Value{*flag};
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value)) {
        return Value{*whole};
    }
    if (const auto* real = std::get_if<double>(&value)) {
        return Value{*real};
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        return Value{*text};
    }
    if (const auto* list = std::get_if<std::vector<std::string>>(&value)) {
        Value::List items;
        for (const std::string& item : *list) {
            items.emplace_back(item);
        }
        return Value{std::move(items)};
    }
    return Value{};
}

[[nodiscard]] inline std::optional<std::string> textOf(const Value& value) {
    if (const std::string* text = value.asString()) {
        return *text;
    }
    if (const std::int64_t* whole = value.asInt()) {
        return std::to_string(*whole);
    }
    if (const double* real = value.asFloat()) {
        char digits[40];
        const auto [end, error] = std::to_chars(digits, digits + sizeof digits, *real);
        return error == std::errc{} ? std::string{digits, end} : std::string{};
    }
    if (const bool* flag = value.asBool()) {
        return *flag ? "true" : "false";
    }
    return std::nullopt;
}

/// The value as the player and entity layers take it; a map or bytes has no such form.
[[nodiscard]] inline std::optional<PluginEntityValue> entityValueOf(const Value& value) {
    if (value.isNull()) {
        return PluginEntityValue{};
    }
    if (const bool* flag = value.asBool()) {
        return PluginEntityValue{*flag};
    }
    if (const std::int64_t* whole = value.asInt()) {
        return PluginEntityValue{*whole};
    }
    if (const double* real = value.asFloat()) {
        return PluginEntityValue{*real};
    }
    if (const std::string* text = value.asString()) {
        return PluginEntityValue{*text};
    }
    if (const Value::List* list = value.asList()) {
        std::vector<std::string> items;
        for (const Value& item : *list) {
            const std::optional<std::string> text = textOf(item);
            if (!text) {
                return std::nullopt;
            }
            items.push_back(*text);
        }
        return PluginEntityValue{std::move(items)};
    }
    return std::nullopt;
}

/// A scalar of the value document as an event field; nothing for a container.
[[nodiscard]] inline std::optional<PluginEventValue> eventValueOf(const Value& value) {
    if (value.isNull()) {
        return PluginEventValue{};
    }
    if (const bool* flag = value.asBool()) {
        return PluginEventValue{*flag};
    }
    if (const std::int64_t* whole = value.asInt()) {
        return PluginEventValue{*whole};
    }
    if (const double* real = value.asFloat()) {
        return PluginEventValue{*real};
    }
    if (const std::string* text = value.asString()) {
        return PluginEventValue{*text};
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<std::string_view> mapText(const Value& map, std::string_view key) {
    const Value* found = map.find(key);
    const std::string* text = found == nullptr ? nullptr : found->asString();
    return text == nullptr ? std::nullopt : std::optional<std::string_view>{*text};
}
} // namespace basalt::server::plugins::wasm
