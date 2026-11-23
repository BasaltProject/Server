#pragma once

#include "WasmValues.hpp"

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/plugins/PluginWorldEnv.hpp>
#include <basalt/server/plugins/PluginWorldgen.hpp>
#include <basalt/wire/common/ByteView.hpp>

#include <cstring>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::plugins::wasm::worlds {
using wire::wasm::MapEntry;
using wire::wasm::Value;

/// Bulk buffers are capped so one call cannot ask the host for gigabytes.
inline constexpr std::int32_t kMaxBulk = 1 << 24;
inline constexpr std::int32_t kSectionBlocks = 4096;
inline constexpr std::int32_t kLightBytes = 2048;

struct Registry {
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    ~Registry();

    HandleTable<std::shared_ptr<PluginWorldRef>> handles;
    std::map<std::string, std::int32_t, std::less<>> byName;
    HandleTable<std::int64_t> noises;
    PluginWorldgen* worldgen = nullptr;
};

[[nodiscard]] inline Registry& registry(State& state) { return state.areaState<Registry>("worlds"); }

/// The handle for a live world, reusing the one this plugin already holds for it.
[[nodiscard]] std::int32_t handleOf(State& state, const std::shared_ptr<PluginWorldRef>& world);

/// kOk with `out` set, kErrInvalid for a malformed handle, kErrStale for a world that was unloaded.
[[nodiscard]] std::int32_t resolve(State& state, std::int32_t handle, std::shared_ptr<PluginWorldRef>& out);

[[nodiscard]] inline const Value* field(const Value& map, std::string_view key) { return map.find(key); }

[[nodiscard]] inline std::int64_t intField(const Value& map, std::string_view key, std::int64_t fallback = 0) {
    const Value* found = map.find(key);
    if (found == nullptr) {
        return fallback;
    }
    if (const std::int64_t* whole = found->asInt()) {
        return *whole;
    }
    if (const double* real = found->asFloat()) {
        return static_cast<std::int64_t>(*real);
    }
    return fallback;
}

[[nodiscard]] inline double realField(const Value& map, std::string_view key, double fallback = 0.0) {
    const Value* found = map.find(key);
    if (found == nullptr) {
        return fallback;
    }
    if (const double* real = found->asFloat()) {
        return *real;
    }
    if (const std::int64_t* whole = found->asInt()) {
        return static_cast<double>(*whole);
    }
    return fallback;
}

[[nodiscard]] inline bool boolField(const Value& map, std::string_view key, bool fallback = false) {
    const Value* found = map.find(key);
    const bool* flag = found == nullptr ? nullptr : found->asBool();
    return flag != nullptr ? *flag : fallback;
}

[[nodiscard]] inline std::string textField(const Value& map, std::string_view key, std::string_view fallback = {}) {
    const Value* found = map.find(key);
    const std::string* text = found == nullptr ? nullptr : found->asString();
    return text != nullptr ? *text : std::string{fallback};
}

[[nodiscard]] inline bool hasField(const Value& map, std::string_view key) { return map.find(key) != nullptr; }

inline void put(Value::Map& map, std::string key, Value value) {
    map.push_back(MapEntry{std::move(key), std::move(value)});
}

[[nodiscard]] inline Value intList(std::span<const std::int32_t> values) {
    Value::List list;
    list.reserve(values.size());
    for (const std::int32_t entry : values) {
        list.emplace_back(static_cast<std::int64_t>(entry));
    }
    return Value{std::move(list)};
}

[[nodiscard]] inline Value textList(const std::vector<std::string>& values) {
    Value::List list;
    list.reserve(values.size());
    for (const std::string& entry : values) {
        list.emplace_back(entry);
    }
    return Value{std::move(list)};
}

[[nodiscard]] inline Value bytesValue(std::span<const std::uint8_t> data) {
    return Value{wire::wasm::Blob{std::vector<std::uint8_t>(data.begin(), data.end())}};
}

/// `count` triples as an i32 count, or -1 when that is over the bulk cap.
[[nodiscard]] constexpr std::int32_t triple(std::int32_t count) noexcept {
    return count < 0 || count > kMaxBulk / 3 ? -1 : count * 3;
}

/// `count` pairs as an i32 count, or -1 when that is over the bulk cap.
[[nodiscard]] constexpr std::int32_t pairs(std::int32_t count) noexcept {
    return count < 0 || count > kMaxBulk / 2 ? -1 : count * 2;
}

/// Copies `count` i32 out of guest memory; nothing for a bad range or a count over the bulk cap.
[[nodiscard]] inline std::optional<std::vector<std::int32_t>> readInts(const abi::GuestMemory& memory, std::int32_t ptr,
                                                                       std::int32_t count) {
    if (count < 0 || count > kMaxBulk) {
        return std::nullopt;
    }
    if (count == 0) {
        return std::vector<std::int32_t>{};
    }
    const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(count) * 4U);
    if (!view) {
        return std::nullopt;
    }
    std::vector<std::int32_t> out(static_cast<std::size_t>(count));
    std::memcpy(out.data(), view->data(), view->size());
    return out;
}

/// The needed-length convention for i32 buffers: the count the full answer needs, written only when it fits `cap`.
[[nodiscard]] inline std::int32_t putInts(const abi::GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                                          std::span<const std::int32_t> data) {
    const auto needed = static_cast<std::int32_t>(data.size());
    if (cap < 0) {
        return abi::kErrInvalid;
    }
    if (needed > cap || needed == 0) {
        return needed;
    }
    const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(needed) * 4U);
    if (!view) {
        return abi::kErrInvalid;
    }
    std::memcpy(view->data(), data.data(), view->size());
    return needed;
}

/// A fixed-size i32 output the guest sized itself (a section of 4096); INVALID when it does not fit memory.
[[nodiscard]] inline std::int32_t writeInts(const abi::GuestMemory& memory, std::int32_t ptr,
                                            std::span<const std::int32_t> data) {
    const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(data.size()) * 4U);
    if (!view) {
        return abi::kErrInvalid;
    }
    std::memcpy(view->data(), data.data(), view->size());
    return static_cast<std::int32_t>(data.size());
}

[[nodiscard]] inline std::optional<std::vector<std::uint8_t>> readBytes(const abi::GuestMemory& memory,
                                                                        std::int32_t ptr, std::int32_t len) {
    if (len < 0 || len > static_cast<std::int32_t>(wire::wasm::kValueMaxBytes)) {
        return std::nullopt;
    }
    const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(len));
    if (!view) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(view->begin(), view->end());
}

[[nodiscard]] inline std::int32_t putBytes(const abi::GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                                           std::span<const std::uint8_t> data) {
    return memory.put(ptr, cap, wire::common::asText(data));
}

/// A map argument, or the error to answer with; an empty buffer is an empty map.
[[nodiscard]] inline std::expected<Value, std::int32_t> mapArgument(const abi::GuestMemory& memory, std::int32_t ptr,
                                                                    std::int32_t len) {
    if (len == 0) {
        return Value{Value::Map{}};
    }
    std::expected<Value, std::int32_t> value = readValue(memory, ptr, len);
    if (value && value->asMap() == nullptr) {
        return std::unexpected(abi::kErrInvalid);
    }
    return value;
}

/// Resolves the world handle at argument 0 and answers with what `fn(state, world)` returns.
template <typename Fn>
void withWorld(void* user, abi::Call& c, Fn&& fn) {
    State& state = stateOf(user);
    std::shared_ptr<PluginWorldRef> world;
    const std::int32_t status = resolve(state, c.i32(0), world);
    c.ret(status != abi::kOk ? status : fn(state, *world));
}

} // namespace basalt::server::plugins::wasm::worlds
