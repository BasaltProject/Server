#include "WasmHud.hpp"

namespace basalt::server::plugins::wasm {
namespace hud {
namespace {
[[nodiscard]] PluginBossBars& service(State& state) { return state.host->services().bossBars(); }

[[nodiscard]] std::int32_t barOf(State& state, std::int32_t handle, Bar*& out) {
    return stateOfHud(state).bars.find(handle, out);
}

[[nodiscard]] std::int32_t fillBar(State& state, const Value& spec, PluginBossBar& bar) {
    std::optional<std::string> text;
    std::optional<bool> flag;
    std::optional<double> number;
    std::int32_t status = takeText(state, spec, "title", text, "mini:");
    bar.title = text.value_or(bar.title);
    std::optional<std::string> word;
    status = status != abi::kOk ? status : takeString(spec, "color", word);
    bar.color = word.value_or(bar.color);
    word.reset();
    status = status != abi::kOk ? status : takeString(spec, "style", word);
    bar.style = word.value_or(bar.style);
    status = status != abi::kOk ? status : takeNumber(spec, "progress", number);
    bar.progress = number.value_or(bar.progress);
    status = status != abi::kOk ? status : takeBool(spec, "darken_sky", flag);
    bar.darkenSky = flag.value_or(bar.darkenSky);
    flag.reset();
    status = status != abi::kOk ? status : takeBool(spec, "boss_music", flag);
    bar.bossMusic = flag.value_or(bar.bossMusic);
    flag.reset();
    status = status != abi::kOk ? status : takeBool(spec, "fog", flag);
    bar.fog = flag.value_or(bar.fog);
    flag.reset();
    status = status != abi::kOk ? status : takeBool(spec, "visible", flag);
    bar.visible = flag.value_or(bar.visible);
    return status;
}

void bossbarCreate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(0), c.i32(1));
    if (!spec) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginBossBar bar;
    bar.title = "\"\"";
    std::optional<std::string> key;
    std::int32_t status = fillBar(state, *spec, bar);
    status = status != abi::kOk ? status : takeString(*spec, "key", key);
    if (status != abi::kOk) {
        c.ret(status == abi::kErrAbsent ? status : abi::kErrInvalid);
        return;
    }
    if (key) {
        bar.id = *key;
        bar.keyed = true;
    }
    const std::optional<std::string> made = service(state).create(bar);
    HudState& hud = stateOfHud(state);
    const std::int32_t handle = made ? hud.bars.add(Bar{*made, {}}) : 0;
    if (handle == 0) {
        if (made) {
            static_cast<void>(service(state).remove(*made));
        }
        c.ret(abi::kErrInvalid);
        return;
    }
    if (!bar.keyed) {
        PluginHost* host = state.host;
        Bar* held = nullptr;
        static_cast<void>(hud.bars.find(handle, held));
        held->registration = state.plugin->registrations.add(
            "boss bar " + *made, [host, id = *made] { static_cast<void>(host->services().bossBars().remove(id)); });
    }
    c.ret(handle);
}

void bossbarAttach(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> id = c.memory().string(c.i32(0), c.i32(1));
    if (!id || id->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (!service(state).get(*id).has_value()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const std::int32_t handle = stateOfHud(state).bars.add(Bar{std::string{*id}, {}});
    c.ret(handle != 0 ? handle : abi::kErrInvalid);
}

void bossbarUpdate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Bar* held = nullptr;
    std::int32_t status = barOf(state, c.i32(0), held);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    if (status == abi::kOk && !spec) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        std::optional<PluginBossBar> bar = service(state).get(held->id);
        if (!bar) {
            status = abi::kErrStale;
        } else {
            status = fillBar(state, *spec, *bar);
            if (status == abi::kOk && !service(state).update(*bar)) {
                status = abi::kErrStale;
            }
        }
    }
    c.ret(status == abi::kErrAbsent ? abi::kErrInvalid : status);
}

void bossbarRemove(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HudState& hud = stateOfHud(state);
    Bar* held = nullptr;
    const std::int32_t status = barOf(state, c.i32(0), held);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::string id = held->id;
    PluginHandle registration = held->registration;
    static_cast<void>(hud.bars.remove(c.i32(0)));
    registration.remove();
    static_cast<void>(service(state).remove(id));
    c.ret(abi::kOk);
}

void bossbarPlayer(void* user, abi::Call& c, bool add) {
    State& state = stateOf(user);
    Bar* held = nullptr;
    std::int32_t status = barOf(state, c.i32(0), held);
    std::shared_ptr<PluginPlayerRef> player;
    if (status == abi::kOk) {
        status = state.resolvePlayer(c.i32(1), player);
    }
    if (status == abi::kOk) {
        const bool done = add ? service(state).addPlayer(held->id, player->uuid())
                              : service(state).removePlayer(held->id, player->uuid());
        status = done || service(state).get(held->id).has_value() ? abi::kOk : abi::kErrStale;
    }
    c.ret(status);
}

void bossbarPlayerAdd(void* user, abi::Call& c) { bossbarPlayer(user, c, true); }

void bossbarPlayerRemove(void* user, abi::Call& c) { bossbarPlayer(user, c, false); }

void bossbarPlayers(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Bar* held = nullptr;
    const std::int32_t status = barOf(state, c.i32(0), held);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), stringList(service(state).players(held->id))));
}

void bossbarGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Bar* held = nullptr;
    const std::int32_t status = barOf(state, c.i32(0), held);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginBossBar> bar = service(state).get(held->id);
    if (!bar) {
        c.ret(abi::kErrStale);
        return;
    }
    Value::Map map;
    map.push_back({"id", Value{bar->id}});
    map.push_back({"key", bar->keyed ? Value{bar->id} : Value{}});
    map.push_back({"title", Value{bar->title}});
    map.push_back({"color", Value{bar->color}});
    map.push_back({"style", Value{bar->style}});
    map.push_back({"progress", Value{bar->progress}});
    map.push_back({"darken_sky", Value{bar->darkenSky}});
    map.push_back({"boss_music", Value{bar->bossMusic}});
    map.push_back({"fog", Value{bar->fog}});
    map.push_back({"visible", Value{bar->visible}});
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void bossbarKept(void* user, abi::Call& c) {
    c.ret(putValue(c.memory(), c.i32(0), c.i32(1), stringList(service(stateOf(user)).kept())));
}
} // namespace
} // namespace hud

void addHudExtraImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"hud_bossbar_create", hud::bossbarCreate, user},
                              {"hud_bossbar_attach", hud::bossbarAttach, user},
                              {"hud_bossbar_update", hud::bossbarUpdate, user},
                              {"hud_bossbar_remove", hud::bossbarRemove, user},
                              {"hud_bossbar_player_add", hud::bossbarPlayerAdd, user},
                              {"hud_bossbar_player_remove", hud::bossbarPlayerRemove, user},
                              {"hud_bossbar_players", hud::bossbarPlayers, user},
                              {"hud_bossbar_get", hud::bossbarGet, user},
                              {"hud_bossbar_kept", hud::bossbarKept, user},
                          });
}
} // namespace basalt::server::plugins::wasm
