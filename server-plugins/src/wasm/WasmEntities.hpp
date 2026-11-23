#pragma once

#include "WasmValues.hpp"

#include <basalt/server/plugins/PluginEntityOps.hpp>
#include <basalt/server/plugins/PluginPlayerAccess.hpp>
#include <basalt/server/plugins/PluginWorldRef.hpp>

#include <expected>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins::wasm::entities {

using wire::wasm::Value;

/// A live entity or player behind one handle; `key` is what the same entity answers when it is found again.
struct Target {
    std::shared_ptr<PluginEntityRef> entity;
    std::shared_ptr<PluginPlayerRef> player;
    std::string key;

    [[nodiscard]] bool alive() const;
    [[nodiscard]] bool isPlayer() const;
    [[nodiscard]] std::int32_t id() const;
    [[nodiscard]] std::string type() const;
    [[nodiscard]] std::string world() const;
    [[nodiscard]] std::string uuid() const;
    [[nodiscard]] std::array<double, 3> position() const;
};

/// The entity handles of one plugin. A handle dies with its entity, and the same entity keeps one handle.
class Table {
public:
    [[nodiscard]] std::int32_t handleFor(const std::shared_ptr<PluginEntityRef>& entity);
    [[nodiscard]] std::int32_t handleFor(const std::shared_ptr<PluginPlayerRef>& player);

    /// kOk with `out` set; STALE also drops a handle whose entity is gone.
    [[nodiscard]] std::int32_t resolve(std::int32_t handle, Target*& out);

    /// Whether the handle was live; its entity is untouched.
    bool release(std::int32_t handle);

private:
    [[nodiscard]] std::int32_t add(Target target);
    void sweep();

    HandleTable<Target> slots_;
    std::map<std::string, std::int32_t, std::less<>> byKey_;
};

[[nodiscard]] inline Table& table(State& state) { return state.areaState<Table>("entities"); }

[[nodiscard]] inline std::int32_t resolve(State& state, std::int32_t handle, Target*& out) {
    return table(state).resolve(handle, out);
}

/// The player a target is: its own ref, or the one behind the entity ref of a player; null for anything else.
[[nodiscard]] std::shared_ptr<PluginPlayerRef> playerOf(State& state, const Target& target);

/// The id with `minecraft:` in front when it has no namespace.
[[nodiscard]] std::string withNamespaceId(std::string_view id);

[[nodiscard]] std::shared_ptr<PluginWorldRef> worldNamed(State& state, std::string_view name);

/// A guest value as the server's entity value; the error is the ABI result (maps cannot cross).
[[nodiscard]] std::expected<PluginEntityValue, std::int32_t> toEntityValue(const Value& value);

/// The arguments of entity_call out of a value list (an empty buffer is no arguments).
[[nodiscard]] std::expected<std::vector<PluginEntityValue>, std::int32_t>
toArguments(const abi::GuestMemory& memory, std::int32_t ptr, std::int32_t len);

/// A server value as a guest value; `hint` is the property or operation name, which says how lists and rows read.
[[nodiscard]] Value fromEntityValue(std::string_view hint, const PluginEntityValue& value);

[[nodiscard]] Value pointValue(const std::array<double, 3>& point);

/// The operation names with the typed columns of their row results, for entity_operations.
[[nodiscard]] std::vector<std::string_view> columnsOf(std::string_view operation);

} // namespace basalt::server::plugins::wasm::entities
