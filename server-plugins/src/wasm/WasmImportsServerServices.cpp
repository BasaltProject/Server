#include "WasmServerSupport.hpp"

#include <algorithm>

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;

constexpr std::int32_t kDefaultTimeoutTicks = 100;

struct Offer {
    std::string pluginId;
    std::shared_ptr<State*> link;
    std::int32_t token = 0;
};

/// Offers of every host in the process; a plugin reaches only the ones of its own host.
class Registry {
public:
    bool add(const PluginHost* host, const std::string& name, Offer offer) {
        const std::lock_guard<std::mutex> lock{mutex_};
        return hosts_[host].try_emplace(name, std::move(offer)).second;
    }

    void remove(const PluginHost* host, const std::string& name, const std::string& pluginId) {
        const std::lock_guard<std::mutex> lock{mutex_};
        auto offers = hosts_.find(host);
        if (offers == hosts_.end()) {
            return;
        }
        if (const auto found = offers->second.find(name);
            found != offers->second.end() && found->second.pluginId == pluginId) {
            offers->second.erase(found);
        }
        if (offers->second.empty()) {
            hosts_.erase(offers);
        }
    }

    [[nodiscard]] std::optional<Offer> find(const PluginHost* host, std::string_view name) {
        const std::lock_guard<std::mutex> lock{mutex_};
        const auto offers = hosts_.find(host);
        if (offers == hosts_.end()) {
            return std::nullopt;
        }
        const auto found = offers->second.find(name);
        return found == offers->second.end() ? std::nullopt : std::optional<Offer>{found->second};
    }

    [[nodiscard]] std::vector<std::string> names(const PluginHost* host) {
        const std::lock_guard<std::mutex> lock{mutex_};
        std::vector<std::string> out;
        if (const auto offers = hosts_.find(host); offers != hosts_.end()) {
            for (const auto& [name, offer] : offers->second) {
                out.push_back(name);
            }
        }
        return out;
    }

private:
    std::mutex mutex_;
    std::map<const PluginHost*, std::map<std::string, Offer, std::less<>>> hosts_;
};

[[nodiscard]] Registry& registry() {
    static Registry offers;
    return offers;
}

void serviceRegister(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!name || name->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::vector<std::string>& exported = state.plugin->manifest.exports.services;
    if (std::ranges::find(exported, *name) == exported.end()) {
        state.plugin->log.warn([&name] {
            return "service_register: " + std::string{*name} + " is not in [exports] services of plugin.toml";
        });
        c.ret(abi::kErrDenied);
        return;
    }
    PluginHost* host = state.host;
    const std::string id = state.plugin->manifest.id;
    if (!registry().add(host, std::string{*name}, Offer{id, state.link, c.i32(2)})) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHandle registered =
        keep(state, "service(" + std::string{*name} + ")",
             PluginHandle{[host, key = std::string{*name}, id] { registry().remove(host, key, id); }});
    const std::int32_t handle = area(state).services.add(std::move(registered));
    c.ret(handle != 0 ? handle : abi::kErrInvalid);
}

void serviceRemove(void* user, abi::Call& c) {
    Area& held = area(stateOf(user));
    PluginHandle* handle = nullptr;
    if (held.services.find(c.i32(0), handle) != abi::kOk) {
        c.ret(abi::kErrAbsent);
        return;
    }
    handle->remove();
    static_cast<void>(held.services.remove(c.i32(0)));
    c.ret(abi::kOk);
}

void serviceHas(void* user, abi::Call& c) {
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    c.ret(name ? (registry().find(stateOf(user).host, *name) ? 1 : 0) : abi::kErrInvalid);
}

void serviceList(void* user, abi::Call& c) {
    c.ret(returnValue(c, 0, listOfText(registry().names(stateOf(user).host))));
}

void serviceReply(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    std::int32_t status = state.check(c.i32(0), Context::Kind::Completion, context);
    if (status == abi::kOk &&
        (context->result == nullptr || context->value == nullptr || context->value->find("caller") == nullptr)) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::expected<Value, std::int32_t> value = readOptionalValue(c.memory(), c.i32(1), c.i32(2));
    std::string encoded;
    if (!value || !wire::wasm::encodeValue(*value, encoded)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    *context->result = std::move(encoded);
    c.ret(abi::kOk);
}

[[nodiscard]] Value failure(std::string_view error) {
    return mapOf({{"ok", Value{false}}, {"error", Value{std::string{error}}}});
}

[[nodiscard]] bool pending(State& state, std::int32_t request) {
    std::int32_t* token = nullptr;
    return requests(state).find(request, token) == abi::kOk;
}

void answer(State& caller, std::int32_t request, std::string_view name, Value args, Offer offer) {
    if (!pending(caller, request)) {
        return;
    }
    State* provider = *offer.link;
    if (provider == nullptr || provider->plugin->disabled) {
        static_cast<void>(completeRequest(caller, request, failure("unavailable")));
        return;
    }
    const Value call = mapOf(
        {{"name", Value{std::string{name}}}, {"caller", Value{caller.plugin->manifest.id}}, {"args", std::move(args)}});
    std::string reply;
    Context context;
    context.kind = Context::Kind::Completion;
    context.value = &call;
    context.result = &reply;
    if (!provider->run(offer.token, context, "service " + std::string{name})) {
        static_cast<void>(completeRequest(caller, request, failure("failed")));
        return;
    }
    Value result;
    if (!reply.empty()) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(reply.data());
        std::expected<Value, std::string> decoded = wire::wasm::decodeValue({bytes, reply.size()});
        if (decoded) {
            result = std::move(*decoded);
        }
    }
    static_cast<void>(completeRequest(caller, request, mapOf({{"ok", Value{true}}, {"value", std::move(result)}})));
}

void serviceCall(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    std::expected<Value, std::int32_t> args = readOptionalValue(memory, c.i32(2), c.i32(3));
    if (!name || name->empty() || !args) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::optional<Offer> offer = registry().find(state.host, *name);
    if (!offer) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const std::int32_t request = beginRequest(state, c.i32(5));
    if (request == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::int32_t ticks = c.i32(4) <= 0 ? kDefaultTimeoutTicks : c.i32(4);
    const auto timeout = std::make_shared<std::shared_ptr<PluginHandle>>();
    *timeout = runAfter(state, ticks, "service_call timeout", [request](State& live) {
        if (pending(live, request)) {
            static_cast<void>(completeRequest(live, request, failure("timeout")));
        }
    });
    static_cast<void>(runSoon(state, "service_call " + std::string{*name},
                              [request, timeout, key = std::string{*name}, args = std::move(*args),
                               offer = std::move(*offer)](State& live) mutable {
                                  answer(live, request, key, std::move(args), std::move(offer));
                                  if (*timeout != nullptr) {
                                      (*timeout)->remove();
                                  }
                              }));
    c.ret(request);
}
} // namespace

void addServerServicesImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"service_register", serviceRegister, user},
                              {"service_reply", serviceReply, user},
                              {"service_remove", serviceRemove, user},
                              {"service_has", serviceHas, user},
                              {"service_list", serviceList, user},
                              {"service_call", serviceCall, user},
                          });
}
} // namespace basalt::server::plugins::wasm
