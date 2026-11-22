#include "WasmValues.hpp"

namespace basalt::server::plugins::wasm {
namespace {
void valueResult(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Completion, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), *context->value));
}

void requestCancel(void* user, abi::Call& c) {
    c.ret(requests(stateOf(user)).remove(c.i32(0)) ? abi::kOk : abi::kErrAbsent);
}
} // namespace

void addCoreValuesImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"value_result", valueResult, user},
                              {"request_cancel", requestCancel, user},
                          });
}
} // namespace basalt::server::plugins::wasm
