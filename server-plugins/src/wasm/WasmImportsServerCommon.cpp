#include "WasmServerSupport.hpp"

namespace basalt::server::plugins::wasm::server {
namespace {
using wire::common::JsonObject;
using wire::common::JsonValue;
using wire::common::toml::TomlTable;
using wire::common::toml::TomlValue;

[[nodiscard]] std::string swapped(std::string_view key) {
    std::string out{key};
    for (char& letter : out) {
        letter = letter == '-' ? '_' : letter == '_' ? '-' : letter;
    }
    return out;
}

[[nodiscard]] bool looksLikeUuid(std::string_view text) {
    return text.size() == 36 && text[8] == '-' && text[13] == '-' && text[18] == '-' && text[23] == '-';
}
} // namespace

Value fromToml(const TomlValue& value) {
    if (const std::string* text = value.get<std::string>()) {
        return Value{*text};
    }
    if (const std::int64_t* whole = value.get<std::int64_t>()) {
        return Value{*whole};
    }
    if (const double* real = value.get<double>()) {
        return Value{*real};
    }
    if (const bool* flag = value.get<bool>()) {
        return Value{*flag};
    }
    if (const auto* array = value.get<std::vector<TomlValue>>()) {
        Value::List list;
        for (const TomlValue& item : *array) {
            list.push_back(fromToml(item));
        }
        return Value{std::move(list)};
    }
    Value::Map map;
    if (const TomlTable* table = value.get<TomlTable>()) {
        for (const std::string& key : table->keys()) {
            map.push_back(MapEntry{key, fromToml(*table->find(key))});
        }
    }
    return Value{std::move(map)};
}

Value fromJson(const JsonValue& value) {
    if (const bool* flag = value.get<bool>()) {
        return Value{*flag};
    }
    if (const std::int64_t* whole = value.get<std::int64_t>()) {
        return Value{*whole};
    }
    if (const double* real = value.get<double>()) {
        return Value{*real};
    }
    if (const std::string* text = value.get<std::string>()) {
        return Value{*text};
    }
    if (const auto* array = value.get<std::vector<JsonValue>>()) {
        Value::List list;
        for (const JsonValue& item : *array) {
            list.push_back(fromJson(item));
        }
        return Value{std::move(list)};
    }
    Value::Map map;
    if (const JsonObject* object = value.get<JsonObject>()) {
        for (std::size_t index = 0; index < object->size(); ++index) {
            map.push_back(MapEntry{object->names()[index], fromJson(object->values()[index])});
        }
    }
    return value.isNull() ? Value{} : Value{std::move(map)};
}

std::optional<JsonValue> toJson(const Value& value) {
    if (value.isNull()) {
        return JsonValue{};
    }
    if (const bool* flag = value.asBool()) {
        return JsonValue::of(*flag);
    }
    if (const std::int64_t* whole = value.asInt()) {
        return JsonValue::of(*whole);
    }
    if (const double* real = value.asFloat()) {
        return JsonValue::of(*real);
    }
    if (const std::string* text = value.asString()) {
        return JsonValue::of(*text);
    }
    if (const Value::List* list = value.asList()) {
        std::vector<JsonValue> items;
        for (const Value& item : *list) {
            std::optional<JsonValue> converted = toJson(item);
            if (!converted) {
                return std::nullopt;
            }
            items.push_back(std::move(*converted));
        }
        return JsonValue::ofArray(std::move(items));
    }
    const Value::Map* map = value.asMap();
    if (map == nullptr) {
        return std::nullopt;
    }
    JsonObject object;
    for (const MapEntry& entry : *map) {
        std::optional<JsonValue> converted = toJson(entry.value);
        if (!converted) {
            return std::nullopt;
        }
        object.set(entry.key, std::move(*converted));
    }
    return JsonValue::ofObject(std::move(object));
}

const TomlValue* configFind(const TomlTable& table, std::string_view dotted) {
    if (const TomlValue* direct = configValue(table, dotted)) {
        return direct;
    }
    return configValue(table, swapped(dotted));
}

Subject resolveSubject(PluginHost& host, std::string_view text) {
    Subject subject;
    if (text == "console") {
        subject.console = true;
        subject.known = true;
        subject.name = "CONSOLE";
        return subject;
    }
    std::shared_ptr<PluginPlayerRef> player = looksLikeUuid(text) ? host.services().players().byUuid(text) : nullptr;
    if (player == nullptr) {
        player = host.services().players().byName(text);
    }
    if (player != nullptr && player->online()) {
        subject.known = true;
        subject.uuid = player->uuid();
        subject.name = player->name();
    }
    return subject;
}

namespace {
[[nodiscard]] std::shared_ptr<PluginHandle> arm(State& state, std::string what,
                                                const std::function<PluginHandle(std::function<void()>)>& start,
                                                std::function<void(State&)> body) {
    auto slot = std::make_shared<PluginHandle>();
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    PluginHandle task = start([host, link, slot, body = std::move(body)] {
        const std::lock_guard<std::recursive_mutex> turn{host->turnMutex()};
        State* live = *link;
        if (live == nullptr) {
            return;
        }
        body(*live);
        slot->remove();
    });
    *slot = keep(state, std::move(what), std::move(task));
    return slot;
}
} // namespace

std::shared_ptr<PluginHandle> runSoon(State& state, std::string what, std::function<void(State&)> body) {
    PluginHost* host = state.host;
    return arm(
        state, std::move(what),
        [host](std::function<void()> fn) { return host->services().tasks().after(1, std::move(fn)); }, std::move(body));
}

std::shared_ptr<PluginHandle> runAfter(State& state, std::int32_t ticks, std::string what,
                                       std::function<void(State&)> body) {
    PluginHost* host = state.host;
    return arm(
        state, std::move(what),
        [host, ticks](std::function<void()> fn) { return host->services().tasks().after(ticks, std::move(fn)); },
        std::move(body));
}

std::shared_ptr<PluginHandle> runEvery(State& state, std::int32_t period, std::string what,
                                       std::function<bool(State&)> body) {
    auto slot = std::make_shared<PluginHandle>();
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    PluginHandle task = host->services().tasks().every(period, period, [host, link, slot, body = std::move(body)] {
        const std::lock_guard<std::recursive_mutex> turn{host->turnMutex()};
        State* live = *link;
        if (live != nullptr && body(*live)) {
            slot->remove();
        }
    });
    *slot = keep(state, std::move(what), std::move(task));
    return slot;
}
} // namespace basalt::server::plugins::wasm::server

namespace basalt::server::plugins::wasm {
void addServerCommonImports(State&, std::vector<abi::HostImport>&) {}
} // namespace basalt::server::plugins::wasm
