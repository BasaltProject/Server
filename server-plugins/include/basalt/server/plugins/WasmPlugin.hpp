#pragma once

#include <basalt/wire/wasm/Engine.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>

namespace basalt::server::plugins {
class LoadedPlugin;
class PluginHost;

namespace wasm {
class State;
}

/// A plugin written against the WebAssembly ABI: one module instance, its handle tables and its registrations.
class WasmPlugin {
public:
    /// Compiles (through `cacheDir` when it is not empty), links the ABI imports and instantiates; init has not run.
    [[nodiscard]] static std::expected<std::unique_ptr<WasmPlugin>, std::string>
    create(PluginHost& host, LoadedPlugin& plugin, const wire::wasm::Engine& engine,
           std::span<const std::uint8_t> module, const std::string& cacheDir);

    WasmPlugin(const WasmPlugin&) = delete;
    WasmPlugin& operator=(const WasmPlugin&) = delete;
    ~WasmPlugin();

    /// Runs basalt_init, then the plugin.enable listeners. False when the plugin failed and must not stay loaded.
    [[nodiscard]] bool start();

    /// Runs the plugin.disable listeners; the host retires the registrations afterwards.
    void stop();

    /// After this no callback reaches the guest, whatever thread a registration is called from.
    void close();

    /// Traps the guest call in progress, from any thread.
    void interrupt() noexcept;

private:
    explicit WasmPlugin(std::unique_ptr<wasm::State> state);

    std::unique_ptr<wasm::State> state_;
};
} // namespace basalt::server::plugins
