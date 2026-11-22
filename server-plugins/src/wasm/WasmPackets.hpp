#pragma once

#include "WasmState.hpp"
#include "WasmValues.hpp"

#include <basalt/server/plugins/PluginPackets.hpp>

#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::plugins::wasm::packets {

inline constexpr std::size_t kMaxBody = 1U << 20U;
inline constexpr std::chrono::milliseconds kBudget{5};
inline constexpr std::int32_t kMaxStrikes = 3;
inline constexpr std::string_view kArea = "packets";

/// What the running listener is looking at; lives on the stack of the dispatch and is reached through PacketsState.
struct Active {
    PluginPacket* packet = nullptr;
    const std::string* channel = nullptr;
    const std::string* playerUuid = nullptr;
    std::span<const std::uint8_t> bytes;
    bool observer = false;
};

/// One registered listener; `strikes` counts consecutive calls over the time budget.
struct ListenerSlot {
    PluginHandle handle;
    std::int32_t id = 0;
    std::int32_t strikes = 0;
    bool dead = false;
};

struct PacketsState {
    HandleTable<std::shared_ptr<ListenerSlot>> listeners;
    Active* active = nullptr;
};

[[nodiscard]] inline PacketsState& ownState(State& state) { return state.areaState<PacketsState>(kArea); }

/// The manifest declares it and the operator granted it.
[[nodiscard]] bool mayUsePackets(const State& state);
[[nodiscard]] bool mayUseChannel(const State& state, std::string_view channel);

/// kOk with `out` set for a live packet or channel context of this plugin; STALE or INVALID otherwise.
[[nodiscard]] std::int32_t activeOf(State& state, std::int32_t ctx, Active*& out);

[[nodiscard]] std::string_view phaseName(PacketPhase phase);

/// Runs `token` for one packet or message under the plugin's turn; true when the answers may be applied.
bool runListener(State& state, ListenerSlot& slot, std::int32_t token, Active& active, std::string_view why);

} // namespace basalt::server::plugins::wasm::packets
