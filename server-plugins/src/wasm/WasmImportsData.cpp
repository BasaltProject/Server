#include "WasmState.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <vector>

namespace basalt::server::plugins::wasm {
namespace {
constexpr std::int32_t kMaxTopRows = 1000;

[[nodiscard]] std::optional<std::string> stored(State& state, PluginPlayerRef& player, std::string_view key) {
    return state.host->services().storage().playerValue(state.plugin->manifest.id, player.uuid(), key);
}

void write(State& state, PluginPlayerRef& player, std::string_view key, const std::optional<std::string>& value) {
    state.host->services().storage().setPlayerValue(state.plugin->manifest.id, player.uuid(), key, value);
}

[[nodiscard]] std::optional<std::int64_t> integerOf(const std::optional<std::string>& value) {
    if (!value || value->size() < 2 || (*value)[0] != 'n') {
        return std::nullopt;
    }
    std::int64_t whole = 0;
    const char* end = value->data() + value->size();
    const auto [stop, error] = std::from_chars(value->data() + 1, end, whole);
    if (error != std::errc{} || stop != end) {
        return std::nullopt;
    }
    return whole;
}

[[nodiscard]] bool granted(const State& state) { return state.capable("players_data"); }

void dataGetI64(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> key = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int64_t result = c.i64(3);
    if (key && granted(state) && state.resolvePlayer(c.i32(0), player) == abi::kOk) {
        result = integerOf(stored(state, *player, *key)).value_or(result);
    }
    c.ret(result);
}

void dataGetBool(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> key = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t result = c.i32(3);
    if (key && granted(state) && state.resolvePlayer(c.i32(0), player) == abi::kOk) {
        const std::optional<std::string> value = stored(state, *player, *key);
        if (value && value->size() >= 2 && (*value)[0] == 'b') {
            result = (*value)[1] == '1' ? 1 : 0;
        }
    }
    c.ret(result);
}

void dataGetStr(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> key = memory.string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = key ? state.resolvePlayer(c.i32(0), player) : abi::kErrInvalid;
    if (status == abi::kOk && !granted(state)) {
        status = abi::kErrDenied;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<std::string> value = stored(state, *player, *key);
    if (!value || value->empty() || (*value)[0] != 's') {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(memory.put(c.i32(3), c.i32(4), std::string_view{*value}.substr(1)));
}

template <typename Encode>
void dataSet(void* user, abi::Call& c, Encode&& encode) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> key = memory.string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = key ? state.resolvePlayer(c.i32(0), player) : abi::kErrInvalid;
    std::optional<std::string> encoded;
    if (status == abi::kOk) {
        encoded = encode(memory);
        if (!encoded) {
            status = abi::kErrInvalid;
        } else if (!granted(state)) {
            status = abi::kErrDenied;
        }
    }
    if (status == abi::kOk) {
        write(state, *player, *key, encoded);
    }
    c.ret(status);
}

void dataSetI64(void* user, abi::Call& c) {
    dataSet(user, c, [&c](const abi::GuestMemory&) { return std::optional{"n" + std::to_string(c.i64(3))}; });
}

void dataSetBool(void* user, abi::Call& c) {
    dataSet(user, c, [&c](const abi::GuestMemory&) { return std::optional<std::string>{c.i32(3) != 0 ? "b1" : "b0"}; });
}

void dataSetStr(void* user, abi::Call& c) {
    dataSet(user, c, [&c](const abi::GuestMemory& memory) -> std::optional<std::string> {
        const std::optional<std::string_view> value = memory.string(c.i32(3), c.i32(4));
        if (!value) {
            return std::nullopt;
        }
        return "s" + std::string{*value};
    });
}

void dataRemove(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> key = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = key ? state.resolvePlayer(c.i32(0), player) : abi::kErrInvalid;
    if (status == abi::kOk) {
        if (!granted(state)) {
            status = abi::kErrDenied;
        } else if (!stored(state, *player, *key)) {
            status = abi::kErrAbsent;
        } else {
            write(state, *player, *key, std::nullopt);
        }
    }
    c.ret(status);
}

void storeTop(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> key = memory.string(c.i32(0), c.i32(1));
    if (!granted(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    if (!key || c.i32(2) <= 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::int32_t limit = std::min(c.i32(2), kMaxTopRows);
    std::string packed;
    for (const auto& [uuid, encoded] : state.host->services().storage().top(state.plugin->manifest.id, *key, limit)) {
        const std::optional<std::int64_t> value = integerOf(encoded);
        if (!value) {
            continue;
        }
        const std::shared_ptr<PluginPlayerRef> player = state.host->services().players().byUuid(uuid);
        const std::string name = player == nullptr ? uuid : player->name();
        const auto length = static_cast<std::uint32_t>(name.size());
        char raw[8];
        std::memcpy(raw, &length, 4);
        packed.append(raw, 4);
        packed += name;
        std::memcpy(raw, &*value, 8);
        packed.append(raw, 8);
    }
    c.ret(memory.put(c.i32(3), c.i32(4), packed));
}

void placeholderRegister(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    const std::int32_t token = c.i32(2);
    if (!name || name->empty() || std::ranges::find(state.placeholders, *name) != state.placeholders.end()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    PluginHandle handle = host->services().placeholders().registerPlaceholder(
        state.plugin->manifest.id, *name, [host, link, token](PluginPlayerRef* player) {
            std::string said;
            const std::lock_guard<std::recursive_mutex> turn{host->turnMutex()};
            State* live = *link;
            if (live == nullptr) {
                return said;
            }
            Context context;
            context.kind = Context::Kind::Placeholder;
            context.result = &said;
            context.player = player == nullptr ? 0 : live->playerHandle(player->shared_from_this());
            static_cast<void>(live->run(token, context, "placeholder"));
            return said;
        });
    state.placeholders.emplace_back(*name);
    static_cast<void>(
        state.plugin->registrations.add("placeholder(%" + state.plugin->manifest.id + "_" + std::string{*name} + "%)",
                                        [moved = std::move(handle)]() mutable { moved.remove(); }));
    c.ret(abi::kOk);
}

void placeholderPlayer(void* user, abi::Call& c) {
    Context* context = nullptr;
    const std::int32_t status = stateOf(user).check(c.i32(0), Context::Kind::Placeholder, context);
    c.ret(status != abi::kOk ? status : context->player);
}

void placeholderResult(void* user, abi::Call& c) {
    const std::optional<std::string_view> text = c.memory().string(c.i32(1), c.i32(2));
    Context* context = nullptr;
    std::int32_t status = stateOf(user).check(c.i32(0), Context::Kind::Placeholder, context);
    if (status == abi::kOk) {
        if (text && context->result != nullptr) {
            context->result->assign(*text);
        } else {
            status = abi::kErrInvalid;
        }
    }
    c.ret(status);
}
} // namespace

void addDataImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"data_get_i64", dataGetI64, user},
                              {"data_set_i64", dataSetI64, user},
                              {"data_get_bool", dataGetBool, user},
                              {"data_set_bool", dataSetBool, user},
                              {"data_get_str", dataGetStr, user},
                              {"data_set_str", dataSetStr, user},
                              {"data_remove", dataRemove, user},
                              {"store_top", storeTop, user},
                              {"placeholder_register", placeholderRegister, user},
                              {"placeholder_player", placeholderPlayer, user},
                              {"placeholder_result", placeholderResult, user},
                          });
}
} // namespace basalt::server::plugins::wasm
