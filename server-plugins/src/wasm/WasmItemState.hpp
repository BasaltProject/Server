#pragma once

#include "WasmItemAccess.hpp"
#include "WasmValues.hpp"

#include <unordered_set>

namespace basalt::server::plugins::wasm::items {

struct ItemsState {
    InventoryTable inventories;
    std::unordered_set<std::string> knownItems;
    bool loaded = false;
};

[[nodiscard]] inline ItemsState& itemsOf(State& state) { return state.areaState<ItemsState>("items"); }

[[nodiscard]] inline bool knownItem(State& state, const std::string& id) {
    ItemsState& held = itemsOf(state);
    if (!held.loaded) {
        held.loaded = true;
        for (std::string& item : state.host->services().worlds().itemIds()) {
            held.knownItems.insert(std::move(item));
        }
    }
    return held.knownItems.empty() || held.knownItems.contains(id);
}

/// A value read out of guest memory as a stack, null being an empty slot; INVALID in the error.
[[nodiscard]] inline std::expected<Slot, std::int32_t> readStack(State& state, const abi::GuestMemory& memory,
                                                                 std::int32_t ptr, std::int32_t len) {
    const std::expected<Value, std::int32_t> value = readValue(memory, ptr, len);
    if (!value.has_value()) {
        return std::unexpected(value.error());
    }
    if (value->isNull()) {
        return Slot{};
    }
    std::optional<PluginItem> item =
        fromValue(*value, [&state](const std::string& id) { return knownItem(state, id); });
    if (!item.has_value()) {
        return std::unexpected(abi::kErrInvalid);
    }
    return Slot{std::move(*item)};
}

} // namespace basalt::server::plugins::wasm::items
