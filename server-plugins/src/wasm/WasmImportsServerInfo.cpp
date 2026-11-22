#include "WasmServerSupport.hpp"

#include <basalt/wire/auth/MojangSessionService.hpp>

#include <thread>

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;

constexpr std::int32_t kToPlayers = 1;
constexpr std::int32_t kToConsole = 2;

void serverBroadcast(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> text = memory.string(c.i32(0), c.i32(1));
    const std::optional<ArgumentList> arguments = readArguments(memory, c.i32(2), c.i32(3));
    const std::int32_t flags = c.i32(4);
    if (!text || !arguments || (flags & (kToPlayers | kToConsole)) == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (!renderText(state.plugin->text, *text, arguments->view(), state.scratch)) {
        c.ret(abi::kErrAbsent);
        return;
    }
    std::int32_t reached = 0;
    if ((flags & kToPlayers) != 0) {
        for (const std::shared_ptr<PluginPlayerRef>& player : state.host->services().players().online()) {
            if (player->online()) {
                player->send(state.scratch);
                ++reached;
            }
        }
    }
    if ((flags & kToConsole) != 0) {
        state.plugin->log.info([&state] { return state.scratch; });
    }
    c.ret(reached);
}

void placeholderFill(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> text = memory.string(c.i32(0), c.i32(1));
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = c.i32(2) == 0 ? abi::kOk : state.resolvePlayer(c.i32(2), player);
    if (status != abi::kOk || !text) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(memory.put(c.i32(3), c.i32(4), state.host->services().placeholders().fill(*text, player.get())));
}

[[nodiscard]] std::int64_t numberSetting(PluginServerControl& control, std::string_view key) {
    const std::optional<std::string> text = control.setting(key);
    return text ? std::atoll(text->c_str()) : 0;
}

void serverInfo(void* user, abi::Call& c) {
    State& state = stateOf(user);
    PluginHost& host = *state.host;
    PluginServerControl& control = host.services().serverControl();
    const std::array<double, 4> pace = control.pace();
    Value::List plugins;
    for (const PluginStatus& status : host.statuses()) {
        plugins.push_back(mapOf({{"id", Value{status.id}},
                                 {"name", Value{status.name}},
                                 {"version", Value{status.version}},
                                 {"disabled", Value{status.disabled}}}));
    }
    Value::List authors;
    for (const std::string& author : state.plugin->manifest.authors) {
        authors.emplace_back(author);
    }
    const wire::plugins::PluginManifest& manifest = state.plugin->manifest;
    const auto setting = [&control](std::string_view key) { return control.setting(key).value_or(std::string{}); };
    c.ret(returnValue(c, 0,
                      mapOf({{"software", Value{"Basalt"}},
                             {"version", Value{setting("version")}},
                             {"minecraft", Value{setting("minecraft_version")}},
                             {"protocol", Value{numberSetting(control, "protocol_version")}},
                             {"tps_1m", Value{pace[0]}},
                             {"tps_5m", Value{pace[1]}},
                             {"tps_15m", Value{pace[2]}},
                             {"mean_tick_ms", Value{pace[3]}},
                             {"tick_rate", Value{static_cast<double>(control.tickRate())}},
                             {"frozen", Value{control.frozen()}},
                             {"paused", Value{control.paused()}},
                             {"online", Value{static_cast<std::int64_t>(host.services().players().online().size())}},
                             {"max_players", Value{numberSetting(control, "max_players")}},
                             {"plugins", Value{std::move(plugins)}},
                             {"plugin", mapOf({{"id", Value{manifest.id}},
                                               {"name", Value{manifest.name}},
                                               {"version", Value{manifest.version}},
                                               {"authors", Value{std::move(authors)}},
                                               {"lane", Value{0}},
                                               {"lanes", Value{1}},
                                               {"folder", Value{state.plugin->folder}}})}})));
}

struct Found {
    std::string uuid;
    std::string name;
    std::string texture;
    std::string signature;
};

struct Lookup {
    std::mutex mutex;
    bool ready = false;
    std::optional<Found> found;
};

[[nodiscard]] std::optional<Found> lookup(const std::string& name) {
    const std::optional<std::string> uuid = wire::auth::mojangUuidOf(name);
    if (!uuid) {
        return std::nullopt;
    }
    Found found{.uuid = *uuid, .name = name, .texture = {}, .signature = {}};
    if (const std::optional<wire::auth::SessionProfile> profile = wire::auth::mojangProfileOf(*uuid)) {
        found.name = profile->name;
        for (const wire::auth::SessionProfileProperty& property : profile->properties) {
            if (property.name == "textures") {
                found.texture = property.value;
                found.signature = property.signature.value_or("");
                break;
            }
        }
    }
    return found;
}

[[nodiscard]] Value profileValue(const std::optional<Found>& found) {
    if (!found) {
        return Value{};
    }
    return mapOf({{"uuid", Value{found->uuid}},
                  {"name", Value{found->name}},
                  {"texture", Value{found->texture}},
                  {"signature", Value{found->signature}}});
}

void profileLookup(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!name || name->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::int32_t request = beginRequest(state, c.i32(2));
    if (request == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const auto shared = std::make_shared<Lookup>();
    std::thread{[shared, asked = std::string{*name}] {
        std::optional<Found> found = lookup(asked);
        const std::lock_guard<std::mutex> lock{shared->mutex};
        shared->found = std::move(found);
        shared->ready = true;
    }}.detach();
    static_cast<void>(runEvery(state, 1, "profile_lookup(" + std::string{*name} + ")", [request, shared](State& live) {
        std::optional<Found> found;
        {
            const std::lock_guard<std::mutex> lock{shared->mutex};
            if (!shared->ready) {
                return false;
            }
            found = shared->found;
        }
        static_cast<void>(completeRequest(live, request, profileValue(found)));
        return true;
    }));
    c.ret(request);
}
} // namespace

void addServerInfoImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"server_broadcast", serverBroadcast, user},
                              {"placeholder_fill", placeholderFill, user},
                              {"server_info", serverInfo, user},
                              {"profile_lookup", profileLookup, user},
                          });
}
} // namespace basalt::server::plugins::wasm
