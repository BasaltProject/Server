#pragma once

#include "WasmValues.hpp"

#include <basalt/server/plugins/PluginSurfaces.hpp>

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::plugins::wasm::hud {
using Value = wire::wasm::Value;

struct Board {
    std::int32_t id = 0;
    PluginHandle registration;
};

struct Bar {
    std::string id;
    PluginHandle registration;
};

struct HologramEntry {
    std::shared_ptr<PluginHologramRef> ref;
    PluginHandle registration;
};

struct NpcEntry {
    std::shared_ptr<PluginNpcRef> ref;
    PluginHandle registration;
};

/// What the render callback of a claim reads from other threads.
struct ClaimContent {
    std::mutex lock;
    std::optional<PluginSurfaceContent> fixed;
};

struct Claim {
    std::shared_ptr<ClaimContent> content;
    PluginHandle registration;
};

struct HudState {
    HandleTable<Board> boards;
    std::int32_t mainBoard = 0;
    HandleTable<Bar> bars;
    HandleTable<HologramEntry> holograms;
    HandleTable<NpcEntry> npcs;
    HandleTable<Claim> claims;
    bool answering = false;
    std::optional<PluginSurfaceContent> answer;
};

inline constexpr std::string_view kAreaName = "hud";

[[nodiscard]] inline HudState& stateOfHud(State& state) { return state.areaState<HudState>(kAreaName); }

/// The spec at `ptr`: a zero length is an empty map, anything that is not a map is INVALID.
[[nodiscard]] inline std::expected<Value, std::int32_t> readSpec(const abi::GuestMemory& memory, std::int32_t ptr,
                                                                 std::int32_t len) {
    if (len == 0) {
        return Value{Value::Map{}};
    }
    std::expected<Value, std::int32_t> spec = readValue(memory, ptr, len);
    if (spec && spec->asMap() == nullptr) {
        return std::unexpected(abi::kErrInvalid);
    }
    return spec;
}

/// Each take function leaves `out` alone when the key is absent and returns INVALID for a value of the wrong type.
[[nodiscard]] inline std::int32_t takeString(const Value& spec, std::string_view key, std::optional<std::string>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return abi::kOk;
    }
    const std::string* text = found->asString();
    if (text == nullptr) {
        return abi::kErrInvalid;
    }
    out = *text;
    return abi::kOk;
}

[[nodiscard]] inline std::int32_t takeBool(const Value& spec, std::string_view key, std::optional<bool>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return abi::kOk;
    }
    const bool* flag = found->asBool();
    if (flag == nullptr) {
        return abi::kErrInvalid;
    }
    out = *flag;
    return abi::kOk;
}

[[nodiscard]] inline std::int32_t takeInt(const Value& spec, std::string_view key, std::optional<std::int64_t>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return abi::kOk;
    }
    const std::int64_t* whole = found->asInt();
    if (whole == nullptr) {
        return abi::kErrInvalid;
    }
    out = *whole;
    return abi::kOk;
}

/// An integer or a float.
[[nodiscard]] inline std::int32_t takeNumber(const Value& spec, std::string_view key, std::optional<double>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return abi::kOk;
    }
    if (const double* real = found->asFloat()) {
        out = *real;
    } else if (const std::int64_t* whole = found->asInt()) {
        out = static_cast<double>(*whole);
    } else {
        return abi::kErrInvalid;
    }
    return abi::kOk;
}

/// Renders MiniMessage or an '@key' into `out`; ABSENT when the lang key is missing.
[[nodiscard]] inline std::int32_t renderPlain(const PluginText& text, std::string_view source, std::string& out) {
    out.clear();
    return renderText(text, source, {}, out) ? abi::kOk : abi::kErrAbsent;
}

/// Like takeString, rendered; the server reads plugin text for boards and bars with a "mini:" prefix.
[[nodiscard]] inline std::int32_t takeText(State& state, const Value& spec, std::string_view key,
                                           std::optional<std::string>& out, std::string_view prefix = {}) {
    std::optional<std::string> raw;
    if (const std::int32_t status = takeString(spec, key, raw); status != abi::kOk || !raw) {
        return status;
    }
    std::string rendered{prefix};
    std::string body;
    if (renderPlain(state.plugin->text, *raw, body) != abi::kOk) {
        return abi::kErrAbsent;
    }
    rendered += body;
    out = std::move(rendered);
    return abi::kOk;
}

[[nodiscard]] inline Value stringList(const std::vector<std::string>& names) {
    Value::List list;
    list.reserve(names.size());
    for (const std::string& name : names) {
        list.emplace_back(name);
    }
    return Value{std::move(list)};
}

/// A JSON string literal, the text component form of a plain name.
[[nodiscard]] inline std::string jsonLiteral(std::string_view name) {
    std::string out = "\"";
    for (const char c : name) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out + '"';
}

/// Resolves each audience entry to a uuid: the uuid of an online player of that name, else the text as given.
[[nodiscard]] inline std::optional<std::vector<std::string>> readAudience(State& state, const Value& list) {
    const Value::List* items = list.asList();
    if (items == nullptr) {
        return std::nullopt;
    }
    std::vector<std::string> uuids;
    for (const Value& item : *items) {
        const std::string* text = item.asString();
        if (text == nullptr) {
            return std::nullopt;
        }
        const std::shared_ptr<PluginPlayerRef> player = state.host->services().players().byName(*text);
        uuids.push_back(player != nullptr ? player->uuid() : *text);
    }
    return uuids;
}
} // namespace basalt::server::plugins::wasm::hud
