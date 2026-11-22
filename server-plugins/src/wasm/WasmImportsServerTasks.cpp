#include "WasmServerSupport.hpp"

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;

void taskWait(void* user, abi::Call& c) {
    State& state = stateOf(user);
    if (c.i32(0) < 1) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::int32_t request = beginRequest(state, c.i32(1));
    if (request == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    static_cast<void>(runAfter(state, c.i32(0), "wait",
                               [request](State& live) { static_cast<void>(completeRequest(live, request, Value{})); }));
    c.ret(request);
}

void taskAt(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> world = c.memory().string(c.i32(0), c.i32(1));
    if (!world || world->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (state.host->services().worlds().byName(*world) == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const std::int32_t request = beginRequest(state, c.i32(4));
    if (request == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    const auto inside = std::make_shared<bool>(true);
    const bool arranged = host->services().at(std::string{*world}, c.f64(2), c.f64(3), [host, link, request, inside] {
        const std::lock_guard<std::recursive_mutex> turn{host->turnMutex()};
        State* live = *link;
        if (live == nullptr) {
            return;
        }
        if (*inside) {
            static_cast<void>(runSoon(
                *live, "at", [request](State& later) { static_cast<void>(completeRequest(later, request, Value{})); }));
            return;
        }
        static_cast<void>(completeRequest(*live, request, Value{}));
    });
    *inside = false;
    if (!arranged) {
        static_cast<void>(requests(state).remove(request));
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(request);
}
} // namespace

void addServerTasksImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"task_wait", taskWait, user},
                              {"task_at", taskAt, user},
                          });
}
} // namespace basalt::server::plugins::wasm
