#include "WasmState.hpp"

#include <cstring>
#include <vector>

namespace basalt::server::plugins::wasm {
namespace {
void playerByName(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    const std::shared_ptr<PluginPlayerRef> player = name ? state.host->services().players().byName(*name) : nullptr;
    c.ret(player == nullptr ? 0 : state.playerHandle(player));
}

void playersOnline(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::int32_t cap = c.i32(1);
    if (cap < 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::vector<std::int32_t> handles;
    for (const std::shared_ptr<PluginPlayerRef>& player : state.host->services().players().online()) {
        if (const std::int32_t handle = player->online() ? state.playerHandle(player) : 0; handle > 0) {
            handles.push_back(handle);
        }
    }
    const auto count = static_cast<std::int32_t>(handles.size());
    if (count > cap) {
        c.ret(count);
        return;
    }
    const auto view = memory.range(static_cast<std::uint32_t>(c.i32(0)), static_cast<std::uint32_t>(count) * 4U);
    if (!view) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (count > 0) {
        std::memcpy(view->data(), handles.data(), view->size());
    }
    c.ret(count);
}

void playerName(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : c.memory().put(c.i32(1), c.i32(2), player->name()));
}

enum class Channel : std::uint8_t { Chat, ActionBar };

void deliver(void* user, abi::Call& c, Channel channel) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> text = memory.string(c.i32(1), c.i32(2));
    const std::optional<ArgumentList> arguments = readArguments(memory, c.i32(3), c.i32(4));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!text || !arguments)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        if (renderText(state.plugin->text, *text, arguments->view(), state.scratch)) {
            if (channel == Channel::Chat) {
                player->send(state.scratch);
            } else {
                player->actionBar(state.scratch);
            }
        } else {
            status = abi::kErrAbsent;
        }
    }
    c.ret(status);
}

void playerSend(void* user, abi::Call& c) { deliver(user, c, Channel::Chat); }

void playerActionBar(void* user, abi::Call& c) { deliver(user, c, Channel::ActionBar); }

void playerParticle(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> id = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!id || id->empty() || c.i32(3) < 0)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        player->particle(*id, player->x(), player->y(), player->z(), c.i32(3), c.f64(4));
    }
    c.ret(status);
}

void playerSound(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> id = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!id || id->empty())) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        player->sound(*id, c.f32(3), c.f32(4));
    }
    c.ret(status);
}
} // namespace

void addPlayersImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"player_by_name", playerByName, user},
                              {"players_online", playersOnline, user},
                              {"player_name", playerName, user},
                              {"player_send", playerSend, user},
                              {"player_action_bar", playerActionBar, user},
                              {"player_particle", playerParticle, user},
                              {"player_sound", playerSound, user},
                          });
}
} // namespace basalt::server::plugins::wasm
