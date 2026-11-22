#pragma once

#include "WasmHandles.hpp"
#include "WasmState.hpp"

#include <basalt/wire/wasm/Memory.hpp>
#include <basalt/wire/wasm/Value.hpp>

#include <expected>
#include <mutex>

namespace basalt::server::plugins::wasm {

/// One value out of guest memory; the error is the ABI result (INVALID for a bad range, limit breach or bad bytes).
[[nodiscard]] inline std::expected<wire::wasm::Value, std::int32_t> readValue(const abi::GuestMemory& memory,
                                                                              std::int32_t ptr, std::int32_t len) {
    if (len < 0 || static_cast<std::size_t>(len) > wire::wasm::kValueMaxBytes) {
        return std::unexpected(abi::kErrInvalid);
    }
    const auto bytes = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(len));
    if (!bytes) {
        return std::unexpected(abi::kErrInvalid);
    }
    std::expected<wire::wasm::Value, std::string> value = wire::wasm::decodeValue(*bytes);
    if (!value) {
        return std::unexpected(abi::kErrInvalid);
    }
    return std::move(*value);
}

/// Writes the encoding with the needed-length convention; INVALID when the value breaks a limit.
[[nodiscard]] inline std::int32_t putValue(const abi::GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                                           const wire::wasm::Value& value) {
    std::string encoded;
    if (!wire::wasm::encodeValue(value, encoded)) {
        return abi::kErrInvalid;
    }
    return memory.put(ptr, cap, encoded);
}

/// Runs `token` with a completion context whose value_result answers `value`; false when it did not run.
inline bool deliverValue(State& state, std::int32_t token, const wire::wasm::Value& value) {
    const std::lock_guard<std::recursive_mutex> turn{state.host->turnMutex()};
    if (!state.host->onPluginThread()) {
        return false;
    }
    Context context;
    context.kind = Context::Kind::Completion;
    context.value = &value;
    return state.run(token, context, "completion");
}

/// Pending completions of a plugin by request handle; each holds the token to call back.
using RequestTable = HandleTable<std::int32_t>;

[[nodiscard]] inline RequestTable& requests(State& state) { return state.areaState<RequestTable>("core_values"); }

/// The request handle an area returns for a call that completes later; 0 when too many are pending.
[[nodiscard]] inline std::int32_t beginRequest(State& state, std::int32_t token) { return requests(state).add(token); }

/// Delivers `value` to the request's token; false when it was cancelled or already completed.
inline bool completeRequest(State& state, std::int32_t request, const wire::wasm::Value& value) {
    std::int32_t* token = nullptr;
    if (requests(state).find(request, token) != abi::kOk) {
        return false;
    }
    const std::int32_t target = *token;
    static_cast<void>(requests(state).remove(request));
    return deliverValue(state, target, value);
}

} // namespace basalt::server::plugins::wasm
