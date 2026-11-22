#include "../PluginFilePath.hpp"
#include "WasmServerSupport.hpp"

#include <basalt/server/plugins/PluginFiles.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <charconv>

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;

constexpr std::string_view kServerScope = "server";

[[nodiscard]] std::optional<std::string> encodeScalar(const Value& value) {
    if (const bool* flag = value.asBool()) {
        return *flag ? "b1" : "b0";
    }
    if (const std::int64_t* whole = value.asInt()) {
        return "n" + std::to_string(*whole);
    }
    if (const double* real = value.asFloat()) {
        std::array<char, 40> digits{};
        const auto [end, error] = std::to_chars(digits.data(), digits.data() + digits.size(), *real);
        return error == std::errc{} ? std::optional{"n" + std::string{digits.data(), end}} : std::nullopt;
    }
    if (const std::string* text = value.asString()) {
        return "s" + *text;
    }
    return std::nullopt;
}

[[nodiscard]] Value decodeScalar(std::string_view stored) {
    if (stored.empty()) {
        return Value{};
    }
    const std::string_view body = stored.substr(1);
    if (stored[0] == 's') {
        return Value{std::string{body}};
    }
    if (stored[0] == 'b') {
        return Value{!body.empty() && body[0] == '1'};
    }
    std::int64_t whole = 0;
    const auto [end, error] = std::from_chars(body.data(), body.data() + body.size(), whole);
    if (error == std::errc{} && end == body.data() + body.size()) {
        return Value{whole};
    }
    return Value{std::strtod(std::string{body}.c_str(), nullptr)};
}

void storeGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> key = c.memory().string(c.i32(0), c.i32(1));
    if (!state.capable("storage") || !key || key->empty()) {
        c.ret(!state.capable("storage") ? abi::kErrDenied : abi::kErrInvalid);
        return;
    }
    const std::optional<std::string> stored =
        state.host->services().storage().playerValue(state.plugin->manifest.id, kServerScope, *key);
    c.ret(stored ? returnValue(c, 2, decodeScalar(*stored)) : abi::kErrAbsent);
}

void storeSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> key = memory.string(c.i32(0), c.i32(1));
    if (!state.capable("storage")) {
        c.ret(abi::kErrDenied);
        return;
    }
    const std::expected<Value, std::int32_t> value = readValue(memory, c.i32(2), c.i32(3));
    if (!key || key->empty() || !value) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::optional<std::string> encoded;
    if (!value->isNull()) {
        encoded = encodeScalar(*value);
        if (!encoded) {
            c.ret(abi::kErrInvalid);
            return;
        }
    }
    state.host->services().storage().setPlayerValue(state.plugin->manifest.id, kServerScope, *key, encoded);
    c.ret(abi::kOk);
}

[[nodiscard]] std::optional<std::string> jsonPath(const State& state, std::string_view name) {
    if (name.empty() || name.starts_with('/')) {
        return std::nullopt;
    }
    const bool isPath = name.find('/') != std::string_view::npos || name.ends_with(".json");
    const std::string relative = isPath ? std::string{name} : "data/" + std::string{name} + ".json";
    return pluginApi::pluginFilePath(state.plugin->folder, relative);
}

void dataRead(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!state.capable("storage")) {
        c.ret(abi::kErrDenied);
        return;
    }
    const std::optional<std::string> path = name ? jsonPath(state, *name) : std::nullopt;
    if (!path) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::optional<std::string> text = wire::io::fileIO::readText(*path);
    if (!text) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const auto parsed = wire::common::parseJson(*text);
    c.ret(parsed ? returnValue(c, 2, fromJson(*parsed)) : abi::kErrInvalid);
}

void dataWrite(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    if (!state.capable("storage")) {
        c.ret(abi::kErrDenied);
        return;
    }
    const std::optional<std::string> path = name ? jsonPath(state, *name) : std::nullopt;
    const std::expected<Value, std::int32_t> value = readValue(memory, c.i32(2), c.i32(3));
    const std::optional<wire::common::JsonValue> json = value ? toJson(*value) : std::nullopt;
    if (!path || !json) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::size_t slash = path->rfind('/');
    if (slash != std::string::npos) {
        static_cast<void>(fileService::makeDirectories(path->substr(0, slash)));
    }
    const std::string text = wire::common::writeJson(*json);
    c.ret(fileService::write(*path, std::vector<std::uint8_t>{text.begin(), text.end()}) ? abi::kOk : abi::kErrInvalid);
}

void configValueImport(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> key = c.memory().string(c.i32(0), c.i32(1));
    if (!key) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (key->empty()) {
        c.ret(returnValue(c, 2, fromToml(wire::common::toml::TomlValue::ofTable(state.plugin->config))));
        return;
    }
    const wire::common::toml::TomlValue* found = configFind(state.plugin->config, *key);
    c.ret(found != nullptr ? returnValue(c, 2, fromToml(*found)) : abi::kErrAbsent);
}
} // namespace

void addServerStoreImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"store_get", storeGet, user},
                              {"store_set", storeSet, user},
                              {"data_read", dataRead, user},
                              {"data_write", dataWrite, user},
                              {"config_value", configValueImport, user},
                          });
}
} // namespace basalt::server::plugins::wasm
